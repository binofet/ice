#!/usr/bin/env python3
"""Pinned C++ checks. Default scope is explicit; legacy/vendor migration is opt-in."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
VERSION = "18.1.8"
EXTENSIONS = {".cpp", ".h", ".hpp", ".cc", ".c", ".mm", ".m", ".inl"}
VENDOR_ROOTS = ("engine/3rdparty/", "Projects/Airhockey/Box2D/", "Projects/SkeeBall/bullet/",
                "Common/", "Experimental/", "build/")


def owned(path):
    path = Path(path).resolve()
    if not path.is_relative_to(ROOT) or not path.is_file():
        return False
    name = path.relative_to(ROOT).as_posix()
    return (path.suffix in EXTENSIONS and not name.lower().startswith(tuple(root.lower() for root in VENDOR_ROOTS))
            and not path.stem.lower().startswith("wiimote") and path.name != "resource.h")


def git_paths(*arguments):
    data = subprocess.check_output(["git", *arguments, "-z"], cwd=ROOT)
    return [ROOT / os.fsdecode(p) for p in data.split(b"\0") if p]


def tool_path(name, explicit):
    local = ROOT / "build/quality-tools" / ("Scripts" if os.name == "nt" else "bin") / name
    candidate = explicit or (str(local) if local.exists() else shutil.which(name))
    if not candidate:
        raise RuntimeError("Install Tools/Quality/requirements.txt into build/quality-tools first")
    version = subprocess.check_output([candidate, "--version"], text=True)
    if not re.search(r"\b" + re.escape(VERSION) + r"\b", version):
        raise RuntimeError(f"Expected {name} {VERSION}; got {version.strip()}")
    return candidate


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("format", "lint"))
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--changed-from", metavar="GIT_REF")
    selection.add_argument("--all", action="store_true", help="all owned tracked C/C++ files")
    selection.add_argument("--files", nargs="+", help="explicit owned files, relative to repo root")
    parser.add_argument("--fix", action="store_true", help="format in place; never used by CI")
    parser.add_argument("--tool", help="explicit path to the pinned formatter or analyzer")
    parser.add_argument("--build-dir", default="build/desktop-debug")
    args = parser.parse_args()
    if args.fix and args.command != "format":
        parser.error("--fix applies only to formatting; analyzer fixes require manual review")
    scope = json.loads((ROOT / "Tools/Quality/scope.json").read_text())
    if args.changed_from:
        paths = git_paths("diff", "--name-only", "--diff-filter=ACMR", args.changed_from)
        paths += git_paths("ls-files", "--others", "--exclude-standard")
    elif args.all:
        paths = git_paths("ls-files") + git_paths("ls-files", "--others", "--exclude-standard")
    elif args.files:
        paths = [ROOT / p for p in args.files]
        if any(not owned(p) for p in paths):
            raise RuntimeError("Explicit selections must be existing first-party C/C++ files")
    else:
        paths = [ROOT / p for p in scope[args.command]]
        missing = [str(p) for p in paths if not owned(p)]
        if missing:
            raise RuntimeError("Invalid adopted scope entries: " + ", ".join(missing))
    files = sorted({p.resolve() for p in paths if owned(p)})
    executable = tool_path("clang-format" if args.command == "format" else "clang-tidy", args.tool)
    if args.command == "format":
        failed = 0
        for path in files:
            flags = ["-i"] if args.fix else ["--dry-run", "--Werror"]
            failed += subprocess.run([executable, *flags, str(path)], cwd=ROOT).returncode != 0
        print(f"Formatting: {len(files)} files; {failed} failed", flush=True)
        return int(failed != 0)
    build = (ROOT / args.build_dir).resolve()
    database = build / "compile_commands.json"
    if not database.exists():
        raise RuntimeError("Configure a Ninja/Make desktop build with CMAKE_EXPORT_COMPILE_COMMANDS=ON")
    # Headers are checked via their adopted translation units, not invented compiler arguments.
    if any(p.suffix in {".h", ".hpp"} for p in files):
        files = sorted(set(files + [ROOT / p for p in scope["lint"]]))
    files = [p for p in files if p.suffix in {".cpp", ".cc", ".c", ".mm", ".m"}]
    records = json.loads(database.read_text())
    compiled = {(Path(r["directory"]) / r["file"]).resolve() for r in records}
    missing = [p.relative_to(ROOT).as_posix() for p in files if p not in compiled]
    if missing:
        raise RuntimeError("No compile command for: " + ", ".join(missing))
    failed = 0
    for path in files:
        failed += subprocess.run([executable, "-p", str(build), str(path)], cwd=ROOT).returncode != 0
    print(f"Analysis: {len(files)} translation units; {failed} failed", flush=True)
    return int(failed != 0)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, OSError, subprocess.CalledProcessError) as error:
        print(f"Quality check: {error}", file=sys.stderr)
        sys.exit(2)
