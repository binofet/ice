#!/usr/bin/env python3
"""Owner-run, one-command wrapper for the already-reviewed disabled Spacegoo setup.

Run as jmax over interactive SSH, never as root. Only literal system commands seal
the fixed packet before its first privileged Python execution. This grants no
delegation, starts no services and changes no provider or spending configuration.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


SOURCE = "84e93ccdc13efff2769d753ef268255db3871cec"
HANDOFF = Path("/home/jmax/dev/ice/build/worktrees/spacegoo-disabled-bootstrap-84e93cc/build/owner-handoff-84e93cc")
PACKET = HANDOFF / "review-packet"
SEALED = Path("/var/lib/ice-family-bootstrap-84e93cc")
MANIFEST = "fdd1420f2e614da287617c33d8ecdb7969fdbbc1ea558ea281ab2a9dd5ed1a24"
PLAN = "c8b61abaf8b21a52e60786a02f31b7fa902728d4799c0ab218682109838ba851"
HASHES = {
    "bootstrap.py": "4238b47ce5a956a51e3c55fcda723afd29d422912142f013e117a8843ed60f2d",
    "manifest.json": MANIFEST,
    "payload/contract.py": "9182a8afda5510c633184ea4e62634c7e090b52a7e9e6173bf67e310f24f755e",
    "payload/broker.py": "cea01e213c718b26bbbbefea39ce19b02bf85f87b2ede42b2350153b98744da5",
    "payload/job.py": "9cf6c03a20519b0f4f8643d14d44c7c9b9780533236ef39572325cf850a91fd5",
    "payload/unittest_xml.py": "12097cb0c6cac036c349c4f516003fcc504c56d14e7003210c1a76cd6bee2331",
    "payload/admit.py": "10c2037d567cae2db09187b8f65b0a7a8b891cd3a06a0b3ecbcf749d50c05910",
    "payload/policy.json": "836f527971ee24911af92b78c5c01868cbe55cf82b145a0a4367dfae9dd80072",
}


def need(condition, message):
    if not condition:
        raise ValueError(message)


def seal_commands():
    """Fixed equivalent of section one of the independently reviewed owner handoff."""
    return """set -eu
src=/home/jmax/dev/ice/build/worktrees/spacegoo-disabled-bootstrap-84e93cc/build/owner-handoff-84e93cc/review-packet
dst=/var/lib/ice-family-bootstrap-84e93cc
for ancestor in / /var /var/lib; do
    test ! -L "$ancestor"
    test "$(/usr/bin/stat -c '%F:%u:%g:%a' "$ancestor")" = 'directory:0:0:755'
done
test ! -e "$dst"
test ! -L "$dst"
/usr/bin/install -d -o 0 -g 0 -m 0700 -- "$dst" "$dst/payload"
for file in bootstrap.py manifest.json payload/contract.py payload/broker.py payload/job.py payload/unittest_xml.py payload/admit.py payload/policy.json; do
    /usr/bin/install -o 0 -g 0 -m 0400 -- "$src/$file" "$dst/$file"
done
cd "$dst"
/usr/bin/sha256sum --check --strict <<'HASHES'
""" + "".join(f"{digest}  {name}\n" for name, digest in HASHES.items()) + "HASHES\n"


def check_packet():
    """Unprivileged early refusal; root repeats the literal hash checks after sealing."""
    for name, expected in HASHES.items():
        path = PACKET / name
        need(path.is_file() and not path.is_symlink(), f"Missing/unsafe packet file: {name}")
        with path.open("rb") as source:
            raw = source.read(1024 * 1024 + 1)
        need(len(raw) <= 1024 * 1024 and hashlib.sha256(raw).hexdigest() == expected,
             f"Reviewed packet differs or exceeds its bound: {name}")


def checked_plan(value):
    need(value.get("status") == "plan_only", "Root preflight did not produce a plan")
    plan = value.get("plan")
    need(isinstance(plan, dict), "Root plan is missing")
    raw = json.dumps(plan, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode()
    need(hashlib.sha256(raw).hexdigest() == PLAN and value.get("plan_sha256") == PLAN,
         "Machine plan changed; retained the plan for review and will not apply")
    need(plan.get("source_commit") == SOURCE and plan.get("enabled") is False
         and len(plan.get("actions", [])) == 42, "Unexpected disabled setup scope")


def write_new(path, raw):
    with path.open("xb") as output:
        output.write(raw)
        output.flush()
        os.fsync(output.fileno())


def execute(attempt, runner=subprocess.run):
    """Run fixed commands only; password input belongs to sudo's controlling terminal."""
    def stage(name, arguments, source=None):
        print(f"ICE setup: {name}...", flush=True)
        write_new(attempt / (name + ".started.json"),
                  json.dumps({"stage": name, "completion": "unknown"}).encode() + b"\n")
        try:
            result = runner(arguments, input=source, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            env={"PATH": "/usr/bin:/bin", "LANG": "C.UTF-8"}, timeout=180)
        except (subprocess.TimeoutExpired, KeyboardInterrupt) as error:
            write_new(attempt / (name + ".stdout"), getattr(error, "output", None) or b"")
            write_new(attempt / (name + ".stderr"), getattr(error, "stderr", None) or b"")
            write_new(attempt / (name + ".uncertain.json"), json.dumps({
                "stage": name, "reason": type(error).__name__, "completion": "unknown",
                "automatic_retry": False}, indent=2).encode() + b"\n")
            raise ValueError(f"{name} interrupted or timed out; privileged completion is unknown. "
                             f"Inspect {attempt} before any retry.") from None
        write_new(attempt / (name + ".stdout"), result.stdout)
        write_new(attempt / (name + ".stderr"), result.stderr)
        write_new(attempt / (name + ".exit"), (str(result.returncode) + "\n").encode())
        need(result.returncode == 0,
             f"{name} refused or was incomplete; inspect {attempt}. No automatic repair/retry.")
        return result.stdout

    # Root interprets only the reviewed literal shell seal, not this checkout-side wrapper.
    stage("seal", ["/usr/bin/sudo", "/bin/sh", "-eu"], seal_commands().encode())
    fixed = ["/usr/bin/sudo", "/usr/bin/python3", "-I", "-B", str(SEALED / "bootstrap.py")]
    planned = stage("root-plan", [*fixed, "--plan", "--manifest-sha256", MANIFEST])
    checked_plan(json.loads(planned))
    print("ICE setup: exact reviewed 42-action plan matches; applying disabled foundation.", flush=True)
    applied = stage("root-apply", [*fixed, "--apply", "--manifest-sha256", MANIFEST,
                                   "--approved-plan-sha256", PLAN])
    result = json.loads(applied)
    need(result.get("status") == "disabled_bootstrap_complete" and result.get("enabled") is False
         and result.get("services_started") is False and result.get("sudo_delegation") is False
         and len(result.get("completed", [])) == 42, "Setup did not confirm disabled completion")
    write_new(attempt / "receipt.json", json.dumps({"status": result["status"], "enabled": False,
              "source": SOURCE, "manifest_sha256": MANIFEST, "approved_plan_sha256": PLAN,
              "services_started": False, "sudo_delegation": False,
              "scope": "disabled foundation only; CI is not yet commissioned"}, indent=2).encode() + b"\n")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Read-only packet check; no sudo or changes")
    args = parser.parse_args(argv)
    attempt = None
    try:
        need(sys.platform == "linux" and os.geteuid() != 0,
             "Run this wrapper as ordinary jmax on Spacegoo, never with sudo")
        need(os.getuid() == 1000, "This reviewed machine handoff is for Spacegoo's jmax UID1000")
        check_packet()
        need(not SEALED.exists() and not SEALED.is_symlink(),
             "Root staging already exists; inspect prior setup before any retry")
        if args.check:
            print("Reviewed packet verified; check only. No sudo, files or accounts changed.")
            return 0
        need(sys.stdin.isatty(), "Run with ssh -t so sudo can prompt in your terminal")
        os.umask(0o077)
        attempt = Path(tempfile.mkdtemp(prefix="owner-command-", dir=HANDOFF))
        print(f"ICE setup records: {attempt}", flush=True)
        print("Enter your Spacegoo sudo password if prompted; it is not recorded.", flush=True)
        execute(attempt)
        print("Disabled setup complete. No services/jobs started; no GitHub spending changed.", flush=True)
        return 0
    except (OSError, ValueError, subprocess.SubprocessError, KeyboardInterrupt) as error:
        print(f"ICE setup needs inspection: {error or type(error).__name__}", file=sys.stderr)
        if attempt is not None:
            print(f"Retained records: {attempt}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
