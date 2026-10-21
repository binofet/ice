#!/usr/bin/env python3
"""Owned-runner source-job preflight; no registration, host installation or dispatch."""
import argparse
import hashlib
import importlib.metadata
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
PREFIX = Path('/opt/ice-family-ci/toolchains/native-20261018-v1')
PACKAGES = {'clang-format': '18.1.8', 'clang-tidy': '18.1.8',
            'ninja': '1.13.0', 'libclang': '18.1.1'}
REQUIREMENTS = {'ice': 'tools/quality/requirements.txt'}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def runtime():
    system = platform.system()
    runner_os = {'Linux': 'Linux', 'Darwin': 'macOS'}.get(system, system)
    machine = platform.machine()
    arch = {'x86_64': 'X64', 'amd64': 'X64', 'arm64': 'ARM64', 'aarch64': 'ARM64'}.get(machine.lower(), machine)
    release = {}
    if system == 'Linux':
        release = {key: value for key, value in platform.freedesktop_os_release().items()
                   if key in {'ID', 'NAME', 'VERSION_ID', 'PRETTY_NAME'}}
    elif system == 'Darwin':
        release = {'version': platform.mac_ver()[0]}
    result = {'system': system, 'runner_os': runner_os, 'architecture': arch,
              'machine': machine, 'release': release, 'kernel': platform.release(),
              'python': platform.python_version(), 'python_executable': str(Path(sys.executable).resolve())}
    require((runner_os, arch) in {('Linux', 'X64'), ('macOS', 'ARM64')}, 'Unsupported owned source executor OS/architecture')
    require(os.environ.get('RUNNER_OS') == runner_os and os.environ.get('RUNNER_ARCH') == arch,
            'Provider runner OS/architecture disagrees with observed native runtime')
    require(result['python'] == '3.12.15', 'Owned source jobs require preprovisioned Python3.12.15')
    require(bool(release), 'Actual native OS release is unavailable')
    return result


NIGHTLY_CRON = '17 9 * * *'


def quality_plan(repo, event=None, payload=None, ref=None, commit=None):
    event = os.environ.get('GITHUB_EVENT_NAME', 'local') if event is None else event
    if payload is None:
        path = os.environ.get('GITHUB_EVENT_PATH')
        payload = json.loads(Path(path).read_text()) if path else {}
    inputs = payload.get('inputs', {}) if event == 'workflow_dispatch' else {}
    require(isinstance(inputs, dict), 'Quality inputs must be an object')
    selected = inputs.get('quality_plan', 'day')
    require(isinstance(selected, str) and selected in {'day', 'nightly'}, 'Unknown day/nightly quality plan')
    if event != 'schedule' and selected != 'nightly':
        return 'day'
    repository = payload.get('repository', {})
    require(isinstance(repository, dict), 'Nightly repository metadata must be an object')
    require(repo == 'ice' and repository.get('id') == 40782766
            and repository.get('fork') is False and repository.get('default_branch') == 'master'
            and (os.environ.get('GITHUB_REF') if ref is None else ref) == 'refs/heads/master',
            'Nightly quality requires the fixed ICE default branch')
    if event == 'schedule':
        require(payload.get('schedule') == NIGHTLY_CRON, 'Unexpected nightly schedule')
    else:
        require(event == 'workflow_dispatch' and inputs.get('diagnostic') == 'none'
                and inputs.get('expected_head') == (os.environ.get('GITHUB_SHA') if commit is None else commit),
                'Manual nightly quality requires the exact event head and no diagnostic')
    return 'nightly'


def standards_plan(source):
    if 'standards_plan' not in source:
        return 'legacy'
    selected = source['standards_plan']
    require(isinstance(selected, str) and selected in {'day-cpp23-v1', 'nightly-cpp20-23-v1'},
            'Unknown standards plan')
    expected = 'day' if selected == 'day-cpp23-v1' else 'nightly'
    require(source.get('quality_plan') == expected, 'Standards and quality plans conflict')
    allowed = {'23'} if selected == 'day-cpp23-v1' else {'20', '23'}
    require(str(source.get('cpp')) in allowed, 'C++ standard is outside the declared plan')
    require(source.get('profile') != 'sanitizers' or str(source.get('cpp')) == '23',
            'Sanitizers require C++23')
    return selected


def source_scope(repo):
    require(repo in REQUIREMENTS, 'Unsupported source repository')
    require(os.environ.get('GITHUB_REPOSITORY') == 'binofet/' + repo, 'Unexpected source repository')
    require(Path(os.environ.get('GITHUB_WORKSPACE', '')).resolve() == ROOT, 'Unexpected source workspace')
    event = os.environ.get('GITHUB_EVENT_NAME')
    require(event in {'push', 'pull_request', 'workflow_dispatch', 'schedule'}, 'Unsupported source event')
    quality_plan(repo)
    if event == 'pull_request':
        payload = json.loads(Path(os.environ['GITHUB_EVENT_PATH']).read_text())
        require(payload.get('pull_request', {}).get('head', {}).get('repo', {}).get('full_name') == 'binofet/' + repo,
                'Fork source must not execute on the persistent owned source runner')


def probe(command, environment=None):
    executable = shutil.which(command[0], path=(environment or os.environ).get('PATH'))
    require(executable is not None, 'Missing preprovisioned tool: ' + command[0])
    completed = subprocess.run([executable, *command[1:]], env=environment, text=True,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30, check=True)
    text = completed.stdout.strip()
    require(len(text.encode('utf-8')) <= 16384, 'Tool version output exceeds evidence bound')
    print('+ ' + ' '.join(command) + '\n' + text, flush=True)
    return {'executable': str(Path(executable).resolve()), 'output': text}


def toolchain(repo, profile, observed):
    commands = {name: probe([name, '--version']) for name in ('git', 'cmake', 'ctest', 'make')}
    expected = '4.2.3' if observed['runner_os'] == 'Linux' else '3.27.2'
    for name in ('cmake', 'ctest'):
        require(re.search(r'\bversion ' + re.escape(expected) + r'(?:\s|$)', commands[name]['output']),
                name + ' differs from the owned toolchain contract')
    if observed['runner_os'] == 'Linux':
        for name in ('gcc-13', 'g++-13'):
            commands[name] = probe([name, '-dumpfullversion', '-dumpversion'])
            require(commands[name]['output'].split('.')[0] == '13', 'GCC13 is required')
        if profile == 'native':
            commands['clang++-18'] = probe(['/usr/bin/clang++-18', '--version'])
            require(re.search(r'\bversion 18\.', commands['clang++-18']['output']), 'Native clang18 headers/tools are required')
            for name in ('xvfb-run', 'Xvfb', 'xauth', 'glxinfo'):
                path = shutil.which(name)
                require(path is not None, 'Missing preprovisioned software-render tool: ' + name)
                commands[name] = {'executable': str(Path(path).resolve())}
    else:
        commands['xcode'] = probe(['xcodebuild', '-version'])
        commands['sdk'] = probe(['xcrun', '--sdk', 'macosx', '--show-sdk-version'])
        commands['compiler'] = probe(['xcrun', 'clang++', '--version'])
        require(re.search(r'^Xcode 26\.3(?:\s|$)', commands['xcode']['output']), 'Xcode26.3 is required')
        require(commands['sdk']['output'] == '26.2', 'macOS SDK26.2 is required')
    if repo == 'ice' and profile == 'native':
        environment = dict(os.environ, RUSTUP_TOOLCHAIN='1.90.0', RUSTUP_AUTO_INSTALL='0')
        if observed['runner_os'] == 'Linux':
            environment['RUSTUP_HOME'] = str(PREFIX / 'rustup')
        require(bool(environment.get('RUSTUP_HOME')), 'Owned-shell must supply the private/preprovisioned Rust home')
        commands['rustc'] = probe(['rustc', '--version'], environment)
        require(commands['rustc']['output'].startswith('rustc 1.90.0 '), 'Rust1.90.0 is required; auto-install is disabled')
    validate_commands(commands, repo, profile, observed)
    return commands


def validate_commands(commands, repo, profile, observed):
    required = {'git', 'cmake', 'ctest', 'make'}
    linux = observed['runner_os'] == 'Linux'
    if linux:
        required |= {'gcc-13', 'g++-13'}
        if profile == 'native':
            required |= {'clang++-18', 'xvfb-run', 'Xvfb', 'xauth', 'glxinfo'}
    else:
        required |= {'xcode', 'sdk', 'compiler'}
    if repo == 'ice' and profile == 'native':
        required.add('rustc')
    require(isinstance(commands, dict) and required <= commands.keys(), 'Owned toolchain command inventory is incomplete')
    for name in required:
        row = commands[name]
        require(isinstance(row, dict) and isinstance(row.get('executable'), str)
                and Path(row['executable']).is_absolute(), 'Owned tool path is missing: ' + name)
    expected = '4.2.3' if linux else '3.27.2'
    for name in ('cmake', 'ctest'):
        require(re.search(r'\bversion ' + re.escape(expected) + r'(?:\s|$)', commands[name].get('output', '')),
                'Owned CMake/CTest version differs')
    if linux:
        for name in ('gcc-13', 'g++-13'):
            require(commands[name].get('output', '').split('.')[0] == '13', 'Owned GCC version differs')
        if profile == 'native':
            require(re.search(r'\bversion 18\.', commands['clang++-18'].get('output', '')),
                    'Owned native clang version differs')
    else:
        require(re.search(r'^Xcode 26\.3(?:\s|$)', commands['xcode'].get('output', ''))
                and commands['sdk'].get('output') == '26.2', 'Owned Xcode/SDK evidence differs')
    if 'rustc' in required:
        require(commands['rustc'].get('output', '').startswith('rustc 1.90.0 '), 'Owned Rust version differs')


def verify_quality(repo):
    require(sys.prefix != sys.base_prefix and Path(sys.prefix).resolve() == ROOT / 'build/ci-tools/venv',
            'Quality tools must run in the fresh checkout-local venv')
    actual = {name: importlib.metadata.version(name) for name in PACKAGES}
    require(actual == PACKAGES, 'Project quality package versions differ from exact pins')
    for name in ('clang-format', 'clang-tidy', 'ninja'):
        path = shutil.which(name)
        require(path is not None and Path(path).parent.resolve() == ROOT / 'build/ci-tools/venv/bin',
                'Quality executable escaped the checkout-local venv: ' + name)
        probe([name, '--version'])
    return actual


def prepare(repo, profile):
    source_scope(repo)
    observed = runtime()
    commands = toolchain(repo, profile, observed)
    requirements = ROOT / REQUIREMENTS[repo]
    expected = [name + '==' + version for name, version in PACKAGES.items()]
    require(requirements.read_text().splitlines() == expected, 'Quality requirements differ from reviewed exact pins')
    venv = ROOT / 'build/ci-tools/venv'
    require(os.environ.get('CI_VENV') == str(venv), 'CI_VENV must name the checkout-local tool environment')
    require(not venv.exists() and not venv.is_symlink(), 'Refuse existing tool venv; use a fresh checkout/attempt')
    venv.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([sys.executable, '-m', 'venv', str(venv)], check=True, timeout=120)
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith('PIP_') and key not in {'PYTHONHOME', 'PYTHONPATH'}}
    environment.update(PIP_CONFIG_FILE=os.devnull, PIP_REQUIRE_VIRTUALENV='1', PYTHONNOUSERSITE='1',
                       PATH=str(venv / 'bin') + os.pathsep + os.environ['PATH'])
    python = venv / 'bin/python3'
    subprocess.run([str(python), '-m', 'pip', 'install', '--require-virtualenv', '--no-input',
                    '--disable-pip-version-check', '--no-cache-dir', '--index-url', 'https://pypi.org/simple',
                    '-r', str(requirements)], env=environment, check=True, timeout=600)
    versions = subprocess.check_output([str(python), '-c',
        'import importlib.metadata,json; print(json.dumps({name:importlib.metadata.version(name) for name in '
        + repr(list(PACKAGES)) + '}))'], env=environment, text=True, timeout=30)
    require(json.loads(versions) == PACKAGES, 'Installed project packages differ from exact pins')
    output = Path(os.environ['CI_EVIDENCE_DIR'])
    source = output / 'source.json'
    require(source.is_file(), 'Source identity must precede dependency preparation')
    metadata = {'format': 'ice-family-owned-toolchain-v1', 'repository': 'binofet/' + repo,
                'profile': profile, 'runtime': observed, 'commands': commands,
                'source_id': digest(source), 'requirements_sha256': digest(requirements),
                'packages': dict(PACKAGES), 'venv': 'build/ci-tools/venv'}
    path = output / 'toolchain.json'
    require(not path.exists(), 'Refuse an existing toolchain evidence record')
    path.write_text(json.dumps(metadata, indent=2, allow_nan=False) + '\n')
    print('Prepared checkout-local pinned venv; source/control host commissioning is not implied.', flush=True)


def evidence_issues(output, source):
    if 'owned_runtime' not in source:
        return []
    try:
        metadata = json.loads((output / 'toolchain.json').read_text())
        require(metadata.get('format') == 'ice-family-owned-toolchain-v1', 'Owned toolchain evidence format differs')
        require(metadata.get('runtime') == source['owned_runtime'], 'Owned runtime changed between source and dependencies')
        require(metadata.get('source_id') == digest(output / 'source.json'), 'Owned toolchain evidence belongs to another source')
        require(metadata.get('packages') == PACKAGES and metadata.get('venv') == 'build/ci-tools/venv',
                'Owned project tool evidence differs')
        repo = metadata.get('repository', '').removeprefix('binofet/')
        require(repo in REQUIREMENTS and metadata.get('repository') == source.get('owned_repository')
                and metadata.get('profile') == source.get('profile', 'native')
                and metadata.get('requirements_sha256') == digest(ROOT / REQUIREMENTS[repo]),
                'Owned quality requirement source differs')
        require(metadata.get('runtime', {}).get('runner_os') == source['runner_os'], 'Owned OS evidence differs')
        validate_commands(metadata.get('commands'), repo, metadata['profile'], metadata['runtime'])
        return []
    except (OSError, ValueError, KeyError, TypeError) as error:
        return ['Owned runner evidence: ' + str(error)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=('prepare', 'verify-quality'))
    parser.add_argument('--repo', choices=tuple(REQUIREMENTS), required=True)
    parser.add_argument('--profile', choices=('native', 'sanitizers'), default='native')
    args = parser.parse_args()
    if args.mode == 'prepare':
        prepare(args.repo, args.profile)
    else:
        verify_quality(args.repo)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, subprocess.SubprocessError, importlib.metadata.PackageNotFoundError) as error:
        print('Owned runner preflight failed: ' + str(error), file=sys.stderr)
        raise SystemExit(1)
