"""Offline wheel trust and interrupted-acquisition fixtures; no network or host cache."""
import errno
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import owned_runner as owned
import test_owned_runner

class QualityWheelTests(unittest.TestCase):
    def wheel_fixture(self, root):
        rows, bodies = [], {}
        for package, version in owned.PACKAGES.items():
            filename = package.replace('-', '_') + '-' + version + '-py3-none-manylinux2014_x86_64.whl'
            body = (package + ':fixture').encode()
            rows.append(dict(package=package, version=version, filename=filename,
                             bytes=len(body), sha256=hashlib.sha256(body).hexdigest()))
            bodies[filename] = body
        manifest = root / owned.WHEEL_MANIFEST
        manifest.parent.mkdir(parents=True)
        manifest.write_text(json.dumps(dict(format='linux-x64-quality-wheels-v1', wheels=rows)))
        (root / 'build/ci-tools').mkdir(parents=True)
        return rows, bodies

    def test_linux_wheels_resume_completed_downloads_then_install_offline_with_one_budget(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); rows, bodies = self.wheel_fixture(root)
            home = root / 'home'; home.mkdir()
            calls = []; clock = [0.0]; fail = [True]
            def pip(argv, **kwargs):
                calls.append((argv, kwargs['timeout']))
                self.assertIn('--no-deps', argv); self.assertIn('--require-hashes', argv)
                self.assertIn('--only-binary=:all:', argv)
                clock[0] += 100
                if argv[3] == 'download':
                    self.assertEqual(argv[argv.index('--index-url') + 1], 'https://pypi.org/simple')
                    package = Path(argv[-1]).read_text().split('==')[0]
                    row = next(row for row in rows if row['package'] == package)
                    if package == 'ninja' and fail[0]:
                        raise subprocess.TimeoutExpired(argv, kwargs['timeout'])
                    destination = Path(argv[argv.index('--dest') + 1])
                    (destination / row['filename']).write_bytes(bodies[row['filename']])
                else:
                    self.assertEqual(argv[3], 'install')
                    self.assertIn('--no-index', argv); self.assertNotIn('--index-url', argv)
                    self.assertEqual(len(Path(argv[-1]).read_text().splitlines()), 4)
            with patch.object(owned, 'ROOT', root), patch.object(owned.Path, 'home', return_value=home), \
                    patch.object(owned.time, 'monotonic', side_effect=lambda: clock[0]), \
                    patch.object(owned.subprocess, 'run', side_effect=pip):
                with self.assertRaises(subprocess.TimeoutExpired):
                    owned.install_linux_quality('/fixture/python', {}, 600)
                self.assertEqual([timeout for _, timeout in calls], [600, 500, 400])
                cache = home / '.cache/ice-quality-wheels-v1' / hashlib.sha256(os.environ.get('RUNNER_NAME', 'standalone').encode()).hexdigest()[:16]
                self.assertEqual({p.name for p in cache.iterdir()}, {r['filename'] for r in rows[:2]})
                fail[0] = False; calls.clear(); clock[0] = 0
                evidence = owned.install_linux_quality('/fresh-venv/python', {}, 600)
                self.assertEqual(evidence['cache_hits'], ['clang-format', 'clang-tidy'])
                self.assertEqual([argv[3] for argv, _ in calls], ['download', 'download', 'install'])
                self.assertEqual([timeout for _, timeout in calls], [600, 500, 400])
                calls.clear(); clock[0] = 0
                evidence = owned.install_linux_quality('/another-fresh-venv/python', {}, 600)
                self.assertEqual(evidence['cache_hits'], list(owned.PACKAGES))
                self.assertEqual([argv[3] for argv, _ in calls], ['install'])

    def test_linux_cache_corruption_symlink_and_unexpected_files_fail_before_pip(self):
        for damage in ('corrupt', 'symlink', 'extra', 'cache-symlink'):
            with self.subTest(damage=damage), tempfile.TemporaryDirectory() as directory:
                root = Path(directory); rows, bodies = self.wheel_fixture(root)
                home = root / 'home'; cache = home / '.cache/ice-quality-wheels-v1' / hashlib.sha256(os.environ.get('RUNNER_NAME', 'standalone').encode()).hexdigest()[:16]
                cache.mkdir(parents=True)
                wheel = cache / rows[0]['filename']
                if damage == 'corrupt':
                    wheel.write_bytes(b'x' * rows[0]['bytes'])
                elif damage == 'symlink':
                    target = root / 'body'; target.write_bytes(bodies[rows[0]['filename']]); wheel.symlink_to(target)
                elif damage == 'extra':
                    (cache / 'unreviewed.whl').write_bytes(b'unknown')
                else:
                    cache.rmdir(); cache.symlink_to(root)
                with patch.object(owned, 'ROOT', root), patch.object(owned.Path, 'home', return_value=home), \
                        patch.object(owned.subprocess, 'run') as run:
                    with self.assertRaises(ValueError):
                        owned.install_linux_quality('/fixture/python', {}, 600)
                    run.assert_not_called()

    def test_linux_cache_publication_stays_on_home_filesystem_when_checkout_is_elsewhere(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / 'checkout'; root.mkdir()
            rows, bodies = self.wheel_fixture(root)
            home = Path(directory) / 'other-filesystem-home'; home.mkdir()
            cache = home / '.cache/ice-quality-wheels-v1' / hashlib.sha256(os.environ.get('RUNNER_NAME', 'standalone').encode()).hexdigest()[:16]
            original_replace = Path.replace
            publications = []
            def replace(source, target):
                if source.is_relative_to(root) != target.is_relative_to(root):
                    raise OSError(errno.EXDEV, 'Simulated distinct checkout and HOME filesystems')
                self.assertTrue(source.is_relative_to(cache))
                self.assertTrue(target.is_relative_to(cache))
                publications.append(target.name)
                return original_replace(source, target)
            def pip(argv, **kwargs):
                if argv[3] == 'download':
                    package = Path(argv[-1]).read_text().split('==')[0]
                    row = next(row for row in rows if row['package'] == package)
                    destination = Path(argv[argv.index('--dest') + 1])
                    (destination / row['filename']).write_bytes(bodies[row['filename']])
            with patch.object(owned, 'ROOT', root), patch.object(owned.Path, 'home', return_value=home), \
                    patch.object(owned.Path, 'replace', replace), \
                    patch.object(owned.time, 'monotonic', return_value=0), \
                    patch.object(owned.subprocess, 'run', side_effect=pip):
                owned.install_linux_quality('/fixture/python', {}, 600)
            self.assertEqual(publications, [r['filename'] for r in rows])
            self.assertEqual({p.name for p in cache.iterdir()}, set(publications))

    def test_linux_download_must_match_exact_wheel_and_expired_budget_cannot_restart(self):
        for failure in ('bad-hash', 'unexpected-wheel', 'expired'):
            with self.subTest(failure=failure), tempfile.TemporaryDirectory() as directory:
                root = Path(directory); rows, bodies = self.wheel_fixture(root)
                home = root / 'home'; home.mkdir()
                def pip(argv, **kwargs):
                    destination = Path(argv[argv.index('--dest') + 1])
                    filename = rows[0]['filename'] if failure == 'bad-hash' else 'unexpected.whl'
                    (destination / filename).write_bytes(b'bad')
                with patch.object(owned, 'ROOT', root), patch.object(owned.Path, 'home', return_value=home), \
                        patch.object(owned.time, 'monotonic', return_value=600 if failure == 'expired' else 0), \
                        patch.object(owned.subprocess, 'run', side_effect=pip) as run:
                    with self.assertRaises(TimeoutError if failure == 'expired' else ValueError):
                        owned.install_linux_quality('/fixture/python', {}, 600)
                    self.assertEqual(run.call_count, 0 if failure == 'expired' else 1)
                self.assertEqual(list((home / '.cache/ice-quality-wheels-v1' / hashlib.sha256(os.environ.get('RUNNER_NAME', 'standalone').encode()).hexdigest()[:16]).iterdir()), [])

    def test_linux_prepare_preserves_isolation_and_never_records_failed_dependencies(self):
        for failed in (False, True):
            with self.subTest(failed=failed), tempfile.TemporaryDirectory() as directory:
                root = Path(directory).resolve()
                venv = root / 'build/ci-tools/venv'
                output = root / 'evidence'; output.mkdir()
                (output / 'source.json').write_text('{}')
                requirements = root / owned.REQUIREMENTS['ice']
                requirements.parent.mkdir(parents=True)
                requirements.write_text(''.join(k + '==' + v + '\n' for k, v in owned.PACKAGES.items()))
                fixture = test_owned_runner.OwnedRunnerTests()
                environment = {'CI_VENV': str(venv), 'CI_EVIDENCE_DIR': str(output), 'PATH': '/usr/bin',
                               'PIP_EXTRA_INDEX_URL': 'https://untrusted.invalid', 'PYTHONPATH': '/untrusted'}
                def acquisition(python, env, deadline):
                    self.assertEqual(python, venv / 'bin/python3')
                    self.assertNotIn('PIP_EXTRA_INDEX_URL', env)
                    self.assertNotIn('PYTHONPATH', env)
                    self.assertEqual(env['PIP_CONFIG_FILE'], os.devnull)
                    self.assertEqual(deadline, 600)
                    if failed:
                        raise subprocess.CalledProcessError(2, ['fixture'])
                    return {'fixture': 'verified-wheels'}
                with patch.object(owned, 'ROOT', root), patch.dict(os.environ, environment, clear=True), \
                     patch.object(owned, 'source_scope'), patch.object(owned, 'runtime', return_value=fixture.observed()), \
                     patch.object(owned, 'toolchain', return_value=fixture.commands()), \
                     patch.object(owned.subprocess, 'run'), patch.object(owned.time, 'monotonic', return_value=0), \
                     patch.object(owned, 'install_linux_quality', side_effect=acquisition), \
                     patch.object(owned.subprocess, 'check_output', return_value=json.dumps(owned.PACKAGES)) as versions:
                    if failed:
                        with self.assertRaises(subprocess.CalledProcessError):
                            owned.prepare('ice', 'native')
                        versions.assert_not_called()
                        self.assertFalse((output / 'toolchain.json').exists())
                    else:
                        owned.prepare('ice', 'native')
                        self.assertEqual(json.loads((output / 'toolchain.json').read_text())['wheel_preparation'],
                                         {'fixture': 'verified-wheels'})

    def test_interrupted_wheel_retries_only_missing_package_under_shared_deadline(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); rows, bodies = self.wheel_fixture(root)
            home = root / 'home'; home.mkdir()
            calls = []; failed = [False]; clock = [0]
            def pip(argv, **kwargs):
                clock[0] += 10
                if argv[3] == 'download':
                    package = Path(argv[-1]).read_text().split('==')[0]
                    calls.append(package)
                    if package == 'clang-tidy' and not failed[0]:
                        failed[0] = True
                        raise subprocess.CalledProcessError(2, argv, output='BrokenPipeError ProtocolError')
                    row = next(r for r in rows if r['package'] == package)
                    (Path(argv[argv.index('--dest') + 1]) / row['filename']).write_bytes(bodies[row['filename']])
            with patch.object(owned, 'ROOT', root), patch.object(owned.Path, 'home', return_value=home), \
                 patch.object(owned.time, 'monotonic', side_effect=lambda: clock[0]), \
                 patch.object(owned.subprocess, 'run', side_effect=pip):
                result = owned.install_linux_quality('/fixture/python', {}, 600)
            self.assertEqual(calls, ['clang-format', 'clang-tidy', 'clang-tidy', 'ninja', 'libclang'])
            self.assertEqual(result['budget_seconds'], 600)

if __name__ == '__main__':
    unittest.main()
