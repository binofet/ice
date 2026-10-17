"""Portable wrapper contracts: sudo/root commands are captured, never executed."""

import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import owner_setup as setup


class OwnerSetupTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.plan = {"source_commit": setup.SOURCE, "enabled": False,
                     "actions": [{"kind": "synthetic"}] * 42}
        self.digest = hashlib.sha256(json.dumps(self.plan, sort_keys=True,
                                    separators=(",", ":"), ensure_ascii=True).encode()).hexdigest()
        self.planned = {"status": "plan_only", "plan_sha256": self.digest, "plan": self.plan}
        self.applied = {"status": "disabled_bootstrap_complete", "enabled": False,
                        "services_started": False, "sudo_delegation": False,
                        "completed": self.plan["actions"]}
        self.calls = []

    def runner(self, arguments, **options):
        self.calls.append((arguments, options))
        value = self.planned if "--plan" in arguments else self.applied
        return subprocess.CompletedProcess(arguments, 0, json.dumps(value).encode(), b"")

    def test_exact_fixed_source_and_external_hashes(self):
        self.assertEqual(setup.PLAN, "c8b61abaf8b21a52e60786a02f31b7fa902728d4799c0ab218682109838ba851")
        self.assertEqual(setup.MANIFEST, "fdd1420f2e614da287617c33d8ecdb7969fdbbc1ea558ea281ab2a9dd5ed1a24")
        self.assertEqual(len(setup.HASHES), 8)
        commands = setup.seal_commands()
        self.assertNotIn("/usr/bin/python", commands)
        self.assertIn('test ! -e "$dst"', commands)
        self.assertIn('test ! -L "$dst"', commands)
        for name, digest in setup.HASHES.items():
            self.assertIn(digest + "  " + name + "\n", commands)

    def test_seal_then_plan_then_only_matching_apply_and_retained_receipt(self):
        with patch.object(setup, "PLAN", self.digest):
            setup.execute(self.root, self.runner)
        self.assertEqual(len(self.calls), 3)
        self.assertEqual(self.calls[0][0], ["/usr/bin/sudo", "/bin/sh", "-eu"])
        self.assertEqual(self.calls[0][1]["input"], setup.seal_commands().encode())
        for command, options in self.calls[1:]:
            self.assertEqual(command[:5], ["/usr/bin/sudo", "/usr/bin/python3", "-I", "-B",
                                           str(setup.SEALED / "bootstrap.py")])
            self.assertEqual(options["env"], {"PATH": "/usr/bin:/bin", "LANG": "C.UTF-8"})
            self.assertNotIn("shell", options)
        self.assertIn("--plan", self.calls[1][0])
        self.assertEqual(self.calls[2][0][-2:], ["--approved-plan-sha256", self.digest])
        self.assertEqual(json.loads((self.root / "receipt.json").read_text())["enabled"], False)
        for stage in ("seal", "root-plan", "root-apply"):
            self.assertEqual((self.root / (stage + ".exit")).read_text(), "0\n")

    def test_changed_actual_plan_rejects_even_with_claimed_expected_digest(self):
        self.planned["plan"]["actions"].append({"kind": "unapproved"})
        with patch.object(setup, "PLAN", self.digest), self.assertRaisesRegex(ValueError, "Machine plan changed"):
            setup.execute(self.root, self.runner)
        self.assertEqual(len(self.calls), 2)
        self.assertTrue((self.root / "root-plan.stdout").is_file())
        self.assertFalse((self.root / "root-apply.stdout").exists())

    def test_seal_failure_prevents_privileged_python(self):
        def failed(arguments, **options):
            self.calls.append((arguments, options))
            return subprocess.CompletedProcess(arguments, 1, b"", b"hash mismatch")
        with self.assertRaisesRegex(ValueError, "seal refused"):
            setup.execute(self.root, failed)
        self.assertEqual(len(self.calls), 1)
        self.assertEqual((self.root / "seal.stderr").read_bytes(), b"hash mismatch")

    def test_plan_failure_prevents_apply(self):
        def failed(arguments, **options):
            result = self.runner(arguments, **options)
            if "--plan" in arguments:
                result.returncode = 1
            return result
        with self.assertRaisesRegex(ValueError, "root-plan refused"):
            setup.execute(self.root, failed)
        self.assertEqual(len(self.calls), 2)

    def test_partial_apply_keeps_diagnostic_without_success_receipt(self):
        def failed(arguments, **options):
            result = self.runner(arguments, **options)
            if "--apply" in arguments:
                result.returncode = 1
                result.stdout = b'{"status":"partial","completed":["synthetic"]}'
            return result
        with patch.object(setup, "PLAN", self.digest), self.assertRaisesRegex(ValueError, "root-apply refused"):
            setup.execute(self.root, failed)
        self.assertIn(b"partial", (self.root / "root-apply.stdout").read_bytes())
        self.assertFalse((self.root / "receipt.json").exists())

    def test_enabled_or_started_result_cannot_be_success(self):
        self.applied["services_started"] = True
        with patch.object(setup, "PLAN", self.digest), self.assertRaisesRegex(ValueError, "disabled completion"):
            setup.execute(self.root, self.runner)
        self.assertFalse((self.root / "receipt.json").exists())

    def test_timeout_retains_partial_output_and_unknown_completion(self):
        def timed_out(arguments, **options):
            raise subprocess.TimeoutExpired(arguments, 180, output=b"partial stdout", stderr=b"partial stderr")
        with self.assertRaisesRegex(ValueError, "completion is unknown"):
            setup.execute(self.root, timed_out)
        self.assertTrue((self.root / "seal.started.json").is_file())
        self.assertEqual((self.root / "seal.stdout").read_bytes(), b"partial stdout")
        uncertainty = json.loads((self.root / "seal.uncertain.json").read_text())
        self.assertEqual(uncertainty["completion"], "unknown")
        self.assertFalse(uncertainty["automatic_retry"])
        self.assertFalse((self.root / "seal.exit").exists())
        self.assertFalse((self.root / "root-plan.started.json").exists())

    def test_interruption_retains_unknown_completion_without_fabricated_exit(self):
        def interrupted(arguments, **options):
            raise KeyboardInterrupt()
        with self.assertRaisesRegex(ValueError, "completion is unknown"):
            setup.execute(self.root, interrupted)
        uncertainty = json.loads((self.root / "seal.uncertain.json").read_text())
        self.assertEqual(uncertainty["reason"], "KeyboardInterrupt")
        self.assertFalse((self.root / "seal.exit").exists())

    def test_read_only_check_does_not_invoke_sudo_or_create_attempt(self):
        with patch.object(setup.sys, "platform", "linux"), patch.object(setup.os, "geteuid", return_value=1000), \
             patch.object(setup.os, "getuid", return_value=1000), patch.object(setup, "SEALED", self.root / "absent"), \
             patch.object(setup, "check_packet") as checked, patch.object(setup, "execute") as execute, \
             patch.object(setup.tempfile, "mkdtemp") as mkdir:
            self.assertEqual(setup.main(["--check"]), 0)
        checked.assert_called_once_with()
        execute.assert_not_called()
        mkdir.assert_not_called()


if __name__ == "__main__":
    unittest.main(verbosity=2)
