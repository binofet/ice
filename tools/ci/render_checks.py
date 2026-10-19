#!/usr/bin/env python3
"""Run each explicit Linux display/GPU executable even if another one fails."""

import argparse
import os
from pathlib import Path
import signal
import subprocess
import sys

import xvfb_readiness as xvfb


def cases(build="build/ci"):
    display = ["xvfb-run", "-a", "-e", "/dev/stderr", "-s", "-screen 0 1280x800x24 -nolisten tcp"]
    return (
        ("gpu-environment", [*display, "glxinfo", "-B"]),
        ("native-x11", [*display, f"{build}/ice_platform_input_x11_tests"]),
        ("native-window", [*display, f"{build}/bin/ice_window_smoke"]),
        ("gpu-gl", [*display, f"{build}/bin/ice_gl_tests", "tests/data/gl"]),
        ("gpu-switch", [*display, f"{build}/bin/ice_render_switch_tests", "tests/data"]),
        ("gpu-reload", [*display, f"{build}/bin/ice_live_pipeline_tests"]),
        ("gpu-mesh", [*display, f"{build}/bin/ice_mesh_tests", "tests/data/mesh"]),
        ("gpu-textured", [*display, f"{build}/bin/ice_textured_mesh_tests"]),
        ("gpu-skinned", [*display, f"{build}/bin/ice_skinned_mesh_tests"]),
        ("gpu-hdr", [*display, f"{build}/bin/ice_hdr_tests", "tests/data/hdr"]),
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", default="build/ci")
    args = parser.parse_args()
    try:
        descriptor = xvfb.mesa_descriptor({})
    except (OSError, ValueError) as error:
        reason = str(error) if isinstance(error, xvfb.PreflightError) else type(error).__name__
        print('Mesa EGL descriptor refused before Xvfb: ' + reason, file=sys.stderr)
        return 64
    environment = dict(os.environ, LANG="C.UTF-8", XMODIFIERS="@im=none",
                       LIBGL_ALWAYS_SOFTWARE="1", LP_NUM_THREADS=os.environ.get("CI_PARALLEL", "4"),
                       __EGL_VENDOR_LIBRARY_FILENAMES=str(xvfb.MESA_JSON))
    for name in ("MESA_GL_VERSION_OVERRIDE", "MESA_GLSL_VERSION_OVERRIDE",
                 "MESA_GLES_VERSION_OVERRIDE"):
        environment.pop(name, None)
    print('EGL vendor descriptor: ' + str(xvfb.MESA_JSON) + ' sha256=' + descriptor['sha256'], flush=True)
    failed, cancelled, active = False, None, None
    previous = {}

    def cancel(number, _frame):
        nonlocal cancelled
        if cancelled is None:
            cancelled = number
        if active is not None:
            try:
                active.send_signal(number)
            except ProcessLookupError:
                pass

    try:
        if os.name != 'nt':
            for number in (signal.SIGINT, signal.SIGTERM):
                previous[number] = signal.signal(number, cancel)
        for name, command in cases(args.build):
            if cancelled is not None:
                break
            active = subprocess.Popen([sys.executable, str(Path(__file__).with_name("run_stage.py")),
                                       "--timeout-seconds", "240", name, "--", *command],
                                      env=environment)
            if cancelled is not None:
                active.send_signal(cancelled)
            result = active.wait()
            active = None
            failed |= result != 0
        return 128 + cancelled if cancelled is not None else int(failed)
    finally:
        for number, handler in previous.items():
            signal.signal(number, handler)


if __name__ == "__main__":
    sys.exit(main())
