#!/usr/bin/env python3
"""Run each explicit Linux display/GPU executable even if another one fails."""

import argparse
import os
from pathlib import Path
import subprocess
import sys


def cases(build="build/ci"):
    display = ["xvfb-run", "-a", "-s", "-screen 0 1280x800x24 -nolisten tcp"]
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
    environment = dict(os.environ, LANG="C.UTF-8", XMODIFIERS="@im=none",
                       LIBGL_ALWAYS_SOFTWARE="1", LP_NUM_THREADS="4")
    for name in ("MESA_GL_VERSION_OVERRIDE", "MESA_GLSL_VERSION_OVERRIDE",
                 "MESA_GLES_VERSION_OVERRIDE"):
        environment.pop(name, None)
    failed = False
    for name, command in cases(args.build):
        result = subprocess.run([sys.executable, str(Path(__file__).with_name("run_stage.py")),
                                 "--timeout-seconds", "240", name, "--", *command],
                                env=environment)
        failed |= result.returncode != 0
    return int(failed)


if __name__ == "__main__":
    sys.exit(main())
