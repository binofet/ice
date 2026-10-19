#!/usr/bin/env python3
"""Offline preflight fixtures: no host graphics lookup, ELF loading or display process."""
import base64
from contextlib import ExitStack, redirect_stdout
import hashlib
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import xvfb_readiness as diagnostic


class ExecBoundary(BaseException):
    """Stop at the exec boundary without starting any process."""


class XvfbReadinessTests(unittest.TestCase):
    def setUp(self):
        self.stack = ExitStack()
        self.addCleanup(self.stack.close)
        self.root = Path(self.stack.enter_context(tempfile.TemporaryDirectory()))
        self.output = self.root / 'evidence'
        self.output.mkdir()
        self.descriptor = self.root / '50_mesa.json'
        self.document = {'file_format_version': '1.0.0',
                         'ICD': {'library_path': 'libEGL_mesa.so.0'}}
        self.write_descriptor(self.document)
        names = {'mesa_egl': 'libEGL_mesa.so.0', 'egl_loader': 'libEGL.so.1',
                 'xvfb_run': 'xvfb-run', 'xvfb': 'Xvfb', 'xauth': 'xauth',
                 'xdpyinfo': 'xdpyinfo', 'glxinfo': 'glxinfo'}
        self.paths = {name: self.root / filename for name, filename in names.items()}
        for name, path in self.paths.items():
            # Header-shaped data only. Nothing is executable native code or loaded.
            raw = bytearray(64)
            raw[:6] = b'\x7fELF\x02\x01'
            raw[18:20] = b'\x3e\x00'
            path.write_bytes(b'#!/bin/sh\n# fixture only\n' if name == 'xvfb_run' else raw)
            path.chmod(0o755)
        self.actual_observe = diagnostic.observe_file

        def observe_fixture(path, maximum):
            raw, record = self.actual_observe(path, maximum)
            # Simulated package-owned metadata, never represented as host evidence.
            record['uid'] = 0
            return raw, record

        self.observe_fixture = observe_fixture
        self.stack.enter_context(patch.object(diagnostic, 'MESA_JSON', self.descriptor))
        self.stack.enter_context(patch.object(diagnostic, 'SYSTEM_PATHS', self.paths))
        self.observer = self.stack.enter_context(patch.object(
            diagnostic, 'observe_file', side_effect=observe_fixture))
        self.stack.enter_context(patch.object(diagnostic.os, 'getuid', return_value=984))
        self.stack.enter_context(patch.object(diagnostic.os, 'geteuid', return_value=984))
        self.stack.enter_context(patch.object(diagnostic.os, 'getgid', return_value=973))
        self.stack.enter_context(patch.object(diagnostic.os, 'getegid', return_value=973))
        self.actual_packages = diagnostic.package_evidence
        self.stack.enter_context(patch.object(diagnostic, 'package_evidence', return_value={
            'available': False, 'error_class': 'FileNotFoundError'}))
        self.stack.enter_context(patch.object(diagnostic.shutil, 'which', side_effect=lambda name, **_:
            str(self.paths['xvfb' if name == 'Xvfb' else 'xauth'])))
        self.stack.enter_context(patch.dict(os.environ, {
            'CI_EVIDENCE_DIR': str(self.output), 'DIAG_EXPECTED_HEAD': 'a' * 40,
            'PATH': '/fixture/bin', 'SECRET_TOKEN': 'must-never-be-recorded'}, clear=True))
        self.execve = self.stack.enter_context(patch.object(
            diagnostic.os, 'execve', side_effect=ExecBoundary))
        self.stack.enter_context(redirect_stdout(io.StringIO()))

    def write_descriptor(self, document):
        self.descriptor.write_text(json.dumps(document))

    def record(self):
        return json.loads((self.output / 'xvfb-preflight.json').read_text())

    def remove_record(self):
        (self.output / 'xvfb-preflight.json').unlink(missing_ok=True)

    def refused(self, reason):
        self.execve.reset_mock()
        self.assertEqual(diagnostic.preflight_and_run(), 64)
        self.execve.assert_not_called()
        record = self.record()
        self.assertEqual(record['status'], 'failed')
        self.assertEqual(record['reason'], reason)
        self.assertFalse(record['graphics_initialized'])
        self.assertNotIn('must-never-be-recorded', json.dumps(record))
        return record

    def test_ready_records_actual_fixture_bytes_and_sets_selector_before_server_exec(self):
        os.environ.update(__EGL_VENDOR_LIBRARY_FILENAMES='/inherited/nvidia.json',
                          MESA_GL_VERSION_OVERRIDE='9.9', MESA_GLSL_VERSION_OVERRIDE='999',
                          MESA_GLES_VERSION_OVERRIDE='9.9')
        with self.assertRaises(ExecBoundary):
            diagnostic.preflight_and_run()
        executable, argv, environment = self.execve.call_args.args
        self.assertEqual(executable, '/usr/bin/xvfb-run')
        self.assertEqual(argv, diagnostic.display_command())
        self.assertEqual(environment['__EGL_VENDOR_LIBRARY_FILENAMES'], str(self.descriptor))
        self.assertEqual(environment['LIBGL_ALWAYS_SOFTWARE'], '1')
        self.assertEqual(environment['LP_NUM_THREADS'], '2')
        self.assertTrue(all(name not in environment for name in diagnostic.OVERRIDES))
        record = self.record()
        self.assertEqual(record['status'], 'ready')
        raw = self.descriptor.read_bytes()
        self.assertEqual(base64.b64decode(record['descriptor']['actual_bytes_base64']), raw)
        self.assertEqual(record['descriptor']['sha256'], hashlib.sha256(raw).hexdigest())
        self.assertEqual(record['files']['mesa_egl']['sha256'],
                         hashlib.sha256(self.paths['mesa_egl'].read_bytes()).hexdigest())
        self.assertEqual(set(record['effective_selectors']), set(diagnostic.SELECTORS))
        self.assertEqual(record['inherited_selectors']['__EGL_VENDOR_LIBRARY_FILENAMES'],
                         '/inherited/nvidia.json')
        self.assertEqual(record['credentials'], dict(uid=984, euid=984, gid=973, egid=973))
        self.assertFalse(record['packages']['available'])
        self.assertIn('no loaded-library or renderer proof', record['library_identity_limit'])
        self.assertLess((self.output / 'xvfb-preflight.json').stat().st_size, diagnostic.MAX_RECORD)
        self.assertNotIn('must-never-be-recorded', json.dumps(record))

    def test_missing_and_unreadable_descriptor_stop_before_server(self):
        self.descriptor.unlink()
        self.refused('FileNotFoundError')
        self.remove_record()
        with patch.object(diagnostic, 'observe_file', side_effect=PermissionError):
            self.refused('PermissionError')

    def test_malformed_duplicate_nonfinite_and_deep_json_stop_before_server(self):
        for raw in (b'{bad', b'\xff', b'{"ICD":{},"ICD":{}}', b'{"x":NaN}',
                    b'[' * 1100 + b']' * 1100):
            with self.subTest(raw=raw[:20]):
                self.descriptor.write_bytes(raw)
                self.refused('malformed-mesa-json')
                self.remove_record()

    def test_wrong_format_and_library_identity_stop_before_server(self):
        documents = [([], 'mesa-json-format-mismatch'),
                     ({'file_format_version': '2.0.0', 'ICD': {}}, 'mesa-json-format-mismatch'),
                     ({'file_format_version': '1.0.0', 'ICD': []}, 'mesa-json-format-mismatch')]
        for library in (None, 42, 'libEGL_nvidia.so.0', './libEGL_mesa.so.0',
                        str(self.root / 'other' / 'libEGL_mesa.so.0')):
            documents.append((dict(file_format_version='1.0.0', ICD=dict(library_path=library)),
                              'mesa-library-path-mismatch'))
        for document, reason in documents:
            with self.subTest(document=document):
                self.write_descriptor(document)
                self.refused(reason)
                self.remove_record()

    def test_absolute_matching_mesa_library_is_allowed(self):
        self.document['ICD']['library_path'] = str(self.paths['mesa_egl'])
        self.write_descriptor(self.document)
        with self.assertRaises(ExecBoundary):
            diagnostic.preflight_and_run()
        self.assertEqual(self.record()['descriptor']['library_path'], str(self.paths['mesa_egl']))

    def test_descriptor_and_image_size_bounds_stop_before_server(self):
        self.descriptor.write_bytes(b' ' * (diagnostic.MAX_DESCRIPTOR + 1))
        self.refused('file-size-bound')
        self.remove_record()
        self.write_descriptor(self.document)
        self.paths['mesa_egl'].write_bytes(b'x' * 65)
        with patch.object(diagnostic, 'MAX_FILE', 64):
            self.refused('file-size-bound')

    def test_wrong_account_or_gid_refuses_before_any_file_probe(self):
        for function, value, reason in (('getuid', 1000, 'owned-account-mismatch'),
                                        ('geteuid', 0, 'owned-account-mismatch'),
                                        ('getegid', 0, 'gid-egid-mismatch')):
            with self.subTest(function=function), patch.object(diagnostic.os, function, return_value=value):
                self.observer.reset_mock()
                self.refused(reason)
                self.observer.assert_not_called()
                self.remove_record()

    def test_conflicting_or_unbounded_selector_stops_before_file_probe(self):
        for name in diagnostic.CONFLICTS:
            with self.subTest(name=name), patch.dict(os.environ, {name: 'conflicting-fixture'}):
                self.observer.reset_mock()
                self.refused('conflicting-selector')
                self.observer.assert_not_called()
                self.remove_record()
        for value in ('x' * 257, 'line\nbreak'):
            with patch.dict(os.environ, {'__EGL_VENDOR_LIBRARY_DIRS': value}):
                self.refused('selector-value-bound-or-encoding')
                self.remove_record()

    def test_untrusted_or_setid_system_image_stops_before_server(self):
        for mutate, reason in ((lambda record: record.update(uid=1000), 'untrusted-system-file'),
                               (lambda record: record.update(mode='0o666'), 'untrusted-system-file'),
                               (lambda record: record.update(mode='0o4755'), 'setid-system-file')):
            def observe(path, maximum):
                raw, record = self.observe_fixture(path, maximum)
                if path == self.paths['xvfb']:
                    mutate(record)
                return raw, record
            with self.subTest(reason=reason), patch.object(diagnostic, 'observe_file', side_effect=observe):
                self.refused(reason)
                self.remove_record()

    def test_missing_or_wrong_arch_image_stops_before_server(self):
        self.paths['mesa_egl'].write_bytes(b'not an ELF image')
        self.refused('elf-architecture-mismatch')
        self.remove_record()
        self.paths['mesa_egl'].unlink()
        self.refused('FileNotFoundError')

    def test_package_metadata_is_whitelisted_optional_and_never_binary_version_proof(self):
        status = self.root / 'status'
        status.write_text('Package: libegl-mesa0\nVersion: fixture-version\n'
                          'Architecture: amd64\nStatus: install ok installed\n'
                          'Description: must-never-be-recorded\n\n'
                          'Package: unrelated\nVersion: must-never-be-recorded\n')
        def observe(path, maximum):
            self.assertEqual(path, Path('/var/lib/dpkg/status'))
            self.assertEqual(maximum, diagnostic.MAX_FILE)
            return self.observe_fixture(status, maximum)
        with patch.object(diagnostic, 'observe_file', side_effect=observe):
            result = self.actual_packages()
        self.assertTrue(result['available'])
        self.assertEqual(result['entries'], [dict(Package='libegl-mesa0', Version='fixture-version',
            Architecture='amd64', Status='install ok installed')])
        self.assertIn('not binary version or load proof', result['claim'])
        self.assertNotIn('must-never-be-recorded', json.dumps(result))
        with patch.object(diagnostic, 'observe_file', side_effect=FileNotFoundError):
            self.assertEqual(self.actual_packages(), dict(available=False, error_class='FileNotFoundError'))
        self.execve.assert_not_called()

    def test_path_shadowing_and_helper_format_mismatch_stop_before_server(self):
        with patch.object(diagnostic.shutil, 'which', return_value=None):
            self.refused('helper-path-mismatch')
        self.remove_record()
        self.paths['xvfb_run'].write_text('not a shell script')
        self.refused('helper-format-mismatch')

    def test_nonexecutable_helper_stops_before_server(self):
        self.paths['xvfb'].chmod(0o644)
        self.refused('helper-not-executable')

    def test_existing_record_and_oversized_record_cannot_reach_exec(self):
        target = self.output / 'xvfb-preflight.json'
        target.write_bytes(b'original evidence')
        with self.assertRaises(FileExistsError):
            diagnostic.preflight_and_run()
        self.execve.assert_not_called()
        self.assertEqual(target.read_bytes(), b'original evidence')
        target.unlink()
        with patch.object(diagnostic, 'MAX_RECORD', 1), self.assertRaisesRegex(
                diagnostic.PreflightError, 'preflight-record-bound'):
            diagnostic.preflight_and_run()
        self.execve.assert_not_called()
        self.assertFalse(target.exists())

    def test_preflight_is_inside_original_deadline_and_preserves_client_failure_order(self):
        with patch.object(diagnostic.sys, 'platform', 'linux'), \
                patch.object(diagnostic.sys, 'argv', ['xvfb_readiness.py']), self.assertRaises(ExecBoundary):
            diagnostic.main()
        _, argv, _ = self.execve.call_args.args
        stage = argv.index('--timeout-seconds')
        self.assertEqual(argv[stage:stage + 4], ['--timeout-seconds', '60', 'xvfb-readiness', '--'])
        self.assertEqual(argv[-6:-2], [diagnostic.sys.executable, '-I', '-S', '-B'])
        self.assertEqual(argv[-1], '--preflight-and-run')
        self.assertNotIn('/usr/bin/xvfb-run', argv)
        self.assertIn('exit "$readiness_status"', diagnostic.CHECK)
        self.assertLess(diagnostic.CHECK.index('exit "$readiness_status"'),
                        diagnostic.CHECK.index('/usr/bin/glxinfo -B'))
        self.assertIn('exit "$glxinfo_status"', diagnostic.CHECK)


if __name__ == '__main__':
    unittest.main()
