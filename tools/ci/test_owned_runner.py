#!/usr/bin/env python3
"""Offline owned-runner contracts; no host setup, package installation or native build."""
import copy
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import tempfile
import unittest
from types import SimpleNamespace
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

    def test_nightly_event_authorization_requires_fixed_default_and_exact_manual_head(self):
        payload = {'repository': {'id': 40782766, 'fork': False, 'default_branch': 'master'},
                   'schedule': owned.NIGHTLY_CRON}
        self.assertEqual(owned.quality_plan('ice', 'schedule', payload, 'refs/heads/master'), 'nightly')
        for mutation in (lambda p: p['repository'].update(id=1), lambda p: p['repository'].update(fork=True),
                         lambda p: p['repository'].update(default_branch='main'),
                         lambda p: p.update(schedule='* * * * *')):
            bad = copy.deepcopy(payload); mutation(bad)
            with self.assertRaises(ValueError):
                owned.quality_plan('ice', 'schedule', bad, 'refs/heads/master')
        with self.assertRaises(ValueError):
            owned.quality_plan('ice', 'schedule', payload, 'refs/heads/feature')
        with self.assertRaises(ValueError):
            owned.quality_plan('ignition', 'schedule', payload, 'refs/heads/master')
        payload['inputs'] = dict(quality_plan='nightly', diagnostic='none', expected_head='a'*40)
        self.assertEqual(owned.quality_plan('ice', 'workflow_dispatch', payload, 'refs/heads/master', 'a'*40), 'nightly')
        for key, value in [('quality_plan', 'other'), ('quality_plan', None), ('diagnostic', 'xvfb-readiness-v1'),
                           ('expected_head', 'b'*40)]:
            bad = copy.deepcopy(payload); bad['inputs'][key] = value
            with self.assertRaises(ValueError):
                owned.quality_plan('ice', 'workflow_dispatch', bad, 'refs/heads/master', 'a'*40)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve(); event = root / 'event.json'
            event.write_text(json.dumps(payload))
            with patch.object(owned, 'ROOT', root), patch.dict(os.environ, {
                    'GITHUB_REPOSITORY': 'binofet/ice', 'GITHUB_WORKSPACE': str(root),
                    'GITHUB_EVENT_NAME': 'schedule', 'GITHUB_EVENT_PATH': str(event),
                    'GITHUB_REF': 'refs/heads/master'}, clear=True):
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
    def workflow_jobs(self, text):
        section = text.split('\njobs:\n', 1)[1]
        headers = list(re.finditer(r'^  ([a-z][a-z0-9-]*):\n', section, re.MULTILINE))
        names = [match.group(1) for match in headers]
        self.assertEqual(len(names), len(set(names)), 'Duplicate job owner')
        return {match.group(1): section[match.end():headers[index + 1].start()
                if index + 1 < len(headers) else len(section)]
                for index, match in enumerate(headers)}

    def job_condition(self, job):
        match = re.search(r'^    if: ([^\n]+)\n', job, re.MULTILINE)
        self.assertIsNotNone(match, 'An owned job requires explicit routing')
        value = match.group(1)
        if value == '>-':
            value = re.match(r'(?:      [^\n]*\n)+', job[match.end():]).group(0)
        return ' '.join(value.split())

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

    def test_fresh_uid_checkouts_preserve_git_ownership_and_exact_action_pin(self):
        root = Path(__file__).resolve().parents[2]
        engine = (root / '.github/workflows/engine-quality.yml').is_file()
        paths = [('.github/workflows/engine-quality.yml' if engine else '.github/workflows/quality.yml',
                  3 if engine else 1), ('.github/workflows/family-gate.yml', 1),
                 ('tools/ci/github/templates/family-gate.yml', 1)]
        for path, expected in paths:
            text = (root / path).read_text()
            if engine and path == '.github/workflows/engine-quality.yml':
                jobs = self.workflow_jobs(text)
                self.assertEqual(set(jobs), {'native', 'sanitizers', 'xvfb-readiness',
                                             'reject-invalid-manual-route'})
                for owner in ('native', 'sanitizers', 'xvfb-readiness'):
                    with self.subTest(checkout_owner=owner):
                        self.assertEqual(jobs[owner].count('uses: actions/checkout@'), 1)
                self.assertNotIn('uses: actions/checkout@', jobs['reject-invalid-manual-route'])
                self.assertIn('ref: ${{ github.sha }}', jobs['xvfb-readiness'])
            blocks = re.findall(r'^        uses: (actions/checkout@[a-f0-9]{40})[^\n]*\n'
                                r'        with:\n((?:          [^\n]*\n)+)', text, re.MULTILINE)
            self.assertEqual(len(blocks), expected)
            self.assertEqual(text.count('uses: actions/checkout@'), expected)
            for index, (action, inputs) in enumerate(blocks):
                with self.subTest(checkout=path, index=index):
                    self.assertEqual(action, 'actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1')
                    values = dict(line.strip().split(': ', 1) for line in inputs.splitlines())
                    self.assertEqual(values.get('set-safe-directory'), 'false')
                    self.assertEqual(values.get('persist-credentials'), 'false')
                    self.assertEqual(values.get('submodules'), 'false')

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
        self.assertIn('source "$CI_VENV/bin/activate"', text)
        self.assertIn('/opt/homebrew/bin/python3.12', text)
        self.assertIn('cpp: [20, 23]', text)
        expected_shell = 'shell: /usr/local/libexec/ice-family-native-ci/owned-shell {0}'
        if repo == 'ice':
            jobs = self.workflow_jobs(text)
            self.assertEqual(set(jobs), {'native', 'sanitizers', 'xvfb-readiness',
                                         'reject-invalid-manual-route'})
            for owner, block in jobs.items():
                with self.subTest(source_owner=owner):
                    shells = [line.strip() for line in block.splitlines()
                              if line.strip().startswith('shell:')]
                    self.assertEqual(shells, [expected_shell])
            for owner in ('native', 'sanitizers'):
                for variable in ('CI_PARALLEL', 'CMAKE_BUILD_PARALLEL_LEVEL'):
                    with self.subTest(worker_budget_owner=owner, variable=variable):
                        self.assertEqual(re.findall(r'^      ' + variable + r": (.+)$",
                                                    jobs[owner], re.MULTILINE),
                                         ["${{ matrix.os == 'macos-15' && '10' || '2' }}" if owner == 'native' else "'2'"])
                self.assertIn('source "$CI_VENV/bin/activate"', jobs[owner])
                self.assertIn('tools/ci/test_evidence.py', jobs[owner])
        else:
            self.assertIn("matrix.os == 'macos-15' && '1' || '2'", text)
            shells = [line.strip() for line in text.splitlines() if line.strip().startswith('shell:')]
            self.assertEqual(shells, [expected_shell])
        self.assertNotIn('sudo ', text)
        self.assertNotIn('apt-get', text)
        self.assertNotIn('rustup toolchain install', text)
        self.assertNotIn('actions/setup-python', text)
        self.assertNotIn('--parallel 4', text)


    def test_ice_manual_diagnostic_route_is_exact_and_cannot_replace_normal_quality(self):
        root = Path(__file__).resolve().parents[2]
        path = root / '.github/workflows/engine-quality.yml'
        if not path.is_file():
            return  # Ignition has no ICE diagnostic route; original ownership checks still apply.
        jobs = self.workflow_jobs(path.read_text())
        fixed = "'refs/heads/codex/xvfb-readiness-20261019'"
        normal = "(inputs.quality_plan == 'day' && inputs.diagnostic == 'none' && inputs.expected_head == '' && github.ref != " + fixed + ")"
        actual_diagnostic = ("github.actor == 'binofet' && github.run_attempt == 1 && "
            "inputs.quality_plan == 'day' && inputs.diagnostic == 'xvfb-readiness-v1' && inputs.expected_head == github.sha && github.ref == " + fixed)
        repository = ("github.repository == 'binofet/ice' && github.event.repository.id == 40782766 && "
            "github.event.repository.fork == false && github.event_name == 'workflow_dispatch'")
        expected_diagnostic = (repository + " && github.actor == 'binofet' && github.run_attempt == 1 && "
            "github.ref == " + fixed + " && inputs.quality_plan == 'day' && inputs.diagnostic == 'xvfb-readiness-v1' && inputs.expected_head == github.sha")
        nightly = ("(inputs.quality_plan == 'nightly' && inputs.diagnostic == 'none' && inputs.expected_head == github.sha && "
                   "github.event.repository.default_branch == 'master' && github.ref == 'refs/heads/master')")
        expected_refusal = repository + " && !(" + normal + " || " + nightly + " || (" + actual_diagnostic + "))"
        self.assertEqual(self.job_condition(jobs['xvfb-readiness']), expected_diagnostic)
        self.assertEqual(self.job_condition(jobs['reject-invalid-manual-route']), expected_refusal)
        refusal = jobs['reject-invalid-manual-route']
        self.assertIn('exit 64', refusal)
        for forbidden in ('actions/checkout@', 'run_stage.py', 'xvfb_readiness.py', 'cmake', 'glxinfo'):
            self.assertNotIn(forbidden, refusal)
        diagnostic = jobs['xvfb-readiness']
        self.assertIn('os.getuid() == os.geteuid() == 984', diagnostic)
        self.assertIn('native_quality_qualified=False', diagnostic)
        self.assertIn('tools/ci/xvfb_readiness.py', diagnostic)
        self.assertNotIn('tools/ci/report.py summary', diagnostic)
        self.assertNotIn('tools/ci/report.py publication', diagnostic)

    def test_day_nightly_routes_are_default_bound_and_keep_existing_profiles(self):
        text = (Path(__file__).resolve().parents[2] / '.github/workflows/engine-quality.yml').read_text()
        jobs = self.workflow_jobs(text)
        self.assertIn("cron: '17 9 * * *'", text)
        self.assertIn("&& 'nightly' || 'day'", text.split('jobs:')[0])
        self.assertEqual(self.job_condition(jobs['native']), self.job_condition(jobs['sanitizers']))
        def admitted(event, plan='day', ref='refs/heads/master', expected='', diagnostic='none',
                     fork=False, default='master', same_repo=True):
            repository = SimpleNamespace(id=40782766, fork=fork, default_branch=default)
            pull = SimpleNamespace(head=SimpleNamespace(repo=SimpleNamespace(full_name='binofet/ice' if same_repo else 'fork/ice')))
            github = SimpleNamespace(repository='binofet/ice', event_name=event, ref=ref, sha='a'*40,
                actor='binofet', run_attempt=1,
                event=SimpleNamespace(repository=repository, pull_request=pull, schedule='17 9 * * *'))
            inputs = SimpleNamespace(quality_plan=plan, diagnostic=diagnostic, expected_head=expected)
            values = {}
            for name in jobs:
                condition = self.job_condition(jobs[name]).replace('&&', ' and ').replace('||', ' or ').replace('!(', 'not (')
                condition = re.sub(r'\bfalse\b', 'False', condition)
                values[name] = bool(eval(condition, {'__builtins__': {}}, dict(github=github, inputs=inputs)))
            return values
        for event in ('push', 'pull_request', 'schedule'):
            self.assertTrue(admitted(event)['native'])
        self.assertTrue(admitted('workflow_dispatch')['native'])
        self.assertTrue(admitted('workflow_dispatch', 'nightly', expected='a'*40)['native'])
        for values in (admitted('schedule', ref='refs/heads/feature'), admitted('schedule', fork=True),
                       admitted('schedule', default='main'), admitted('pull_request', same_repo=False),
                       admitted('workflow_dispatch', 'nightly', expected='b'*40),
                       admitted('workflow_dispatch', 'nightly', ref='refs/heads/feature', expected='a'*40)):
            self.assertFalse(values['native'] or values['sanitizers'] or values['xvfb-readiness'])
        values = admitted('workflow_dispatch', diagnostic='xvfb-readiness-v1',
                          ref='refs/heads/codex/xvfb-readiness-20261019', expected='a'*40)
        self.assertTrue(values['xvfb-readiness'])
        self.assertFalse(values['native'] or values['reject-invalid-manual-route'])
        values = admitted('workflow_dispatch', 'nightly', diagnostic='xvfb-readiness-v1',
                          ref='refs/heads/codex/xvfb-readiness-20261019', expected='a'*40)
        self.assertTrue(values['reject-invalid-manual-route'])
        self.assertFalse(values['native'] or values['xvfb-readiness'])
        lint = jobs['native'].split('      - name: Pinned static analysis')[1].split('      # Each executable')[0]
        documentation = jobs['native'].split('      - name: Public API documentation and argument contracts')[1].split('      - name: Pinned static analysis')[0]
        conditions = lambda block: [line.strip() for line in block.splitlines() if line.strip().startswith('if:')]
        self.assertEqual(conditions(documentation), conditions(lint))
        self.assertIn('run_stage.py api -- python3 tools/quality/api.py', documentation)
        self.assertIn("github.event_name == 'schedule'", lint)
        self.assertIn("inputs.quality_plan == 'nightly'", lint)
        for build in ('build/ci', 'build/metagen-ci'):
            self.assertIn('/usr/bin/time -l cmake --build '+build+' --parallel "$CI_PARALLEL"', jobs['native'])


if __name__ == '__main__':
    unittest.main()
