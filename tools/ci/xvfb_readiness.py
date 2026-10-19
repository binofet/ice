#!/usr/bin/env python3
"""One Linux Xvfb readiness diagnostic using the existing bounded CI stage."""

import base64
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
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


MESA_JSON = Path('/usr/share/glvnd/egl_vendor.d/50_mesa.json')
SYSTEM_PATHS = {
    'mesa_egl': Path('/usr/lib/x86_64-linux-gnu/libEGL_mesa.so.0'),
    'egl_loader': Path('/usr/lib/x86_64-linux-gnu/libEGL.so.1'),
    'xvfb_run': Path('/usr/bin/xvfb-run'), 'xvfb': Path('/usr/bin/Xvfb'),
    'xauth': Path('/usr/bin/xauth'), 'xdpyinfo': Path('/usr/bin/xdpyinfo'),
    'glxinfo': Path('/usr/bin/glxinfo'),
}
OVERRIDES = ('MESA_GL_VERSION_OVERRIDE', 'MESA_GLSL_VERSION_OVERRIDE',
             'MESA_GLES_VERSION_OVERRIDE')
CONFLICTS = ('LD_PRELOAD', 'LD_LIBRARY_PATH', 'LD_AUDIT', 'LIBGL_DRIVERS_PATH',
             'MESA_LOADER_DRIVER_OVERRIDE', 'GALLIUM_DRIVER', 'GBM_BACKEND',
             'EGL_PLATFORM', '__GLX_VENDOR_LIBRARY_NAME')
SELECTORS = (*CONFLICTS, *OVERRIDES, '__EGL_VENDOR_LIBRARY_FILENAMES',
             '__EGL_VENDOR_LIBRARY_DIRS', 'LIBGL_ALWAYS_SOFTWARE', 'LP_NUM_THREADS')
PACKAGES = {'xvfb', 'xserver-common', 'libegl1', 'libegl-mesa0', 'libglvnd0',
            'libglx-mesa0', 'libnvidia-egl-gbm1'}
MAX_DESCRIPTOR = 4096
MAX_FILE = 32 * 1024 * 1024
MAX_RECORD = 32768


class PreflightError(ValueError):
    """A fixed reason code, never an arbitrary environment value or file body."""


def require(condition, reason):
    if not condition:
        raise PreflightError(reason)


def unique_object(pairs):
    result = {}
    for name, value in pairs:
        require(name not in result, 'duplicate-json-key')
        result[name] = value
    return result


def json_constant(_value):
    raise PreflightError('non-json-constant')


def observe_file(path, maximum):
    """Read bounded regular-file bytes; record the actual opened file identity."""
    resolved = path.resolve(strict=True)
    fd = os.open(resolved, os.O_RDONLY | os.O_NONBLOCK | os.O_CLOEXEC)
    with os.fdopen(fd, 'rb') as stream:
        before = os.fstat(stream.fileno())
        require(stat.S_ISREG(before.st_mode), 'not-regular-file')
        require(before.st_size <= maximum, 'file-size-bound')
        raw = stream.read(maximum + 1)
        after = os.fstat(stream.fileno())
    require(len(raw) <= maximum, 'file-size-bound')
    identity = lambda value: (value.st_dev, value.st_ino, value.st_size,
                              value.st_mtime_ns, value.st_ctime_ns)
    require(identity(before) == identity(after) and len(raw) == after.st_size,
            'file-changed-during-read')
    return raw, dict(path=str(path), resolved_path=str(resolved), bytes=len(raw),
                     sha256=hashlib.sha256(raw).hexdigest(), uid=after.st_uid,
                     gid=after.st_gid, mode=oct(stat.S_IMODE(after.st_mode)),
                     device=after.st_dev, inode=after.st_ino)


def system_file(record):
    mode = int(record['mode'], 8)
    require(record['uid'] == 0 and not mode & 0o022, 'untrusted-system-file')
    require(not mode & 0o6000, 'setid-system-file')


def selector_evidence(environment):
    values = {}
    for name in SELECTORS:
        value = environment.get(name)
        if value is not None:
            require(len(value) <= 256 and all(32 <= ord(c) < 127 for c in value),
                    'selector-value-bound-or-encoding')
        values[name] = value
    return values


def package_evidence():
    """Package database claims only; never execute a tool or load an ELF image."""
    try:
        raw, identity = observe_file(Path('/var/lib/dpkg/status'), MAX_FILE)
        entries = []
        for paragraph in raw.decode('utf-8').split('\n\n'):
            fields = dict(line.split(': ', 1) for line in paragraph.splitlines()
                          if ': ' in line and not line.startswith((' ', '\t')))
            if fields.get('Package') in PACKAGES:
                selected = {key: fields[key] for key in
                            ('Package', 'Version', 'Architecture', 'Status') if key in fields}
                require(all(len(value) <= 256 for value in selected.values()), 'package-field-bound')
                entries.append(selected)
        require(len(entries) <= 2 * len(PACKAGES), 'package-count-bound')
        return dict(available=True, database=identity, entries=entries,
                    claim='installed package database metadata; not binary version or load proof')
    except (OSError, ValueError) as error:
        return dict(available=False, error_class=type(error).__name__)


def preflight(record, inherited):
    credentials = dict(uid=os.getuid(), euid=os.geteuid(), gid=os.getgid(), egid=os.getegid())
    record['credentials'] = credentials
    require(credentials['uid'] == credentials['euid'] == 984, 'owned-account-mismatch')
    require(credentials['gid'] == credentials['egid'], 'gid-egid-mismatch')
    record['inherited_selectors'] = selector_evidence(inherited)
    require(not any(inherited.get(name) for name in CONFLICTS), 'conflicting-selector')
    environment = dict(inherited, LANG='C.UTF-8', XMODIFIERS='@im=none',
                       LIBGL_ALWAYS_SOFTWARE='1', LP_NUM_THREADS='2',
                       __EGL_VENDOR_LIBRARY_FILENAMES=str(MESA_JSON))
    for name in OVERRIDES:
        environment.pop(name, None)
    record['effective_selectors'] = selector_evidence(environment)

    raw, descriptor = observe_file(MESA_JSON, MAX_DESCRIPTOR)
    record['descriptor'] = dict(descriptor, actual_bytes_base64=base64.b64encode(raw).decode('ascii'))
    system_file(descriptor)
    try:
        document = json.loads(raw.decode('utf-8'), object_pairs_hook=unique_object,
                              parse_constant=json_constant)
    except (UnicodeError, ValueError, RecursionError) as error:
        raise PreflightError('malformed-mesa-json') from error
    require(type(document) is dict and type(document.get('file_format_version')) is str
            and re.fullmatch(r'1\.0\.[0-9]{1,4}', document['file_format_version'])
            and type(document.get('ICD')) is dict, 'mesa-json-format-mismatch')
    library = document['ICD'].get('library_path')
    require(type(library) is str and len(library) <= 256, 'mesa-library-path-mismatch')
    if library != 'libEGL_mesa.so.0':
        try:
            matches = (Path(library).is_absolute() and Path(library).name == 'libEGL_mesa.so.0'
                       and Path(library).resolve(strict=True) == SYSTEM_PATHS['mesa_egl'].resolve(strict=True))
        except (OSError, ValueError):
            matches = False
        require(matches, 'mesa-library-path-mismatch')
    record['descriptor'].update(file_format_version=document['file_format_version'],
                                library_path=library)

    record['files'] = {}
    for name, path in SYSTEM_PATHS.items():
        raw, identity = observe_file(path, MAX_FILE)
        record['files'][name] = identity
        system_file(identity)
        if name == 'xvfb_run':
            require(raw.startswith(b'#!'), 'helper-format-mismatch')
        else:
            require(len(raw) >= 20 and raw[:6] == b'\x7fELF\x02\x01'
                    and raw[18:20] == b'\x3e\x00', 'elf-architecture-mismatch')
        if name not in ('mesa_egl', 'egl_loader'):
            require(os.access(path, os.X_OK), 'helper-not-executable')
    record['path_resolution'] = {}
    for command, name in (('Xvfb', 'xvfb'), ('xauth', 'xauth')):
        found = shutil.which(command, path=environment.get('PATH', os.defpath))
        record['path_resolution'][command] = found
        require(found is not None and Path(found).resolve(strict=True) == SYSTEM_PATHS[name].resolve(strict=True),
                'helper-path-mismatch')
    record['packages'] = package_evidence()
    record['library_identity_limit'] = ('Files above are intended system ELF identities read without loading. '
        'A bare ICD library name is resolved by the runtime loader later; no loaded-library or renderer proof is claimed.')
    return environment


def display_command():
    return ['/usr/bin/xvfb-run', '-a', '-e', '/dev/stderr', '-s',
            '-screen 0 1280x800x24 -nolisten tcp',
            '/bin/bash', '--noprofile', '--norc', '-euo', 'pipefail', '-c', CHECK]


def preflight_and_run():
    record = dict(format='ice-xvfb-preflight-v1', status='failed',
                  purpose='diagnostic-only; no native quality qualification',
                  graphics_commands_invoked=False, expected_head=os.environ.get('DIAG_EXPECTED_HEAD'),
                  bounds=dict(descriptor_bytes=MAX_DESCRIPTOR, file_bytes=MAX_FILE,
                              selector_characters=256, record_bytes=MAX_RECORD))
    environment = None
    try:
        environment = preflight(record, os.environ)
        record['status'] = 'ready'
    except (OSError, ValueError) as error:
        record['reason'] = str(error) if isinstance(error, PreflightError) else type(error).__name__
    raw = json.dumps(record, indent=2, allow_nan=False).encode('utf-8') + b'\n'
    require(len(raw) <= MAX_RECORD, 'preflight-record-bound')
    with (Path(os.environ['CI_EVIDENCE_DIR']) / 'xvfb-preflight.json').open('xb') as stream:
        stream.write(raw)
    print('xvfb_preflight_status=' + record['status'], flush=True)
    if record['status'] != 'ready':
        print('xvfb_preflight_reason=' + record['reason'], flush=True)
        return 64
    os.execve('/usr/bin/xvfb-run', display_command(), environment)


def main():
    if sys.platform != 'linux':
        raise SystemExit('Linux diagnostic only')
    if sys.argv[1:] == ['--preflight-and-run']:
        return preflight_and_run()
    if len(sys.argv) != 1:
        raise SystemExit('Linux diagnostic takes no arguments')
    # The preflight and subsequent exec share the existing stage deadline/group.
    command = [sys.executable, "-I", "-S", "-B",
               str(Path(__file__).resolve().with_name("run_stage.py")),
               "--timeout-seconds", "60", "xvfb-readiness", "--",
               sys.executable, '-I', '-S', '-B', str(Path(__file__).resolve()), '--preflight-and-run']
    os.execve(sys.executable, command, dict(os.environ))


if __name__ == "__main__":
    raise SystemExit(main())
