#!/usr/bin/env python3
"""Require lowercase project directory names without renaming upstream contracts."""

from pathlib import PurePosixPath
import subprocess
import sys

# Vendored include paths are upstream interfaces, not project naming policy.
UPSTREAM = ("engine/3rdparty/khronos/GL/", "engine/3rdparty/khronos/KHR/",
            "examples/airhockey/Box2D/", "examples/skeeball/bullet/")


def violations(paths):
    return sorted({str(parent) for name in paths if not name.startswith(UPSTREAM)
                   for parent in PurePosixPath(name).parents
                   if str(parent) != "." and str(parent) != str(parent).lower()})


def main():
    paths = subprocess.check_output(["git", "ls-files", "-z"]).decode("utf-8").split("\0")
    invalid = violations(path for path in paths if path)
    if invalid:
        print("Project folder names must be lowercase:\n" + "\n".join(invalid), file=sys.stderr)
        return 1
    print("Tracked project folder names are lowercase; explicit upstream include trees retained.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
