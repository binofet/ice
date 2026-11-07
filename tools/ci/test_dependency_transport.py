"""Mocked transport failures only; no downloads, installation or host cache."""
from contextlib import redirect_stdout
import io
import subprocess
import unittest
from unittest.mock import patch
import owned_runner as owned


class DependencyTransportTests(unittest.TestCase):
    def interrupted(self):
        return subprocess.CalledProcessError(2, ['fixture'], output='BrokenPipeError: 32\nProtocolError')

    def run_install(self, effects, clocks):
        with patch.object(owned.time, 'monotonic', side_effect=clocks), \
                patch.object(owned.subprocess, 'run', side_effect=effects) as run, \
                redirect_stdout(io.StringIO()) as log:
            owned.install_quality(['fixture', '--unchanged-pin'], {'PIP_CONFIG_FILE': '/dev/null'})
            return run.call_args_list, log.getvalue()

    def test_interrupted_body_then_success_preserves_command_environment_and_total_budget(self):
        calls, log = self.run_install([self.interrupted(), subprocess.CompletedProcess([], 0, 'ok')],
                                     [10, 11, 31])
        self.assertEqual([c.kwargs['timeout'] for c in calls], [599, 579])
        self.assertTrue(all(c.args == (['fixture', '--unchanged-pin'],) for c in calls))
        self.assertTrue(all(c.kwargs['env'] == {'PIP_CONFIG_FILE': '/dev/null'} for c in calls))
        self.assertIn('ProtocolError', log)

    def test_transport_exhaustion_is_terminal_after_three_attempts(self):
        with patch.object(owned.time, 'monotonic', side_effect=[0, 1, 2, 3]), \
                patch.object(owned.subprocess, 'run', side_effect=self.interrupted()) as run, \
                redirect_stdout(io.StringIO()), self.assertRaises(subprocess.CalledProcessError):
            owned.install_quality(['fixture'], {})
        self.assertEqual(run.call_count, 3)

    def test_deterministic_failure_and_process_timeout_are_not_retried(self):
        for error in (subprocess.CalledProcessError(1, ['fixture'], output='hash mismatch'),
                      subprocess.TimeoutExpired(['fixture'], 600)):
            with self.subTest(error=type(error).__name__), \
                    patch.object(owned.time, 'monotonic', side_effect=[0, 1]), \
                    patch.object(owned.subprocess, 'run', side_effect=error) as run, \
                    redirect_stdout(io.StringIO()), self.assertRaises(type(error)):
                owned.install_quality(['fixture'], {})
            self.assertEqual(run.call_count, 1)

    def test_expired_total_deadline_never_starts_another_process(self):
        with patch.object(owned.time, 'monotonic', side_effect=[0, 1, 601]), \
                patch.object(owned.subprocess, 'run', side_effect=self.interrupted()) as run, \
                redirect_stdout(io.StringIO()), self.assertRaises(TimeoutError):
            owned.install_quality(['fixture'], {})
        self.assertEqual(run.call_count, 1)


if __name__ == '__main__':
    unittest.main()
