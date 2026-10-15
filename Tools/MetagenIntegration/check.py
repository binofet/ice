#!/usr/bin/env python3
"""Verify ICE consumer caches or regenerate through the explicitly checked-out Metagen SDK."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
CACHE = HERE / 'Generated'
MANIFEST = CACHE / 'provenance.json'


def source_commit(source):
    return subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()


def recorded_commit():
    row = subprocess.check_output(['git', '-C', str(ROOT), 'ls-files', '--stage', '--',
                                   'Tools/Metagen'], text=True).split()
    if len(row) != 4 or row[0] != '160000':
        raise RuntimeError('Stage the Tools/Metagen Git submodule before checking consumer caches')
    return row[1]


def digest_files():
    return {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(CACHE.iterdir()) if p.is_file() and p != MANIFEST}


def verify():
    manifest = json.loads(MANIFEST.read_text())
    if set(manifest) != {'repository', 'commit', 'files'} or manifest['repository'] != 'https://github.com/binofet/metagen':
        raise RuntimeError('Invalid consumer-cache provenance')
    if manifest['commit'] != recorded_commit():
        raise RuntimeError('Metagen submodule pointer and cache provenance disagree; regenerate/review')
    if manifest['files'] != digest_files():
        raise RuntimeError('Consumer caches differ from their recorded hashes; regenerate/review')
    print(f"ICE consumer caches verified ({len(manifest['files'])} files; Metagen {manifest['commit'][:12]}). "
          'Source reproducibility runs in Metagen CI or via this explicit source check.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cached-only', action='store_true', help='no private source, .NET or extra Python packages required')
    parser.add_argument('--metagen-source', type=Path, default=ROOT / 'Tools/Metagen')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/metagen-output')
    parser.add_argument('--csharp-json', type=Path)
    parser.add_argument('--update', action='store_true')
    args = parser.parse_args()
    if args.cached_only:
        if args.update or args.csharp_json:
            parser.error('--cached-only cannot regenerate caches or use reflected inputs')
        verify()
        return
    source = args.metagen_source.resolve()
    if not (source / 'check.py').is_file():
        raise RuntimeError('Initialize the private Tools/Metagen submodule or pass --metagen-source PATH')
    commit = source_commit(source)
    if args.update and subprocess.check_output(['git', '-C', str(source), 'status', '--porcelain'], text=True).strip():
        raise RuntimeError('Commit Metagen changes before publishing matching ICE cache provenance')
    command = [sys.executable, str(source / 'check.py'), '--cache-dir', str(CACHE), '--output', str(args.output.resolve())]
    if args.update:
        command.append('--update')
    if args.csharp_json:
        command += ['--csharp-json', str(args.csharp_json.resolve())]
    subprocess.run(command, check=True)
    if args.update:
        MANIFEST.write_text(json.dumps({'repository': 'https://github.com/binofet/metagen',
                                       'commit': commit, 'files': digest_files()}, indent=2) + '\n')
    verify()


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print('Metagen integration: ' + str(error), file=sys.stderr)
        raise SystemExit(1)
