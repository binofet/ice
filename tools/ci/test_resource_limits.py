#!/usr/bin/env python3
"""Pure Python resource wiring regressions; never parse native code or run tools."""
from contextlib import redirect_stderr, redirect_stdout
import importlib.util
import io
import json
import os
from pathlib import Path
import shlex
import sys
import tempfile
import types
import unittest
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[2]


def load_module(name, relative):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def load_api():
    # No real clang/compiler-context import or native library discovery is allowed.
    cindex = types.ModuleType('clang.cindex')
    kinds = ('CLASS_DECL STRUCT_DECL CLASS_TEMPLATE ENUM_DECL TYPEDEF_DECL TYPE_ALIAS_DECL '
             'FUNCTION_DECL FUNCTION_TEMPLATE CXX_METHOD CONSTRUCTOR DESTRUCTOR FIELD_DECL VAR_DECL')
    cindex.CursorKind = types.SimpleNamespace(**{name: name for name in kinds.split()})
    cindex.Index = types.SimpleNamespace(create=Mock(side_effect=AssertionError('Native clang is forbidden')))
    clang = types.ModuleType('clang'); clang.cindex = cindex
    context = types.ModuleType('compiler_context')
    context.system_arguments = Mock(side_effect=AssertionError('Compiler header probing is forbidden'))
    with patch.dict(sys.modules, {'clang': clang, 'clang.cindex': cindex, 'compiler_context': context}):
        return load_module('resource_api_fixture', 'tools/quality/api.py')


class ResourceLimitTests(unittest.TestCase):
    def run_api(self, arguments, expected_workers, environment=None):
        module = load_api()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve(); build = root / 'build/ci'; build.mkdir(parents=True)
            files = [root / 'engine/src/first.cpp', root / 'engine/src/second.cpp',
                     root / 'tests/public_headers.cpp']
            selected = [{'file': str(path), 'command': 'fixture command, never executed'} for path in files]
            database = [selected[0], selected[0], selected[1], selected[2],
                        {'file': str(root / 'tests/unrelated.cpp'), 'command': 'excluded fixture'}]
            (build / 'compile_commands.json').write_text(json.dumps(database))
            report = root / 'report.json'
            pool = Mock()
            pool.map.return_value = [({str(index): {'file': 'fixture.h', 'line': index}},
                                      {str(index)}, [], []) for index in range(3)]
            with patch.object(module, 'ROOT', root), patch.object(module, 'ENGINE', root / 'engine'), \
                    patch.object(module, 'scan', side_effect=AssertionError('Native scan is forbidden')) as scan, \
                    patch.object(module.concurrent.futures, 'ThreadPoolExecutor') as executor, \
                    patch.object(sys, 'argv', ['api.py', *arguments, '--report', str(report)]), \
                    patch.dict(os.environ, environment or {}, clear=False), redirect_stdout(io.StringIO()), \
                    redirect_stderr(io.StringIO()):
                executor.return_value.__enter__.return_value = pool
                try:
                    result = module.main()
                except SystemExit as error:
                    self.fail('Valid API worker arguments were rejected: ' + str(error))
                self.assertEqual(result, 0)
                executor.assert_called_once_with(max_workers=expected_workers)
                pool.map.assert_called_once_with(scan, selected)
                scan.assert_not_called()
            data = json.loads(report.read_text())
            self.assertEqual(data['declarations'], 3)
            self.assertEqual(data['missing'], [])
            self.assertEqual(data['errors'], [])

    def test_api_default_remains_four_and_does_not_implicitly_read_ci_environment(self):
        self.run_api(['--build-dir', 'build/ci'], 4, {'CI_PARALLEL': '1'})

    def test_api_positive_worker_option_reaches_actual_executor_without_dropping_units(self):
        for count in (1, 2, 7):
            with self.subTest(workers=count):
                self.run_api(['--build-dir', 'build/ci', '--workers', str(count)], count)

    def test_api_invalid_workers_reject_before_database_executor_or_scan(self):
        for value in ('0', '-1', 'bad', '1.5', ''):
            with self.subTest(value=value):
                module = load_api()
                with patch.object(sys, 'argv', ['api.py', '--workers', value]), \
                        patch.object(Path, 'read_text', side_effect=AssertionError('Database must not be read')), \
                        patch.object(module.concurrent.futures, 'ThreadPoolExecutor') as executor, \
                        patch.object(module, 'scan') as scan, redirect_stderr(io.StringIO()):
                    with self.assertRaises(SystemExit) as raised:
                        module.main()
                    self.assertEqual(raised.exception.code, 2)
                    executor.assert_not_called()
                    scan.assert_not_called()

    def test_ci_api_stage_passes_selected_budget_to_actual_executor(self):
        text = (ROOT / '.github/workflows/engine-quality.yml').read_text()
        lines = [line for line in text.splitlines() if 'run_stage.py api -- ' in line]
        self.assertEqual(len(lines), 1)
        command = shlex.split(lines[0].strip())
        at = command.index('tools/quality/api.py')
        arguments = command[at + 1:]
        self.assertEqual(arguments.count('--workers'), 1)
        self.assertEqual(arguments[arguments.index('--workers') + 1], '$CI_API_PARALLEL')
        self.assertIn("      CI_API_PARALLEL: '2'", text)
        self.assertIn("      CI_TEST_PARALLEL: '2'", text)
        self.assertIn("CI_PARALLEL: ${{ matrix.os == 'macos-15' && '10' || '2' }}", text)
        self.assertIn('ctest --test-dir build/ci', text)
        self.assertIn('--output-on-failure --parallel "$CI_TEST_PARALLEL"', text)
        for budget in ('2',):
            with self.subTest(ci_budget=budget):
                self.run_api([budget if value == '$CI_API_PARALLEL' else value for value in arguments], int(budget))

    def test_direct_rustc_argv_pins_one_codegen_unit_and_retains_fixture_roundtrip(self):
        module = load_module('resource_rust_fixture', 'tools/metagenintegration/check_rust.py')
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory).resolve()
            with patch.object(sys, 'argv', ['check_rust.py', '--native-build', str(build),
                                           '--rustc', '/fixture/rustc']), \
                    patch.object(module.subprocess, 'run') as run:
                module.main()
            calls = run.call_args_list
            self.assertEqual(len(calls), 5)
            command = calls[1].args[0]
            self.assertEqual(command[0], '/fixture/rustc')
            self.assertEqual(command.count('-Ccodegen-units=1'), 1)
            self.assertIn('--edition=2021', command)
            self.assertIn('-Dwarnings', command)
            self.assertIn(str(ROOT / 'tools/metagenintegration/examples/rust/main.rs'), command)
            executable = str(build / 'rust-proof/metagen-rust-proof')
            self.assertEqual(command[command.index('-o') + 1], executable)
            self.assertEqual(calls[0].args[0], ['cmake', '--build', str(build), '--target', 'meta_wire_fixture'])
            paths = [str(build / 'rust-proof' / name) for name in
                     ('cpp-default.icm', 'cpp-changed.icm', 'rust-default.icm', 'rust-changed.icm')]
            fixture = str(build / 'meta_wire_fixture')
            self.assertEqual(calls[2].args[0], [fixture, '--emit', *paths[:2]])
            self.assertEqual(calls[3].args[0], [executable, *paths])
            self.assertEqual(calls[4].args[0], [fixture, '--verify', *paths[2:]])
            self.assertTrue(all(call.kwargs == {'check': True} for call in calls))


if __name__ == '__main__':
    unittest.main()
