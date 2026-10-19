#!/usr/bin/env python3
"""One Linux Xvfb readiness diagnostic using the existing bounded CI stage."""

import os
from pathlib import Path
import sys


CHECK = r'''
printf 'DISPLAY=%s\n' "${DISPLAY-}"
if /usr/bin/xdpyinfo -display "${DISPLAY-}"; then
    readiness_status=0
else
    readiness_status=$?
fi
printf 'x11_readiness_exit=%s\n' "$readiness_status"
if [ "$readiness_status" -ne 0 ]; then
    exit "$readiness_status"
fi
if /usr/bin/glxinfo -B; then
    glxinfo_status=0
else
    glxinfo_status=$?
fi
printf 'glxinfo_exit=%s\n' "$glxinfo_status"
exit "$glxinfo_status"
'''


def main():
    if len(sys.argv) != 1 or sys.platform != "linux":
        raise SystemExit("Linux diagnostic takes no arguments")
    environment = dict(os.environ, LANG="C.UTF-8", XMODIFIERS="@im=none",
                       LIBGL_ALWAYS_SOFTWARE="1", LP_NUM_THREADS="2")
    for name in ("MESA_GL_VERSION_OVERRIDE", "MESA_GLSL_VERSION_OVERRIDE",
                 "MESA_GLES_VERSION_OVERRIDE"):
        environment.pop(name, None)
    command = [sys.executable, "-I", "-S", "-B",
               str(Path(__file__).resolve().with_name("run_stage.py")),
               "--timeout-seconds", "60", "xvfb-readiness", "--",
               "/usr/bin/xvfb-run", "-a", "-e", "/dev/stderr", "-s",
               "-screen 0 1280x800x24 -nolisten tcp",
               "/bin/bash", "--noprofile", "--norc", "-euo", "pipefail", "-c", CHECK]
    os.execve(sys.executable, command, environment)


if __name__ == "__main__":
    main()
