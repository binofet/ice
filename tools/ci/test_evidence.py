#!/usr/bin/env python3
"""Focused checks that missing, stale or failed CI evidence cannot become a pass."""

from contextlib import redirect_stdout
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET

import report
from test_owned_runner import OwnedRunnerTests, OwnedWorkflowTests
from test_resource_limits import ResourceLimitTests
from test_xvfb_readiness import XvfbReadinessTests
from test_render_checks import RenderChecksTests
from test_stage_cancellation import StageCancellationTests
import check_paths


class EvidenceTests(unittest.TestCase):
    def test_only_directories_are_lowercase_and_upstream_contracts_are_explicit(self):
        self.assertEqual(check_paths.violations([
            "engine/include/ice/MyType.h", "CMakeLists.txt", "tools/quality/check.py",
            "engine/3rdparty/khronos/GL/glcorearb.h",
            "examples/airhockey/Box2D/Collision/Shapes/b2Shape.h",
            "examples/skeeball/bullet/BulletDynamics/test.h"]), [])
        self.assertEqual(check_paths.violations(["engine/Render/MyType.h"]), ["engine/Render"])

    def source(self, runner="macOS", profile="native"):
        return {"runner_os": runner, "cpp": "23", "profile": profile,
                "tested_commit": "a" * 40, "source_head": "b" * 40,
                "tested_base": "c" * 40, "base_role": "PR merge first parent",
                "source_validated": True, "source_issues": [],
                "run_url": "https://github.com/example/ice/actions/runs/42"}

    def test_exact_pr_merge_source_and_base_checked_before_success(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            event = output / "event.json"
            event.write_text(json.dumps({"pull_request": {
                "head": {"sha": "b" * 40}, "base": {"sha": "c" * 40}}}))
            environment = {"GITHUB_EVENT_PATH": str(event), "GITHUB_EVENT_NAME": "pull_request",
                           "GITHUB_SHA": "a" * 40, "RUNNER_OS": "Linux", "CI_CPP": "23",
                           "GITHUB_SERVER_URL": "https://github.com", "GITHUB_RUN_ID": "42",
                           "GITHUB_REPOSITORY": "example/ice", "SECRET_TOKEN": "never-export-this"}
            answers = {("rev-parse", "HEAD"): "a" * 40,
                       ("show", "-s", "--format=%P", "HEAD"): "c" * 40 + " " + "b" * 40,
                       ("ls-tree", "-r", "HEAD"): "160000 commit " + "d" * 40 + "\ttools/metagen",
                       ("status", "--porcelain=v1", "--untracked-files=no"): "",
                       ("rev-parse", "HEAD^{tree}"): "e" * 40}
            with patch.dict(os.environ, environment, clear=True), patch.object(
                    report, "git", side_effect=lambda *args: answers[args]):
                report.identify(output, "native")
                source = json.loads((output / "source.json").read_text())
                self.assertEqual(source["tested_base"], "c" * 40)
                self.assertEqual(source["submodule_pins"], {"tools/metagen": "d" * 40})
                self.assertNotIn("never-export-this", (output / "source.json").read_text())
                os.environ["CI_OWNED_RUNNER"] = "1"
                observed = OwnedRunnerTests().observed()
                with patch.object(report.owned_runner, "runtime", return_value=observed):
                    report.identify(output, "native")
                self.assertEqual(json.loads((output / "source.json").read_text())["owned_runtime"], observed)
                with patch.object(report.owned_runner, "runtime", side_effect=ValueError("fixture runtime mismatch")):
                    with self.assertRaisesRegex(ValueError, "Owned runtime"):
                        report.identify(output, "native")
                self.assertFalse(json.loads((output / "source.json").read_text())["source_validated"])
                del os.environ["CI_OWNED_RUNNER"]
                answers[("show", "-s", "--format=%P", "HEAD")] = "e" * 40 + " " + "b" * 40
                with self.assertRaisesRegex(ValueError, "merge parents differ"):
                    report.identify(output, "native")
                self.assertFalse(json.loads((output / "source.json").read_text())["source_validated"])

    def test_owned_runtime_summary_keeps_provenance_before_an_unbroken_stage_table(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            source = self.fixture(output)
            source["owned_runtime"] = OwnedRunnerTests().observed('Darwin')
            (output / "source.json").write_text(json.dumps(source))
            with patch.object(report.owned_runner, "evidence_issues", return_value=[]):
                self.assertEqual(self.summary(output), 0)
            text = (output / "summary.md").read_text()
            self.assertLess(text.index("Actual owned executor:"), text.index("| Stage |"))
            self.assertIn("26.5.2", text)
            self.assertIn("ARM64", text)
            self.assertIn("3.12.15", text)
            self.assertIn("| --- | --- | ---: | ---: |\n| evidence |", text)

    def stage(self, directory, code, stage="probe", timeout="10"):
        return subprocess.run(
            [sys.executable, str(Path(__file__).with_name("run_stage.py")),
             "--timeout-seconds", timeout, stage, "--", sys.executable, "-c", code],
            env=dict(os.environ, CI_EVIDENCE_DIR=directory), stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, timeout=20)

    def test_failure_code_and_bounded_noisy_logs(self):
        with tempfile.TemporaryDirectory() as directory:
            result = self.stage(directory, "import sys; print('x' * (9 * 1024 * 1024)); sys.exit(23)")
            self.assertEqual(result.returncode, 23)
            record = json.loads((Path(directory) / "probe.json").read_text())
            self.assertEqual(record["exit_code"], 23)
            self.assertGreater(record["duration_seconds"], 0)
            self.assertTrue(record["log_truncated"])
            self.assertLess((Path(directory) / "probe.log").stat().st_size, 8 * 1024 * 1024 + 100)
            self.assertLess(len(result.stdout), 1024 * 1024 + 5000)
            self.assertEqual(self.stage(directory, "pass").returncode, 2)
            self.assertEqual(json.loads((Path(directory) / "probe.json").read_text())["exit_code"], 23)

    def test_deadline_is_recorded_as_failure_not_success(self):
        with tempfile.TemporaryDirectory() as directory:
            result = self.stage(directory, "import time; time.sleep(10)", timeout=".15")
            self.assertEqual(result.returncode, 124)
            record = json.loads((Path(directory) / "probe.json").read_text())
            self.assertTrue(record["timed_out"])
            self.assertLess(record["duration_seconds"], 5)

    @unittest.skipIf(os.name == "nt", "POSIX child process-group test")
    def test_deadline_covers_child_retaining_stdout_after_parent_exit(self):
        with tempfile.TemporaryDirectory() as directory:
            result = self.stage(directory,
                "import subprocess,sys; subprocess.Popen([sys.executable,'-c',"
                "'import time; time.sleep(10)'])", timeout=".2")
            self.assertEqual(result.returncode, 124)

    def fixture(self, output, runner="macOS", profile="native"):
        source = self.source(runner, profile)
        (output / "source.json").write_text(json.dumps(source))
        stages, suites = report.plan(profile, runner)
        for name in stages:
            (output / f"{name}.json").write_text(json.dumps(
                {"exit_code": 0, "duration_seconds": .125}))
            (output / f"{name}.log").write_text("stage executed\n")
        if runner == "Linux" and profile == "native":
            (output / "gpu-environment.log").write_text("OpenGL renderer string: fixture Mesa\n")
            (output / "gles.log").write_text("1: Actual GLES driver: fixture Mesa / ES3.2\n")
        for _, discovery, junit in suites:
            (output / discovery).write_text(json.dumps({"tests": [{"name": "one"}, {"name": "two"}]}))
            (output / junit).write_text('<testsuite><testcase name="one" time=".2"/>'
                                       '<testcase name="two" time=".3"/></testsuite>')
        return source

    def summary(self, output):
        with patch.dict(os.environ, {"GITHUB_STEP_SUMMARY": ""}), redirect_stdout(io.StringIO()), \
                patch.object(report, "cache_provenance", return_value=None), \
                patch.object(report, "final_source_issues", return_value=[]):
            return report.summarize(output)

    def test_source_change_during_the_job_is_rejected(self):
        with patch.object(report, "git", side_effect=["e" * 40, " M engine/src/changed.cpp"]):
            issues = report.final_source_issues(self.source())
        self.assertEqual(len(issues), 2)
        self.assertIn("HEAD changed", issues[0])
        self.assertIn("Tracked source changed", issues[1])

    def test_complete_native_macos_has_measured_cases_but_no_gpu_claim(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            self.fixture(output)
            self.assertEqual(self.summary(output), 0)
            summary = json.loads((output / "summary.json").read_text())
            self.assertEqual(summary["suites"]["cpu"]["cases"][1]["duration_seconds"], .3)
            self.assertFalse((output / "native.xml").exists())
            self.assertIn("GPU/window execution is not configured", (output / "summary.md").read_text())

    def test_same_count_wrong_names_and_duplicates_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            self.fixture(output)
            (output / "cpu.xml").write_text('<testsuite><testcase name="one" time=".1"/>'
                                            '<testcase name="one" time=".2"/></testsuite>')
            self.assertEqual(self.summary(output), 1)
            self.assertFalse(json.loads((output / "summary.json").read_text())[
                "suites"]["cpu"]["inventory_matches"])

    def test_skipped_unmeasured_and_failed_junit_never_pass(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            for child in ('<skipped/>', '<failure/>', '<error/>'):
                self.fixture(output)
                (output / "cpu.xml").write_text('<testsuite><testcase name="one" time=".1">' +
                    child + '</testcase><testcase name="two" time=".2"/></testsuite>')
                self.assertEqual(self.summary(output), 1)
            self.fixture(output)
            (output / "cpu.xml").write_text('<testsuite><testcase name="one"/></testsuite>')
            self.assertEqual(self.summary(output), 1)

    def test_missing_stages_missing_source_and_malformed_stage_retain_report(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            self.assertEqual(self.summary(output), 1)
            self.assertTrue((output / "summary.json").exists())
            self.fixture(output)
            (output / "lint.json").unlink()
            (output / "cpu.json").write_text('{bad-json')
            self.assertEqual(self.summary(output), 1)
            self.assertIn("invalid evidence", (output / "summary.md").read_text())

    def test_linux_native_reports_each_executable_and_missing_gpu_is_not_green(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            self.fixture(output, "Linux")
            self.assertEqual(self.summary(output), 0)
            native = report.test_cases(ET.parse(output / "native.xml").getroot())
            self.assertEqual(len(native), len(report.render_cases()) - 1)
            self.assertTrue(all(case["duration_seconds"] == .125 for case in native))
            (output / "gpu-hdr.json").unlink()
            self.assertEqual(self.summary(output), 1)
            native = report.test_cases(ET.parse(output / "native.xml").getroot())
            self.assertEqual(report.counts(native)["skipped"], 1)

    def test_success_without_actual_driver_evidence_is_not_coverage(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            self.fixture(output, "Linux")
            (output / "gles.log").write_text("stage executed\n")
            self.assertEqual(self.summary(output), 1)
            self.assertIn("omitted its actual driver", (output / "summary.md").read_text())

    def test_native_failure_excerpt_cannot_corrupt_junit_with_control_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            self.fixture(output, "Linux")
            (output / "gpu-hdr.json").write_text(json.dumps(
                {"exit_code": 1, "duration_seconds": .25}))
            (output / "gpu-hdr.log").write_bytes(b"diagnostic\x00with\x01control bytes")
            self.assertEqual(self.summary(output), 1)
            self.assertEqual(report.counts(report.test_cases(ET.parse(
                output / "native.xml").getroot()))["failed"], 1)

    def test_sanitizer_profile_has_scoped_inventory_no_graphics_claim(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            self.fixture(output, "Linux", "sanitizers")
            self.assertEqual(self.summary(output), 0)
            self.assertIn("no GPU", (output / "summary.md").read_text())
            self.assertEqual(list(json.loads((output / "summary.json").read_text())["suites"]), ["cpu"])

    def test_provider_upload_is_validated_and_incomplete_report_still_fails(self):
        with tempfile.TemporaryDirectory() as directory, redirect_stdout(io.StringIO()):
            output = Path(directory)
            source = self.fixture(output)
            self.assertEqual(self.summary(output), 0)
            link = source["run_url"] + "/artifacts/123"
            with patch.dict(os.environ, {"GITHUB_STEP_SUMMARY": ""}):
                self.assertEqual(report.publication(output, "123", link, "success"), 0)
                for artifact, url, outcome in (("", link, "success"), ("123", link, "failure"),
                                                ("123", "https://unrelated/artifacts/123", "success")):
                    with self.assertRaises(ValueError):
                        report.publication(output, artifact, url, outcome)
                (output / "build.json").unlink()
                self.assertEqual(self.summary(output), 1)
                self.assertEqual(report.publication(output, "123", link, "success"), 1)


if __name__ == "__main__":
    unittest.main()
