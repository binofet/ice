#!/usr/bin/env python3
"""Run one CI stage with a real exit result, deadline and bounded evidence."""

import argparse
from collections import deque
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import selectors
import signal
import subprocess
import sys
import threading
import time

LOG_LIMIT = 8 * 1024 * 1024
CONSOLE_LIMIT = 1024 * 1024
CANCEL_GRACE_SECONDS = 1.0


def signal_child(state, number):
    process = state.get('process')
    if process is None or state.get('group_closed'):
        return
    try:
        if os.name == 'nt':
            if number == signal.SIGTERM:
                process.terminate()
            else:
                process.send_signal(number)
        else:
            os.killpg(process.pid, number)
    except ProcessLookupError:
        pass


def kill_child(state):
    process = state.get('process')
    if process is None or state.get('group_closed'):
        return
    try:
        if os.name == 'nt':
            process.kill()
        else:
            os.killpg(process.pid, signal.SIGKILL)
        state['cleanup_escalated'] = True
    except ProcessLookupError:
        pass


def write_json(path, value):
    pending = path.with_suffix(path.suffix + ".pending")
    pending.write_text(json.dumps(value, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    pending.replace(path)


def run(stage, command, output, timeout_seconds):
    state = {'signal': None, 'cancel_at': None, 'process': None, 'group_closed': False}
    previous = {}

    def cancel(number, _frame):
        # No logging, locks, waits or thread creation in the signal handler.
        if state['signal'] is None:
            state['signal'] = number
            state['cancel_at'] = time.monotonic()
        signal_child(state, number)

    try:
        if threading.current_thread() is threading.main_thread():
            for number in (signal.SIGINT, signal.SIGTERM):
                previous[number] = signal.signal(number, cancel)
        return run_owned(stage, command, output, timeout_seconds, state)
    finally:
        for number, handler in previous.items():
            signal.signal(number, handler)


def run_owned(stage, command, output, timeout_seconds, state):
    if not stage or not all(c.isascii() and (c.isalnum() or c == "-") for c in stage):
        raise ValueError("stage must contain ASCII letters, numbers or hyphens")
    if not command or not math.isfinite(timeout_seconds) or timeout_seconds <= 0:
        raise ValueError("provide a command and a positive deadline")
    output.mkdir(parents=True, exist_ok=True)
    if (output / f"{stage}.json").exists():
        raise ValueError(f"stage {stage!r} already has evidence; use a fresh attempt directory")
    started = datetime.now(timezone.utc).isoformat()
    clock = time.monotonic()
    chunks, retained, total = deque(), 0, 0
    console_open = True
    timed_out = threading.Event()
    finished = threading.Event()
    terminated = threading.Event()

    def emit(block):
        nonlocal console_open
        if console_open:
            try:
                sys.stdout.buffer.write(block)
                sys.stdout.buffer.flush()
            except OSError:
                console_open = False
                # Runner cancellation can close its pipe before our finalizer.
                # Keep the private log and avoid another failed flush at exit.
                try:
                    with open(os.devnull, 'wb') as sink:
                        os.dup2(sink.fileno(), sys.stdout.fileno())
                except OSError:
                    pass

    def retain(block):
        nonlocal retained, total
        if total < CONSOLE_LIMIT:
            emit(block[:CONSOLE_LIMIT - total])
        chunks.append(block)
        retained += len(block)
        total += len(block)
        while retained > LOG_LIMIT:
            excess = retained - LOG_LIMIT
            first = chunks.popleft()
            if len(first) > excess:
                chunks.appendleft(first[excess:])
                retained -= excess
            else:
                retained -= len(first)

    emit(("+ " + " ".join(command) + "\n").encode('utf-8', errors='replace'))
    watchdog = None
    process = None
    try:
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   start_new_session=os.name != "nt")
        state['process'] = process
        if state['signal'] is not None:
            signal_child(state, state['signal'])

        def supervise():
            while not finished.wait(.02):
                now = time.monotonic()
                if now - clock >= timeout_seconds:
                    timed_out.set()
                    kill_child(state)
                    terminated.set()
                    return
                if state['cancel_at'] is not None and now - state['cancel_at'] >= CANCEL_GRACE_SECONDS:
                    kill_child(state)
                    terminated.set()
                    return

        watchdog = threading.Thread(target=supervise, name='stage-deadline')
        watchdog.start()
        if os.name == 'nt':
            while block := process.stdout.read1(65536):
                retain(block)
        else:
            # Poll the pipe so a canceled owner cannot hang on a retained FD.
            os.set_blocking(process.stdout.fileno(), False)
            with selectors.DefaultSelector() as reader:
                reader.register(process.stdout, selectors.EVENT_READ)
                while True:
                    ready = reader.select(.05)
                    if ready:
                        block = os.read(process.stdout.fileno(), 65536)
                        if not block:
                            break
                        retain(block)
                    elif terminated.is_set():
                        break
        raw_result = process.wait()
        # The direct child may trap cancellation and exit 0 while descendants
        # close stdout and keep running. Keep the watchdog until group cleanup.
        if state['signal'] is not None:
            if os.name != 'nt':
                try:
                    os.killpg(process.pid, 0)
                except ProcessLookupError:
                    terminated.set()
                    state['group_closed'] = True
            if not state['group_closed']:
                terminated.wait(CANCEL_GRACE_SECONDS + .1)
                if not terminated.is_set():
                    kill_child(state)
        process.stdout.close()
    except OSError as error:
        retain((str(error) + "\n").encode("utf-8", errors="replace"))
        if process is not None:
            kill_child(state)
            process.wait()
        raw_result = 127
    finally:
        state['group_closed'] = True
        finished.set()
        if watchdog:
            watchdog.join()
    result = (128 + state['signal'] if state['signal'] is not None else
              124 if timed_out.is_set() else raw_result if raw_result >= 0 else 128 - raw_result)
    retained_log = b"".join(chunks)
    if total > LOG_LIMIT:
        retained_log = b"[Log truncated: retaining the last 8 MiB.]\n" + retained_log
    (output / f"{stage}.log").write_bytes(retained_log)
    if total > CONSOLE_LIMIT:
        emit(b"\n[Console capped at 1 MiB; final 4 KiB follows. Download the stage log.]\n")
        emit(retained_log[-4096:])
    record = {"stage": stage, "command": command, "started_at": started,
              "duration_seconds": round(time.monotonic() - clock, 6), "exit_code": result,
              "process_exit_code": raw_result, "timeout_seconds": timeout_seconds,
              "timed_out": timed_out.is_set(), "output_bytes": total,
              "log_truncated": total > LOG_LIMIT}
    if state['signal'] is not None:
        record.update(cancelled=True, cancellation_signal=state['signal'],
                      cancellation_grace_seconds=CANCEL_GRACE_SECONDS,
                      cleanup_escalated=state.get('cleanup_escalated', False),
                      process_id=process.pid if process else None)
    if not console_open:
        record['console_output_failed'] = True
    write_json(output / f"{stage}.json", record)
    emit(f"\n{stage}: exit {result}, {record['duration_seconds']:.3f}s\n".encode())
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--timeout-seconds", type=float, default=1800)
    parser.add_argument("stage")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    try:
        return run(args.stage, command, Path(os.environ.get("CI_EVIDENCE_DIR",
                   "build/ci-evidence")), args.timeout_seconds)
    except (OSError, ValueError) as error:
        parser.exit(2, f"CI stage: {error}\n")


if __name__ == "__main__":
    sys.exit(main())
