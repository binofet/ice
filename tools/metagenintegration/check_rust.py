#!/usr/bin/env python3
"""Compile the standalone Rust example and verify C++ -> Rust -> C++ binary equality."""
import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native-build', type=Path, required=True)
    parser.add_argument('--rustc', help='Rust compiler; default PATH, then the isolated local proof toolchain')
    args = parser.parse_args()
    rustc = args.rustc or shutil.which('rustc')
    if not rustc:
        local = list((ROOT / 'build/metagen-rust-tools/rustup/toolchains').glob('1.90.0-*/bin/rustc'))
        rustc = str(local[0]) if local else None
    if not rustc:
        parser.error('Install Rust 1.90+ or pass --rustc; this script never installs toolchains')
    build = args.native_build.resolve()
    subprocess.run(['cmake', '--build', str(build), '--target', 'meta_wire_fixture'], check=True)
    output = build / 'rust-proof'
    output.mkdir(exist_ok=True)
    executable = output / 'metagen-rust-proof'
    subprocess.run([rustc, '--edition=2021', '-Dwarnings', str(ROOT / 'tools/metagenintegration/examples/rust/main.rs'),
                    '-o', str(executable)], check=True)
    paths = [str(output / name) for name in ('cpp-default.icm', 'cpp-changed.icm', 'rust-default.icm', 'rust-changed.icm')]
    fixture = str(build / 'meta_wire_fixture')
    subprocess.run([fixture, '--emit', *paths[:2]], check=True)
    subprocess.run([str(executable), *paths], check=True)
    subprocess.run([fixture, '--verify', *paths[2:]], check=True)


if __name__ == '__main__':
    main()
