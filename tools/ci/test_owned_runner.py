#!/usr/bin/env python3
"""Offline owned-runner contracts; no host setup, package installation or native build."""
import copy
import importlib.util
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

import owned_runner as owned


class OwnedRunnerTests(unittest.TestCase):
    def observed(self, system='Linux'):
        return {'system': system, 'runner_os': 'Linux' if system == 'Linux' else 'macOS',
                'architecture': 'X64' if system == 'Linux' else 'ARM64', 'python': '3.12.15',
                'machine': 'x86_64' if system == 'Linux' else 'arm64', 'kernel': 'fixture',
                'release': {'VERSION_ID': '26.04'} if system == 'Linux' else {'version': '26.5.2'},
                'python_executable': '/owned/python3.12'}

    def commands(self, repo='ice', system='Linux', profile='native'):
        output = {'git': 'git version 2.53.0', 'make': 'GNU Make',
                  'cmake': 'cmake version ' + ('4.2.3' if system == 'Linux' else '3.27.2'),
                  'ctest': 'ctest version ' + ('4.2.3' if system == 'Linux' else '3.27.2')}
        if system == 'Linux':
            output.update({'gcc-13': '13.4.0', 'g++-13': '13.4.0'})
            if profile == 'native':
                output.update({'clang++-18': 'Ubuntu clang version 18.1.8', 'xvfb-run': '',
                               'Xvfb': '', 'xauth': '', 'glxinfo': ''})
                if repo == 'ignition':
                    output['pkg-config'] = '1.8.1'
        else:
            output.update(xcode='Xcode 26.3\nBuild version fixture', sdk='26.2', compiler='Apple clang')
        if repo == 'ice' and profile == 'native':
            output['rustc'] = 'rustc 1.90.0 (fixture)'
        return {name: {'executable': '/owned/' + name, 'output': text} for name, text in output.items()}

    def test_native_runtime_is_observed_independently_of_legacy_job_labels(self):
        for system in ('Linux', 'Darwin'):
            expected = self.observed(system)
            with self.subTest(system=system), patch.dict(os.environ, {
                    'RUNNER_OS': expected['runner_os'], 'RUNNER_ARCH': expected['architecture']}, clear=True), \
                    patch.object(owned.platform, 'system', return_value=system), \
                    patch.object(owned.platform, 'machine', return_value=expected['machine']), \
                    patch.object(owned.platform, 'release', return_value='fixture'), \
                    patch.object(owned.platform, 'python_version', return_value='3.12.15'), \
                    patch.object(owned.platform, 'mac_ver', return_value=('26.5.2', (), '')), \
                    patch.object(owned.platform, 'freedesktop_os_release', return_value={'VERSION_ID': '26.04'}):
                actual = owned.runtime()
                self.assertEqual(actual['release'], expected['release'])
                self.assertEqual(actual['architecture'], expected['architecture'])
                os.environ['RUNNER_OS'] = 'Windows'
                with self.assertRaisesRegex(ValueError, 'disagrees'):
                    owned.runtime()

    def test_missing_or_wrong_native_tool_version_fails_without_install_or_fallback(self):
        for repo, system, profile in [('ice', 'Linux', 'native'), ('ice', 'Linux', 'sanitizers'),
                                      ('ignition', 'Linux', 'native'), ('ice', 'Darwin', 'native')]:
            observed = self.observed(system); commands = self.commands(repo, system, profile)
            owned.validate_commands(commands, repo, profile, observed)
            for name in ('cmake', 'git'):
                with self.subTest(repo=repo, system=system, profile=profile, missing=name):
                    broken = copy.deepcopy(commands); broken.pop(name)
                    with self.assertRaises(ValueError):
                        owned.validate_commands(broken, repo, profile, observed)
            commands['cmake']['output'] = 'cmake version 9.9.9'
            with self.assertRaisesRegex(ValueError, 'version'):
                owned.validate_commands(commands, repo, profile, observed)
        with patch.object(owned.shutil, 'which', return_value=None), patch.object(owned.subprocess, 'run') as run:
            with self.assertRaisesRegex(ValueError, 'Missing preprovisioned'):
                owned.probe(['missing-compiler', '--version'])
            run.assert_not_called()

    def test_fork_and_wrong_workspace_reject_before_source_preparation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve(); event = root / 'event.json'
            event.write_text(json.dumps({'pull_request': {'head': {'repo': {'full_name': 'outsider/ice'}}}}))
            with patch.object(owned, 'ROOT', root), patch.dict(os.environ, {
                    'GITHUB_REPOSITORY': 'binofet/ice', 'GITHUB_WORKSPACE': str(root),
                    'GITHUB_EVENT_NAME': 'pull_request', 'GITHUB_EVENT_PATH': str(event)}, clear=True):
                with self.assertRaisesRegex(ValueError, 'Fork source'):
                    owned.source_scope('ice')
                event.write_text(json.dumps({'pull_request': {'head': {'repo': {'full_name': 'binofet/ice'}}}}))
                owned.source_scope('ice')
                os.environ['GITHUB_WORKSPACE'] = str(root / 'other')
                with self.assertRaisesRegex(ValueError, 'workspace'):
                    owned.source_scope('ice')

    def test_fresh_project_venv_pip_is_explicit_pinned_and_ignores_personal_pip_configuration(self):
        for repo in ('ice', 'ignition'):
            with self.subTest(repo=repo), tempfile.TemporaryDirectory() as directory:
                root = Path(directory).resolve(); venv = root / 'build/ci-tools/venv'; output = root / 'build/ci-evidence'
                output.mkdir(parents=True); (output / 'source.json').write_text('{"source":"fixture"}\n')
                requirements = root / owned.REQUIREMENTS[repo]; requirements.parent.mkdir(parents=True)
                requirements.write_text(''.join(name + '==' + version + '\n' for name, version in owned.PACKAGES.items()))
                environment = {'GITHUB_REPOSITORY': 'binofet/' + repo, 'GITHUB_WORKSPACE': str(root),
                    'GITHUB_EVENT_NAME': 'push', 'CI_VENV': str(venv), 'CI_EVIDENCE_DIR': str(output),
                    'PATH': '/owned/tools:/usr/bin:/bin', 'PIP_EXTRA_INDEX_URL': 'https://private.invalid/secret',
                    'PIP_USER': '1', 'PYTHONPATH': '/personal/site'}
                def command(argv, **kwargs):
                    if argv[1:3] == ['-m', 'venv']:
                        (venv / 'bin').mkdir(parents=True)
                    else:
                        self.assertEqual(argv[:4], [str(venv / 'bin/python3'), '-m', 'pip', 'install'])
                        self.assertIn('--require-virtualenv', argv)
                        self.assertIn('--no-cache-dir', argv)
                        self.assertNotIn('PIP_EXTRA_INDEX_URL', kwargs['env'])
                        self.assertNotIn('PIP_USER', kwargs['env'])
                        self.assertNotIn('PYTHONPATH', kwargs['env'])
                        self.assertEqual(kwargs['env']['PIP_CONFIG_FILE'], os.devnull)
                        self.assertEqual(kwargs['env']['PIP_REQUIRE_VIRTUALENV'], '1')
                with patch.object(owned, 'ROOT', root), patch.dict(os.environ, environment, clear=True), \
                        patch.object(owned, 'runtime', return_value=self.observed()), \
                        patch.object(owned, 'toolchain', return_value=self.commands(repo)), \
                        patch.object(owned.subprocess, 'run', side_effect=command) as run, \
                        patch.object(owned.subprocess, 'check_output', return_value=json.dumps(owned.PACKAGES)):
                    owned.prepare(repo, 'native')
                    self.assertEqual(run.call_count, 2)
                    metadata = json.loads((output / 'toolchain.json').read_text())
                    self.assertEqual(metadata['source_id'], owned.digest(output / 'source.json'))
                    self.assertEqual(metadata['packages'], owned.PACKAGES)
                    with self.assertRaisesRegex(ValueError, 'existing tool venv'):
                        owned.prepare(repo, 'native')
                    self.assertEqual(run.call_count, 2)

    def test_owned_evidence_is_required_source_bound_and_keeps_true_platform(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve(); output = root / 'evidence'; output.mkdir()
            requirement = root / owned.REQUIREMENTS['ice']; requirement.parent.mkdir(parents=True)
            requirement.write_text('fixture pins')
            source = {'runner_os': 'Linux', 'profile': 'native', 'owned_runtime': self.observed(),
                      'owned_repository': 'binofet/ice'}
            (output / 'source.json').write_text(json.dumps(source))
            metadata = {'format': 'ice-family-owned-toolchain-v1', 'repository': 'binofet/ice',
                        'profile': 'native', 'runtime': source['owned_runtime'], 'commands': self.commands(),
                        'source_id': owned.digest(output / 'source.json'), 'packages': dict(owned.PACKAGES),
                        'venv': 'build/ci-tools/venv', 'requirements_sha256': owned.digest(requirement)}
            with patch.object(owned, 'ROOT', root):
                self.assertTrue(owned.evidence_issues(output, source))
                (output / 'toolchain.json').write_text(json.dumps(metadata))
                self.assertEqual(owned.evidence_issues(output, source), [])
                for mutate in (lambda d: d.update(source_id='0' * 64),
                               lambda d: d['runtime'].update(architecture='ARM64'),
                               lambda d: d['packages'].update(ninja='unapproved'),
                               lambda d: d['commands'].pop('glxinfo'),
                               lambda d: d['commands']['clang++-18'].update(output='clang version 19.0.0'),
                               lambda d: d.update(repository='binofet/ignition')):
                    broken = copy.deepcopy(metadata); mutate(broken)
                    (output / 'toolchain.json').write_text(json.dumps(broken))
                    self.assertTrue(owned.evidence_issues(output, source))
            self.assertEqual(owned.evidence_issues(output, {'runner_os': 'Linux'}), [])

    def test_quality_verification_cannot_use_global_python_or_host_executables(self):
        with patch.object(owned.sys, 'prefix', '/same'), patch.object(owned.sys, 'base_prefix', '/same'):
            with self.assertRaisesRegex(ValueError, 'checkout-local venv'):
                owned.verify_quality('ice')
        root = owned.ROOT; venv = root / 'build/ci-tools/venv'
        with patch.object(owned.sys, 'prefix', str(venv)), patch.object(owned.sys, 'base_prefix', '/base'), \
                patch.object(owned.importlib.metadata, 'version', side_effect=lambda name: owned.PACKAGES[name]), \
                patch.object(owned.shutil, 'which', return_value='/host/clang-format'):
            with self.assertRaisesRegex(ValueError, 'escaped'):
                owned.verify_quality('ice')


class OwnedWorkflowTests(unittest.TestCase):
    def test_current_control_and_signal_sources_equal_maintained_templates(self):
        root = Path(__file__).resolve().parents[2]
        repo = 'ice' if (root / '.github/workflows/engine-quality.yml').is_file() else 'ignition'
        spec = importlib.util.spec_from_file_location('owned_workflow_gate_fixture', root / 'tools/ci/github/gate.py')
        gate = importlib.util.module_from_spec(spec); spec.loader.exec_module(gate)
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'rendered'; gate.generate_templates(repo, output)
            for name in ('family-gate.yml', 'family-review-signal.yml'):
                with self.subTest(generated=name):
                    self.assertEqual((output / name).read_text(), (root / '.github/workflows' / name).read_text())
                for path in (root / '.github/workflows' / name, root / 'tools/ci/github/templates' / name):
                    with self.subTest(control=str(path.relative_to(root))):
                        text = path.read_text()
                        self.assertIn('[self-hosted, Linux, X64, "${{ github.event.repository.name }}.control"]', text)
                        shells = [line.strip() for line in text.splitlines() if line.strip().startswith('shell:')]
                        self.assertEqual(shells, ['shell: /bin/bash --noprofile --norc -e -o pipefail {0}'])
                        self.assertNotIn('owned-shell', text)
                        self.assertNotIn('pull_request.head', text)

    def test_source_workflow_has_no_installer_or_hosted_fallback_and_activates_project_tools(self):
        root = Path(__file__).resolve().parents[2]
        path = root / '.github/workflows/engine-quality.yml'
        repo = 'ice' if path.is_file() else 'ignition'
        if repo == 'ignition':
            path = root / '.github/workflows/quality.yml'
        text = path.read_text()
        self.assertIn("github.event.pull_request.head.repo.full_name == github.repository", text)
        self.assertIn('runner: [self-hosted, Linux, X64, ' + repo + '.cpu]', text)
        self.assertIn('runner: [self-hosted, macOS, ARM64, ' + repo + '.cpu]', text)
        self.assertIn("matrix.os == 'macos-15' && '1' || '2'", text)
        self.assertIn('source "$CI_VENV/bin/activate"', text)
        self.assertIn('/opt/homebrew/bin/python3.12', text)
        self.assertIn('cpp: [20, 23]', text)
        shells = [line.strip() for line in text.splitlines() if line.strip().startswith('shell:')]
        self.assertEqual(shells, ['shell: /usr/local/libexec/ice-family-native-ci/owned-shell {0}']
                         * (2 if repo == 'ice' else 1))
        self.assertNotIn('sudo ', text)
        self.assertNotIn('apt-get', text)
        self.assertNotIn('rustup toolchain install', text)
        self.assertNotIn('actions/setup-python', text)
        self.assertNotIn('--parallel 4', text)


if __name__ == '__main__':
    unittest.main()
