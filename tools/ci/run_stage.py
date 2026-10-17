#!/usr/bin/env python3
"""Run one CI stage with a real exit result, deadline and bounded evidence."""

import argparse
from collections import deque
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import signal
import subprocess
import sys
import threading
import time

LOG_LIMIT = 8 * 1024 * 1024
CONSOLE_LIMIT = 1024 * 1024


def write_json(path, value):
    pending = path.with_suffix(path.suffix + ".pending")
    pending.write_text(json.dumps(value, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    pending.replace(path)


def run(stage, command, output, timeout_seconds):
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
    timed_out = threading.Event()
    finished = threading.Event()

    def retain(block):
        nonlocal retained, total
        if total < CONSOLE_LIMIT:
            sys.stdout.buffer.write(block[:CONSOLE_LIMIT - total])
            sys.stdout.buffer.flush()
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

    print("+ " + " ".join(command), flush=True)
    timer = None
    try:
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   start_new_session=os.name != "nt")

        def expire():
            if not finished.is_set():
                timed_out.set()
                try:
                    if os.name == "nt":
                        process.kill()
                    else:
                        os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass

        timer = threading.Timer(timeout_seconds, expire)
        timer.daemon = True
        timer.start()
        while block := process.stdout.read1(65536):
            retain(block)
        raw_result = process.wait()
        finished.set()
        process.stdout.close()
    except OSError as error:
        retain((str(error) + "\n").encode("utf-8", errors="replace"))
        raw_result = 127
    finally:
        finished.set()
        if timer:
            timer.cancel()
    result = 124 if timed_out.is_set() else (raw_result if raw_result >= 0 else 128 - raw_result)
    retained_log = b"".join(chunks)
    if total > LOG_LIMIT:
        retained_log = b"[Log truncated: retaining the last 8 MiB.]\n" + retained_log
    (output / f"{stage}.log").write_bytes(retained_log)
    if total > CONSOLE_LIMIT:
        print("\n[Console capped at 1 MiB; final 4 KiB follows. Download the stage log.]",
              flush=True)
        sys.stdout.buffer.write(retained_log[-4096:])
        sys.stdout.buffer.flush()
    record = {"stage": stage, "command": command, "started_at": started,
              "duration_seconds": round(time.monotonic() - clock, 6), "exit_code": result,
              "process_exit_code": raw_result, "timeout_seconds": timeout_seconds,
              "timed_out": timed_out.is_set(), "output_bytes": total,
              "log_truncated": total > LOG_LIMIT}
    write_json(output / f"{stage}.json", record)
    print(f"\n{stage}: exit {result}, {record['duration_seconds']:.3f}s", flush=True)
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
