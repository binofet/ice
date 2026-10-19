#!/usr/bin/env python3
"""Normal render boundary fixtures; never execute a stage, display or native binary."""
from contextlib import ExitStack, redirect_stderr, redirect_stdout
import io
import json
import os
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest
from unittest.mock import patch

import render_checks as render


class RenderChecksTests(unittest.TestCase):
    def setUp(self):
        self.stack = ExitStack()
        self.addCleanup(self.stack.close)
        self.root = Path(self.stack.enter_context(tempfile.TemporaryDirectory()))
        self.descriptor = self.root / '50_mesa.json'
        self.descriptor.write_text(json.dumps({'file_format_version': '1.0.0',
                                              'ICD': {'library_path': 'libEGL_mesa.so.0'}}))
        original = render.xvfb.observe_file

        def observe_fixture(path, maximum):
            self.assertEqual(path, self.descriptor)
            raw, record = original(path, maximum)
            record['uid'] = 0  # Simulated package-owned metadata, not a host observation.
            return raw, record

        self.stack.enter_context(patch.object(render.xvfb, 'MESA_JSON', self.descriptor))
        self.stack.enter_context(patch.object(render.xvfb, 'observe_file', side_effect=observe_fixture))
        self.full_preflight = self.stack.enter_context(patch.object(render.xvfb, 'preflight'))
        self.child = self.stack.enter_context(patch.object(
            render.subprocess, 'run', return_value=SimpleNamespace(returncode=0)))
        self.stack.enter_context(patch.object(render.sys, 'argv', ['render_checks.py', '--build', '/fixture/build']))
        self.stack.enter_context(patch.dict(os.environ, {
            'CI_PARALLEL': '2', '__EGL_VENDOR_LIBRARY_FILENAMES': '/inherited/nvidia.json',
            'MESA_GL_VERSION_OVERRIDE': '9.9', 'MESA_GLSL_VERSION_OVERRIDE': '999',
            'MESA_GLES_VERSION_OVERRIDE': '9.9', 'SECRET_TOKEN': 'must-never-be-printed'}, clear=True))
        self.stdout = self.stack.enter_context(redirect_stdout(io.StringIO()))
        self.stderr = self.stack.enter_context(redirect_stderr(io.StringIO()))

    def refuse(self):
        self.assertEqual(render.main(), 64)
        self.child.assert_not_called()
        self.full_preflight.assert_not_called()
        self.assertIn('Mesa EGL descriptor refused before Xvfb:', self.stderr.getvalue())
        self.assertNotIn('must-never-be-printed', self.stdout.getvalue() + self.stderr.getvalue())

    def test_selector_reaches_every_original_stage_before_xvfb_with_original_deadlines(self):
        self.assertEqual(render.main(), 0)
        stages = [
            ('gpu-environment', ['glxinfo', '-B']),
            ('native-x11', ['/fixture/build/ice_platform_input_x11_tests']),
            ('native-window', ['/fixture/build/bin/ice_window_smoke']),
            ('gpu-gl', ['/fixture/build/bin/ice_gl_tests', 'tests/data/gl']),
            ('gpu-switch', ['/fixture/build/bin/ice_render_switch_tests', 'tests/data']),
            ('gpu-reload', ['/fixture/build/bin/ice_live_pipeline_tests']),
            ('gpu-mesh', ['/fixture/build/bin/ice_mesh_tests', 'tests/data/mesh']),
            ('gpu-textured', ['/fixture/build/bin/ice_textured_mesh_tests']),
            ('gpu-skinned', ['/fixture/build/bin/ice_skinned_mesh_tests']),
            ('gpu-hdr', ['/fixture/build/bin/ice_hdr_tests', 'tests/data/hdr']),
        ]
        self.assertEqual(self.child.call_count, 10)
        for call, (name, target) in zip(self.child.call_args_list, stages):
            argv = call.args[0]
            self.assertEqual(argv[0], render.sys.executable)
            self.assertEqual(Path(argv[1]).name, 'run_stage.py')
            self.assertEqual(argv[2:6], ['--timeout-seconds', '240', name, '--'])
            self.assertEqual(argv[6:], ['xvfb-run', '-a', '-e', '/dev/stderr', '-s',
                '-screen 0 1280x800x24 -nolisten tcp', *target])
            environment = call.kwargs['env']
            self.assertEqual(environment['__EGL_VENDOR_LIBRARY_FILENAMES'], str(self.descriptor))
            self.assertEqual(environment['LIBGL_ALWAYS_SOFTWARE'], '1')
            self.assertEqual(environment['LP_NUM_THREADS'], '2')
            self.assertEqual(environment['LANG'], 'C.UTF-8')
            self.assertEqual(environment['XMODIFIERS'], '@im=none')
            self.assertTrue(all(name not in environment for name in render.xvfb.OVERRIDES))
        self.full_preflight.assert_not_called()
        self.assertIn('EGL vendor descriptor:', self.stdout.getvalue())
        self.assertIn('sha256=', self.stdout.getvalue())
        self.assertNotIn('must-never-be-printed', self.stdout.getvalue())

    def test_failure_at_each_position_keeps_later_checks_and_aggregate_red(self):
        for failed in range(10):
            with self.subTest(failed=failed):
                self.child.reset_mock()
                self.child.side_effect = [SimpleNamespace(returncode=23 if i == failed else 0)
                                          for i in range(10)]
                self.assertEqual(render.main(), 1)
                self.assertEqual(self.child.call_count, 10)
                self.assertEqual(self.child.call_args_list[-1].args[0][4], 'gpu-hdr')

    def test_missing_unreadable_or_untrusted_descriptor_refuses_before_any_stage(self):
        self.descriptor.unlink()
        self.refuse()
        with patch.object(render.xvfb, 'observe_file', side_effect=PermissionError):
            self.refuse()
        self.descriptor.write_text('{}')
        self.descriptor.chmod(0o666)
        self.refuse()

    def test_malformed_mismatched_or_duplicate_json_never_falls_back_to_all_vendors(self):
        for raw in (b'{bad', b'{"file_format_version":"1.0.0","ICD":{"library_path":"libEGL_nvidia.so.0"}}',
                    b'{"file_format_version":"2.0.0","ICD":{}}',
                    b'{"ICD":{"library_path":"libEGL_nvidia.so.0"},"ICD":{"library_path":"libEGL_mesa.so.0"}}'):
            with self.subTest(raw=raw):
                self.descriptor.write_bytes(raw)
                self.refuse()

    def test_oversized_or_nonregular_descriptor_refuses_before_any_stage(self):
        self.descriptor.write_bytes(b' ' * (render.xvfb.MAX_DESCRIPTOR + 1))
        self.refuse()
        self.descriptor.unlink()
        self.descriptor.mkdir()
        self.refuse()


if __name__ == '__main__':
    unittest.main()
