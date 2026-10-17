#!/usr/bin/env python3
"""Package committed disabled-bootstrap bytes for independent review; never install them."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

import bootstrap


def write_bundle(output, commit, files):
    """Write a new review packet. This helper grants no trust to the packet or its hashes."""
    bootstrap.need(len(commit) == 40 and all(c in "0123456789abcdef" for c in commit),
                   "Exact Git source commit required")
    bootstrap.need(set(files) == set(bootstrap.PACKET_FILES), "Unexpected packet files")
    bootstrap.need(bootstrap.canonical(bootstrap.json_object(files["payload/policy.json"]))
                   == bootstrap.canonical(bootstrap.disabled_policy()),
                   "Policy must remain the exact disabled phase-one policy")
    output = Path(output)
    output.mkdir(mode=0o700)  # Exclusive: no overwrite, merge or automatic partial-output removal.
    (output / "payload").mkdir(mode=0o700)
    manifest = {"format": "ice-family-disabled-bootstrap-v1", "source_commit": commit,
                "files": {name: bootstrap.digest(raw) for name, raw in sorted(files.items())}}
    raw_manifest = bootstrap.canonical(manifest) + b"\n"
    for name, raw in [*files.items(), ("manifest.json", raw_manifest)]:
        with (output / name).open("xb") as target:
            target.write(raw)
            target.flush()
            os.fchmod(target.fileno(), 0o400)
            os.fsync(target.fileno())
    for directory in (output / "payload", output):
        fd = os.open(directory, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        try:
            os.fsync(fd)
        finally:
            os.close(fd)
    return {"format": "ice-family-disabled-bootstrap-review-v1", "source_commit": commit,
            "directory": str(output.absolute()), "manifest_sha256": bootstrap.digest(raw_manifest),
            "installer_sha256": manifest["files"]["bootstrap.py"],
            "files": manifest["files"], "installed": False, "enabled": False}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True, help="New directory; its parent must exist")
    args = parser.parse_args(argv)
    output_started = False
    try:
        root = Path(subprocess.check_output(["git", "rev-parse", "--show-toplevel"], text=True).strip())
        prefix = "tools/ci/spacegoo/"
        commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
        bootstrap.need(not subprocess.check_output(["git", "status", "--porcelain",
                                                    "--untracked-files=normal"], cwd=root),
                       "Package only a clean committed checkout; no dirty-source override exists")
        files = {}
        for name in bootstrap.PACKET_FILES:
            source = ("policy.example.json" if name == "payload/policy.json" else Path(name).name)
            files[name] = subprocess.check_output(["git", "show", commit + ":" + prefix + source], cwd=root)
        bootstrap.need(files["bootstrap.py"] == Path(bootstrap.__file__).read_bytes(),
                       "Loaded bootstrap differs from committed source")
        bootstrap.need(not args.output.exists() and not args.output.is_symlink(), "Output already exists")
        output_started = True
        print(json.dumps(write_bundle(args.output, commit, files), indent=2))
        return 0
    except (Exception, KeyboardInterrupt) as error:
        print(json.dumps({"status": "partial_packet" if output_started else "refused",
                          "diagnostic": str(error), "directory": str(args.output),
                          "installed": False, "enabled": False}), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
