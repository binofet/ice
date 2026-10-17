"""No root, provider, services or credentials: exercise exact admission and real supervision."""

from copy import deepcopy
from pathlib import Path
import json
import os
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parent))
import contract
import job


def example():
    return json.loads(Path(__file__).with_name("policy.example.json").read_text())


def snapshot():
    return {"format": "ice-family-merge-snapshot-v1", "repo": "ice", "head": "a" * 40,
            "base": "b" * 40, "candidate": "c" * 40, "current_head": "a" * 40,
            "current_base": "b" * 40,
            "review": {"head": "a" * 40, "state": "approved", "changes_requested": False,
                       "receipt_sha256": "d" * 64},
            "checks": [{"name": name, "head": "a" * 40, "base": "b" * 40,
                        "candidate": "c" * 40, "status": "completed", "conclusion": "success",
                        "report_sha256": "e" * 64} for name in ("cpu20", "cpu23")]}


class ContractTests(unittest.TestCase):
    def test_example_is_disabled_and_uses_no_cosmo_identities(self):
        policy = contract.policy(example())
        self.assertFalse(policy["enabled"])
        self.assertNotIn("cosmotomata", json.dumps(policy))

    def test_new_distinct_control_and_source_users_required(self):
        for user in ("jmax", "root", "cosmotomata-core-job", "ice-ci-control"):
            with self.subTest(user=user):
                config = example()
                config["repos"]["ice"]["job_user"] = user
                with self.assertRaises(ValueError):
                    contract.policy(config)

    def test_job_record_binds_repo_candidate_policy_dependency_and_review(self):
        config, identity, digest = example(), "f" * 32, "9" * 64
        admission = {"format": "ice-family-cpu-job-v1", "repo": "ignition", "id": identity,
                     "event": "pull_request", "head": "a" * 40, "base": "b" * 40,
                     "candidate": "c" * 40, "dependencies": {"ice": "d" * 40},
                     "policy_sha256": digest, "inventory_sha256": "e" * 64,
                     "review_receipt_sha256": "8" * 64, "provider_run_id": "123"}
        self.assertEqual(contract.admission(admission, config, "ignition", identity, digest), admission)
        for key, value in [("repo", "ice"), ("dependencies", {}), ("policy_sha256", "1" * 64),
                           ("review_receipt_sha256", "0" * 64), ("candidate", "main"),
                           ("id", "../escape"), ("provider_run_id", "123; shell")]:
            with self.subTest(key=key):
                bad = deepcopy(admission)
                bad[key] = value
                with self.assertRaises(ValueError):
                    contract.admission(bad, config, "ignition", identity, digest)

    def test_broker_plan_has_fixed_executable_clean_env_and_isolation(self):
        argv = contract.unit_command(example(), "ice", "a" * 32)
        self.assertEqual(argv[0], "/usr/bin/systemd-run")
        self.assertIn("--uid=ice-cpu-job", argv)
        for value in ["PrivateNetwork=yes", "PrivateDevices=yes", "ProtectHome=yes",
                      "ProtectSystem=strict", "KillMode=control-group", "CPUQuota=200%",
                      "MemoryMax=8192M", "TasksMax=256", "NoNewPrivileges=yes"]:
            self.assertIn("--property=" + value, argv)
        self.assertIn("-i", argv)
        self.assertEqual(argv[-3:], [str(contract.INSTALL / "job.py"), "ice", "a" * 32])
        self.assertFalse(any("/home/" in value for value in argv))
        with self.assertRaises(ValueError):
            contract.unit_command(example(), "ice", "$(shell)")

    def test_exact_reviewed_snapshot_admits_without_merging(self):
        result = contract.verify_merge(example(), snapshot())
        self.assertEqual(result["status"], "admitted_for_exact_snapshot")
        self.assertFalse(result["merge_performed"])

    def test_advanced_head_or_base_and_stale_review_rejected(self):
        for key in ("current_head", "current_base"):
            with self.subTest(key=key):
                row = snapshot()
                row[key] = "f" * 40
                with self.assertRaises(ValueError):
                    contract.verify_merge(example(), row)
        for field, value in [("head", "f" * 40), ("state", "pending"),
                             ("changes_requested", True), ("receipt_sha256", "0" * 64)]:
            with self.subTest(field=field):
                row = snapshot()
                row["review"][field] = value
                with self.assertRaises(ValueError):
                    contract.verify_merge(example(), row)

    def test_required_check_missing_duplicate_or_non_success_is_not_green(self):
        for conclusion in ("failure", "cancelled", "timed_out", "skipped", "neutral", None):
            with self.subTest(conclusion=conclusion):
                row = snapshot()
                row["checks"][0]["conclusion"] = conclusion
                with self.assertRaises(ValueError):
                    contract.verify_merge(example(), row)
        for rows in ([], snapshot()["checks"][:1], [snapshot()["checks"][0]] * 2):
            row = snapshot()
            row["checks"] = rows
            with self.assertRaises(ValueError):
                contract.verify_merge(example(), row)

    def test_passing_check_for_old_merge_or_uncollected_report_rejected(self):
        for field, value in [("head", "f" * 40), ("base", "f" * 40),
                             ("candidate", "f" * 40), ("status", "queued"),
                             ("report_sha256", None), ("report_sha256", "0" * 64)]:
            with self.subTest(field=field):
                row = snapshot()
                row["checks"][0][field] = value
                with self.assertRaises(ValueError):
                    contract.verify_merge(example(), row)

    def test_json_duplicate_nonfinite_and_oversize_inputs_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "input.json"
            for data in ('{"head":1,"head":2}', '{"value":NaN}', '"' + "x" * 100 + '"'):
                path.write_text(data)
                with self.assertRaises(ValueError):
                    contract.bounded_json(path, 64)

    def test_collected_report_requires_exact_source_and_all_actual_stages(self):
        request = {"repo": "ice", "event": "push", "candidate": "a" * 40,
                   "policy_sha256": "d" * 64}
        names = {"clone", "checkout", "source-identity", "base-ancestry"}
        names.update(p + suffix for p in ("cpu20", "cpu23")
                     for suffix in ("-configure", "-build", "-inventory", "-tests"))
        report = {"format": "ice-family-cpu-report-v1", "status": "passed", "repo": "ice",
                  "admission": deepcopy(request), "policy_sha256": request["policy_sha256"],
                  "gpu": False, "devices": False, "providers": False, "merge_performed": False,
                  "stages": [{"name": n, "exit_code": 0, "timed_out": False,
                              "seconds": .1, "log_sha256": "e" * 64} for n in sorted(names)],
                  "profiles": [{"name": n, "status": "passed", "inventory": ["actual-case"], "cases": [
                      {"name": "actual-case", "classname": "CPU", "time": .1,
                       "failed": False, "skipped": False}]} for n in ("cpu20", "cpu23")]}
        self.assertEqual(contract.verify_report(example(), request, report)["tests"], 2)
        mutations = [lambda r: r["admission"].update(candidate="f" * 40),
                     lambda r: r["stages"].pop(),
                     lambda r: r["stages"][0].update(exit_code=23),
                     lambda r: r["stages"][0].update(timed_out=True),
                     lambda r: r["profiles"][0].update(cases=[]),
                     lambda r: r["profiles"][0]["cases"][0].update(skipped=True),
                     lambda r: r["stages"][0].update(seconds=float("nan")),
                     lambda r: r["profiles"][0]["cases"][0].update(time=float("inf")),
                     lambda r: r["profiles"][0]["cases"][0].update(name=""),
                     lambda r: r["profiles"][0]["inventory"].append("actual-case"),
                     lambda r: r["profiles"][0]["cases"].append(dict(r["profiles"][0]["cases"][0])),
                     lambda r: r.update(gpu=True)]
        for mutate in mutations:
            bad = deepcopy(report)
            mutate(bad)
            with self.assertRaises(ValueError):
                contract.verify_report(example(), request, bad)

    def test_native_junit_rejects_nonfinite_negative_missing_times_and_duplicate_ids(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "cases.xml"
            for value in ("nan", "inf", "-1", "invalid"):
                with self.subTest(value=value):
                    path.write_text('<testsuite><testcase name="real" classname="CPU" time="' + value + '"/></testsuite>')
                    with self.assertRaises(ValueError):
                        job.test_cases(path)
            path.write_text('<testsuite>' + '<testcase name="same" classname="CPU" time=".1"/>' * 2 + '</testsuite>')
            with self.assertRaises(ValueError):
                job.test_cases(path)

    def test_real_stage_failure_and_bounded_log_keep_original_exit(self):
        with tempfile.TemporaryDirectory() as directory:
            runner = job.Runner(Path(directory), {"PATH": "/usr/bin:/bin"}, 5)
            with self.assertRaisesRegex(ValueError, "Stage failed"):
                runner.run("failure", [sys.executable, "-c", "print('x'*2000000); raise SystemExit(23)"])
            self.assertEqual(runner.stages[0]["exit_code"], 23)
            self.assertTrue(runner.stages[0]["log_truncated"])
            self.assertLessEqual((Path(directory) / "failure.log").stat().st_size, job.LOG_BYTES + 100)

    def test_real_stage_timeout_is_nonzero_and_leaves_unrelated_process_alive(self):
        with tempfile.TemporaryDirectory() as directory:
            sentinel = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(30)"],
                                        start_new_session=True)
            try:
                runner = job.Runner(Path(directory), {"PATH": "/usr/bin:/bin"}, .15)
                with self.assertRaisesRegex(ValueError, "Stage failed"):
                    runner.run("timeout", [sys.executable, "-c", "import time; time.sleep(30)"])
                self.assertEqual(runner.stages[0]["exit_code"], 124)
                self.assertTrue(runner.stages[0]["timed_out"])
                self.assertIsNone(sentinel.poll())
            finally:
                sentinel.terminate()
                sentinel.wait()

    def test_real_python_junit_has_per_case_timing_and_skip_is_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "test_sample.py").write_text(
                "import unittest\nclass Sample(unittest.TestCase):\n"
                " def test_ok(self): self.assertTrue(True)\n"
                " @unittest.skip('required skip')\n def test_skip(self): pass\n")
            xml = root / "cases.xml"
            result = subprocess.run([sys.executable, str(Path(__file__).with_name("unittest_xml.py")),
                                     str(root), str(xml)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            self.assertEqual(result.returncode, 1)
            cases = list(ET.parse(xml).getroot().iter("testcase"))
            self.assertEqual(len(cases), 2)
            self.assertTrue(all(float(c.get("time")) >= 0 for c in cases))
            self.assertEqual(sum(c.find("skipped") is not None for c in cases), 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
