"""Configured Apple driver defaults shared by libclang and clang-tidy."""
from functools import lru_cache
from pathlib import Path
import shlex
import subprocess
import sys


@lru_cache(maxsize=None)
def _apple_system_includes(compiler, options, language):
    """Libclang has no driver location from which to discover Apple's SDK/stdlib."""
    probe = subprocess.run([compiler, *options, "-E", "-v", "-x", language, "-"],
                           input="", text=True, capture_output=True)
    if probe.returncode:
        raise RuntimeError("Cannot discover configured Apple compiler headers: " + probe.stderr)
    result, reading = [], False
    for line in probe.stderr.splitlines():
        if " -cc1 " in line:
            invocation = shlex.split(line)
            if "-isysroot" in invocation:
                at = invocation.index("-isysroot")
                result.extend(["-isysroot", invocation[at + 1]])
        if "#include <...> search starts here:" in line:
            reading = True
        elif reading and line.strip() == "End of search list.":
            break
        elif reading:
            path = line.strip()
            framework = path.endswith(" (framework directory)")
            path = path.removesuffix(" (framework directory)")
            # Add the C++ library and frameworks. Leave C/builtin search ordering
            # to libclang with the discovered sysroot and selected resource dir;
            # putting SDK C headers first would break their include_next chains.
            directory = Path(path)
            cxx_library = directory.name == "v1" and directory.parent.name == "c++"
            if directory.is_dir() and (framework or cxx_library):
                result.extend(["-iframework" if framework else "-isystem", path])
    if not result:
        raise RuntimeError("Configured Apple compiler reported no system include directories")
    return result


def system_arguments(record):
    """Keep native SDK defaults available when a tool reads the compile database."""
    if sys.platform != "darwin":
        return []
    command = shlex.split(record["command"])
    arguments = command[1:]
    # CMake 4 may omit -isysroot and rely on the configured driver. Preserve its
    # actual SDK/stdlib search paths rather than guessing from libclang's package.
    options, index = [], 0
    while index < len(arguments):
        option = arguments[index]
        if option in ("-isysroot", "--sysroot", "-target", "--target", "-arch"):
            options.extend(arguments[index:index + 2])
            index += 2
            continue
        if option.startswith(("--sysroot=", "--target=", "-stdlib=", "-std=")) or option in (
                "-m32", "-m64", "-nostdinc", "-nostdinc++"):
            options.append(option)
        index += 1
    language = "objective-c++" if Path(record["file"]).suffix == ".mm" else "c++"
    return _apple_system_includes(command[0], tuple(options), language)
