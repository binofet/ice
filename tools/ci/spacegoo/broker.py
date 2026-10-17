#!/usr/bin/python3 -I
"""Install root-owned; never run a checkout copy as root. No provider credentials."""

from pathlib import Path
import fcntl
import hashlib
import json
import os
import pwd
import signal
import stat
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from contract import INSTALL, JOB, POLICY, admission, bounded_json, digest, need, policy, trusted, unit_command, verify_report


def closed(unit):
    result = subprocess.run(["/usr/bin/systemctl", "show", unit, "--property=ActiveState,MainPID"],
                            capture_output=True, text=True, timeout=10)
    need(result.returncode in (0, 4) and len(result.stdout) < 4096, "Cannot inspect source unit closure")
    state = dict(line.split("=", 1) for line in result.stdout.splitlines() if "=" in line)
    need(state.get("ActiveState") in {"inactive", "failed"} and state.get("MainPID") == "0",
         "Source unit remains active")
    group = Path("/sys/fs/cgroup/system.slice") / unit
    if group.exists():
        count = 0
        for path, dirs, _ in os.walk(group):
            count += 1
            need(count <= 128 and not Path(path).is_symlink()
                 and all(not (Path(path) / d).is_symlink() for d in dirs), "Unsafe cgroup tree")
            need(not (Path(path) / "cgroup.procs").read_text().strip(), "Source descendants remain")


def source_report(path, owner):
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    with os.fdopen(fd, "rb") as stream:
        before = os.fstat(stream.fileno())
        need(stat.S_ISREG(before.st_mode) and before.st_uid == owner and before.st_nlink == 1
             and before.st_size <= 1024 * 1024, "Unsafe source report")
        raw = stream.read(1024 * 1024 + 1)
        after = os.fstat(stream.fileno())
        need(len(raw) == before.st_size and (before.st_ino, before.st_size, before.st_mtime_ns)
             == (after.st_ino, after.st_size, after.st_mtime_ns), "Source report changed")
        return raw


def put(path, raw, group):
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o640)
    with os.fdopen(fd, "wb") as stream:
        os.fchown(stream.fileno(), 0, group)
        stream.write(raw)
        stream.flush()
        os.fsync(stream.fileno())


def interrupted(_signal, _frame):
    raise InterruptedError("Controller interrupted; stopping its source unit")


def main(argv):
    lock = None
    try:
        need(os.geteuid() == 0 and Path(__file__).resolve() == INSTALL / "broker.py",
             "Only the reviewed installed root broker may run")
        need(len(argv) == 2, "Expected repository and admitted job ID")
        repo, job_id = argv
        need(JOB.fullmatch(job_id), "Invalid admitted job ID")
        config = policy(bounded_json(trusted(POLICY)))
        need(config["enabled"], "CPU runner is not commissioned")
        need(repo in config["repos"], "Unknown repository")
        need(os.environ.get("SUDO_USER") == config["repos"][repo]["control_user"],
             "Control identity cannot select this repository")
        # Resolve the account now, never allow systemd to choose an implicit identity.
        account = pwd.getpwnam(config["repos"][repo]["job_user"])
        control = pwd.getpwnam(config["repos"][repo]["control_user"])
        need(account.pw_uid != 0, "Non-root job required")
        trusted(INSTALL, directory=True)
        for name in ("contract.py", "job.py"):
            trusted(INSTALL / name)
        root = Path(config["root"])
        path = trusted(root / "intake" / repo / "admissions" / (job_id + ".json"))
        request = admission(bounded_json(path, 65536), config, repo, job_id, digest(POLICY))
        # Hold root's NEW family lock through unit closure and sealing, so another
        # source job with the same UID cannot race the private report collection.
        lock_path = trusted(root / "locks/cpu.lock")
        lock = lock_path.open("r+")
        fcntl.flock(lock.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        unit = "ice-family-ci-" + repo + "-" + job_id + ".service"
        signal.signal(signal.SIGTERM, interrupted)
        signal.signal(signal.SIGINT, interrupted)
        try:
            result = subprocess.run(unit_command(config, repo, job_id), stdin=subprocess.DEVNULL,
                                    env={"PATH": "/usr/bin:/bin", "LANG": "C.UTF-8"}, check=False,
                                    timeout=config["limits"]["runtime_seconds"] + 30)
        finally:
            subprocess.run(["/usr/bin/systemctl", "stop", unit], stdin=subprocess.DEVNULL,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=20)
            closed(unit)
        report_path = root / "jobs" / repo / job_id / "results/report.json"
        passed = False
        diagnostic = "Source unit exited " + str(result.returncode)
        try:
            raw = source_report(report_path, account.pw_uid)
            report = bounded_json(report_path)
            if result.returncode == 0:
                verify_report(config, request, report)
                passed = True
                diagnostic = None
        except (ValueError, OSError, KeyError, TypeError) as error:
            diagnostic = "Report collection refused: " + str(error)[:1000]
            raw = (json.dumps({"format": "ice-family-cpu-report-v1", "status": "failed",
                               "admission": request, "diagnostic": diagnostic}) + "\n").encode()
        receipt_root = trusted(root / "receipts" / repo, directory=True)
        put(receipt_root / (job_id + ".report.json"), raw, control.pw_gid)
        receipt = {"format": "ice-family-closed-cpu-result-v1", "admission": request,
                   "status": "passed" if passed else "failed", "diagnostic": diagnostic,
                   "source_exit_code": result.returncode,
                   "source_unit_closed": True, "report_sha256": hashlib.sha256(raw).hexdigest(),
                   "report": str(receipt_root / (job_id + ".report.json")), "merge_performed": False}
        put(receipt_root / (job_id + ".json"), (json.dumps(receipt, sort_keys=True) + "\n").encode(), control.pw_gid)
        print(json.dumps(receipt))
        return 0 if passed else 1
    except (ValueError, OSError, KeyError, TypeError, subprocess.TimeoutExpired) as error:
        print("CPU broker refused: " + str(error)[:1000], file=sys.stderr)
        return 64
    finally:
        if lock is not None:
            lock.close()


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
