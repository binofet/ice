"""Shared build infrastructure, independent of any generator's schema semantics."""
from pathlib import Path
import shutil
import subprocess


def format_cpp(paths):
    root = Path(__file__).resolve().parents[3]
    formatter = root / 'build/quality-tools/bin/clang-format'
    executable = str(formatter) if formatter.exists() else shutil.which('clang-format')
    if not executable or '18.1.8' not in subprocess.check_output([executable, '--version'], text=True):
        raise RuntimeError('Install Tools/Quality/requirements.txt: generated C++ requires clang-format 18.1.8')
    subprocess.run([executable, '-i', *(str(path) for path in paths)], check=True)
