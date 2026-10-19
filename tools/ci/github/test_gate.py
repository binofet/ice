"""Offline native API fixtures. Never contact GitHub or publish a real status/review."""

from copy import deepcopy
from pathlib import Path
import io
import json
import tempfile
import unittest
from unittest.mock import patch
import zipfile

import gate

HEAD, BASE, MERGE = "a" * 40, "b" * 40, "c" * 40


def pull(repo="ice", head=HEAD, base=BASE):
    config = gate.POLICIES[repo]
    native = {"id": config["id"], "full_name": "binofet/" + repo,
              "private": config["private"], "fork": False}
    return {"number": 7, "state": "open", "draft": False,
            "head": {"sha": head, "repo": native},
            "base": {"sha": base, "repo": native, "ref": config["branch"]}}


def review(repo="ice", **change):
    value = {"id": 99, "user": {"login": "binofet", "type": "User"}, "state": "COMMENTED",
             "commit_id": HEAD, "pull_request_url": f"https://api.github.com/repos/binofet/{repo}/pulls/7",
             "body": gate.review_body(repo, 7, HEAD, BASE, "Reviewed the actual diff and focused results.")}
    value.update(change)
    return value


def zipped(files):
    stream = io.BytesIO()
    with zipfile.ZipFile(stream, "w", zipfile.ZIP_DEFLATED) as archive:
        for name, value in files.items():
            # The fake provider digest and download must describe identical bytes,
            # even when the real clock crosses ZIP's two-second timestamp boundary.
            entry = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(entry, value)
    return stream.getvalue()


def evidence(repo="ice", os="Linux", cpp="20", profile="native"):
    source = {"event": "pull_request", "source_head": HEAD, "tested_base": BASE,
              "tested_candidate_head": HEAD, "tested_commit": MERGE, "runner_os": os,
              "run_url": f"https://github.com/binofet/{repo}/actions/runs/123", "run_attempt": "1",
              "source_validated": True, "source_issues": [], "identity_verified": True,
              "identity_problems": []}
    if repo != "metagen":
        source.update(cpp=cpp, profile=profile)
    names, suites = gate.stage_plan(repo, source)
    summary = {"source": source, "report_incomplete": False, "issues": [], "problems": [],
               "stages": {name: {"exit_code": 0, "duration_seconds": .1} for name in names}, "suites": {}}
    prefix = "ci-evidence/" if repo == "metagen" else ""
    files = {}
    xml = '<testsuite><testcase name="real-case" classname="CPU" time=".01"/></testsuite>'
    if repo == "metagen":
        summary["python"] = {"methods": {"tests": 1, "failures": 0, "errors": 0, "skipped": 0},
                             "subtests": {"tests": 0, "failures": 0, "errors": 0, "skipped": 0},
                             "fixtures": {"tests": 0, "failures": 0, "errors": 0, "skipped": 0},
                             "source_id": gate.sha(json.dumps(source).encode()),
                             "discovered_methods": 1, "executed_methods": 1}
        files[prefix + "python-methods.xml"] = xml
        files[prefix + "python-subtests.xml"] = "<testsuite/>"
        files[prefix + "python-fixtures.xml"] = "<testsuite/>"
        files[prefix + "executables.xml"] = '<testsuite>' + ''.join(
            '<testcase name="proof' + str(i) + '" time=".1"/>' for i in range(3)) + '</testsuite>'
        summary["executable_counts"] = {"tests": 3, "failures": 0, "errors": 0, "skipped": 0}
        files["proof/generated.json"] = "{}"
        files[prefix + "generated-artifacts.json"] = json.dumps([
            {"path": "build/proof/generated.json", "bytes": 2, "sha256": gate.sha(b"{}") }])
    for name in suites:
        summary["suites"][name] = {"counts": {"tests": 1, "passed": 1, "failed": 0, "error": 0, "skipped": 0}, "inventory_matches": True,
                                   "cases": [{"name": "real-case"}]}
        file = "gpu.xml" if name == "native" and repo == "ignition" else name + ".xml"
        files[file] = xml
    files[prefix + "source.json"] = json.dumps(source)
    files[prefix + "summary.json"] = json.dumps(summary)
    return files


class FakeAPI:
    def __init__(self, repo="ice"):
        self.repo = repo
        self.pr = pull(repo)
        self.reviews = [review(repo)]
        config = gate.POLICIES[repo]
        self.workflow = {"id": 42, "path": ".github/workflows/" + config["workflow"], "state": "active"}
        self.runs = [{"id": 123, "event": "pull_request", "workflow_id": 42, "head_sha": HEAD,
                      "repository": {"id": config["id"]}, "head_repository": {"id": config["id"]},
                      "run_attempt": 1, "status": "completed", "conclusion": "success"}]
        self.jobs = [{"name": n, "status": "completed", "conclusion": "success"} for n in config["jobs"]]
        if repo == "ice":
            self.jobs += [{"name": name, "status": "completed", "conclusion": "skipped"} for name in
                          ("Linux / Xvfb readiness diagnostic only", "Refuse invalid manual quality route")]
        identities = ([('Linux', '', 'native'), ('macOS', '', 'native')] if repo == "metagen" else
                      [(o, str(c), 'native') for o in ('Linux', 'macOS') for c in (20, 23)])
        if repo == "ice":
            identities.append(('Linux', '23', 'sanitizers'))
        self.files = {index + 1: evidence(repo, *identity) for index, identity in enumerate(identities)}
        self.artifacts = [{"id": index, "name": repo + "-fixture" + str(index) + "-1",
                           "expired": False, "size_in_bytes": 1000,
                           "digest": "sha256:" + gate.sha(zipped(self.files[index])),
                           "workflow_run": {"id": 123, "repository_id": config["id"],
                                            "head_repository_id": config["id"], "head_sha": HEAD}}
                          for index in self.files]
        self.statuses = []
        self.parents = [BASE, HEAD]
        self.change_before_write = False
        self.withdraw_on_zip = False
        self.withdraw_after_first_success = False

    def call(self, path, data=None, binary=False):
        if "/statuses/" in path:
            self.statuses.append((path, data))
            if self.withdraw_after_first_success and data["state"] == "success":
                self.reviews = [review(self.repo, state="DISMISSED")]
            return {"id": len(self.statuses)}
        if path.endswith("/pulls/7"):
            if self.change_before_write and self.statuses:
                return pull(self.repo, base="d" * 40)
            return deepcopy(self.pr)
        if "/actions/workflows/" in path:
            return self.workflow
        if "/git/commits/" in path:
            return {"parents": [{"sha": value} for value in self.parents]}
        if path.endswith("/zip"):
            if self.withdraw_on_zip:
                self.reviews = [review(self.repo, state="DISMISSED")]
            return zipped(self.files[int(path.split("/")[-2])])
        raise AssertionError(path)

    def pages(self, path, field=None, maximum=500):
        if path.endswith("/reviews"):
            return deepcopy(self.reviews)
        if "/runs?" in path:
            return deepcopy(self.runs)
        if path.endswith("/jobs"):
            return deepcopy(self.jobs)
        if path.endswith("/artifacts"):
            return deepcopy(self.artifacts)
        raise AssertionError(path)


class GateTests(unittest.TestCase):
    def test_all_three_fixed_repositories_accept_realistic_complete_evidence(self):
        for repo in gate.POLICIES:
            with self.subTest(repo=repo):
                result = gate.gate(FakeAPI(repo), repo, 7)
                self.assertEqual(result["reviewed-head"]["state"], "success")
                self.assertEqual(result["source-quality"]["state"], "success", result)
                self.assertFalse(result["merge_performed"])

    def test_ice_exact_five_quality_and_two_skipped_manual_jobs_accept(self):
        api = FakeAPI()
        self.assertEqual(len(api.jobs), 7)
        self.assertEqual(len(api.artifacts), 5)
        result = gate.gate(api, "ice", 7)
        self.assertEqual(result["source-quality"]["state"], "success", result)

    def test_ice_auxiliary_inventory_rejects_missing_duplicate_or_unknown_rows(self):
        for mutation in (lambda a: a.jobs.pop(),
                         lambda a: a.jobs.append(deepcopy(a.jobs[-1])),
                         lambda a: a.jobs.append({"name": "unexpected auxiliary", "status": "completed",
                                                  "conclusion": "skipped"})):
            with self.subTest(mutation=mutation):
                api = FakeAPI()
                mutation(api)
                self.assertEqual(gate.gate(api, "ice", 7)["source-quality"]["state"], "failure")

    def test_ice_manual_jobs_must_be_completed_and_skipped(self):
        for index in (5, 6):
            for status, conclusion in (("completed", "success"), ("completed", "failure"),
                                       ("completed", "cancelled"), ("completed", "neutral"),
                                       ("queued", None), ("in_progress", None)):
                with self.subTest(index=index, status=status, conclusion=conclusion):
                    api = FakeAPI()
                    api.jobs[index].update(status=status, conclusion=conclusion)
                    self.assertEqual(gate.gate(api, "ice", 7)["source-quality"]["state"], "failure")

    def test_ice_skipped_manual_jobs_cannot_replace_required_profile_or_artifact(self):
        for mutation in (lambda a: a.jobs[0].update(conclusion="skipped"),
                         lambda a: a.jobs.pop(0), lambda a: a.artifacts.pop(),
                         lambda a: a.artifacts.append(deepcopy(a.artifacts[-1]))):
            with self.subTest(mutation=mutation):
                api = FakeAPI()
                mutation(api)
                self.assertEqual(gate.gate(api, "ice", 7)["source-quality"]["state"], "failure")

    def test_manual_diagnostic_success_never_substitutes_for_quality(self):
        api = FakeAPI()
        for job in api.jobs[:5]:
            job["conclusion"] = "skipped"
        api.jobs[5]["conclusion"] = "success"
        self.assertEqual(gate.gate(api, "ice", 7)["source-quality"]["state"], "failure")

    def test_other_repositories_refuse_ice_manual_auxiliary_rows(self):
        for repo in ("metagen", "ignition"):
            with self.subTest(repo=repo):
                api = FakeAPI(repo)
                api.jobs.append({"name": "Linux / Xvfb readiness diagnostic only", "status": "completed",
                                 "conclusion": "skipped"})
                self.assertEqual(gate.gate(api, repo, 7)["source-quality"]["state"], "failure")

    def test_gate_contract_stage_is_required_for_every_repository_and_profile(self):
        for repo, profile in (("ice", "native"), ("ice", "sanitizers"),
                              ("metagen", "native"), ("ignition", "native")):
            with self.subTest(repo=repo, profile=profile):
                files = evidence(repo, profile=profile)
                prefix = "ci-evidence/" if repo == "metagen" else ""
                summary = json.loads(files[prefix + "summary.json"])
                del summary["stages"]["gate-contract"]
                files[prefix + "summary.json"] = json.dumps(summary)
                with self.assertRaisesRegex(ValueError, "required_stage_evidence_missing"):
                    gate.artifact_evidence(zipped(files), repo, gate.live_pr(pull(repo), repo, 7),
                                           {"id": 123, "run_attempt": 1})

    def test_fork_wrong_repository_or_target_is_never_admitted(self):
        for modify in [lambda p: p["head"]["repo"].update(fork=True),
                       lambda p: p["head"]["repo"].update(id=1),
                       lambda p: p["base"].update(ref="other"), lambda p: p.update(draft=True)]:
            api = FakeAPI()
            modify(api.pr)
            with self.assertRaises(ValueError):
                gate.gate(api, "ice", 7, True)
            self.assertEqual(api.statuses, [])

    def test_native_comment_review_is_not_an_issue_comment_or_self_authored_record(self):
        context = gate.live_pr(pull(), "ice", 7)
        self.assertEqual(gate.native_review([review()], context)["id"], 99)
        for value in [review(user={"login": "author", "type": "User"}),
                      review(pull_request_url="issue/7"), review(body="approved"),
                      review(commit_id="d" * 40)]:
            with self.assertRaises(ValueError):
                gate.native_review([value], context)

    def test_latest_review_edit_dismissal_rejection_removal_and_new_base_invalidate(self):
        context = gate.live_pr(pull(), "ice", 7)
        rejected = gate.review_body("ice", 7, HEAD, BASE, "Needs correction.", "rejected")
        stale = gate.review_body("ice", 7, HEAD, "d" * 40, "Reviewed an old base.")
        for change in [{"state": "DISMISSED"}, {"state": "CHANGES_REQUESTED"},
                       {"body": "marker removed"}, {"body": rejected}, {"body": stale},
                       {"body": review()["body"].replace("actual diff", "altered diff")}]:
            with self.subTest(change=change):
                latest = review(id=100, **change)
                with self.assertRaises(ValueError):
                    gate.native_review([review(), latest], context)

    def test_failed_cancelled_skipped_queued_or_missing_jobs_refuse_ci(self):
        for state in ("failure", "cancelled", "skipped", "timed_out", "neutral"):
            api = FakeAPI()
            api.jobs[0]["conclusion"] = state
            self.assertEqual(gate.gate(api, "ice", 7)["source-quality"]["state"], "failure")
        api = FakeAPI()
        api.jobs.pop()
        self.assertEqual(gate.gate(api, "ice", 7)["source-quality"]["state"], "failure")

    def test_newer_failed_run_or_push_run_does_not_revive_old_green(self):
        api = FakeAPI()
        api.runs.append(dict(api.runs[0], id=124, conclusion="failure"))
        self.assertEqual(gate.gate(api, "ice", 7)["source-quality"]["state"], "failure")
        api = FakeAPI()
        api.runs[0]["event"] = "push"
        self.assertEqual(gate.gate(api, "ice", 7)["source-quality"]["state"], "failure")

    def test_upload_missing_expired_source_summary_or_candidate_mismatch_refuses(self):
        mutations = [lambda a: a.artifacts.pop(), lambda a: a.artifacts[0].update(expired=True),
                     lambda a: a.files[1].pop("summary.json"),
                     lambda a: a.files[1].update({"source.json": json.dumps({"source_head": "d" * 40})}),
                     lambda a: setattr(a, "parents", ["d" * 40, HEAD])]
        for mutate in mutations:
            api = FakeAPI()
            mutate(api)
            self.assertEqual(gate.gate(api, "ice", 7)["source-quality"]["state"], "failure")

    def test_zero_skip_or_unmeasured_native_cases_refuse(self):
        for xml in ('<testsuite/>', '<testsuite><testcase name="x" time=".1"><skipped/></testcase></testsuite>',
                    '<testsuite><testcase name="x" time="nan"/></testsuite>'):
            api = FakeAPI()
            api.files[1]["cpu.xml"] = xml
            self.assertEqual(gate.gate(api, "ice", 7)["source-quality"]["state"], "failure")

    def test_unsafe_zip_or_extra_nonproject_file_refuses(self):
        for name in ("../secret.json", "/absolute.json", "other-repository/file.json", "credential.env"):
            with self.assertRaises(ValueError):
                gate.zip_files(zipped({name: "x"}), "ice")

    def test_fresh_api_change_before_status_write_never_publishes_success(self):
        api = FakeAPI()
        api.change_before_write = True
        with self.assertRaises(ValueError):
            gate.gate(api, "ice", 7, True)
        self.assertTrue(api.statuses)
        self.assertFalse(any(value["state"] == "success" for _, value in api.statuses))

    def test_publication_is_four_context_writes_and_never_merges(self):
        api = FakeAPI()
        result = gate.gate(api, "ice", 7, True)
        self.assertEqual([value["state"] for _, value in api.statuses], ["pending", "pending", "success", "success"])
        self.assertFalse(result["merge_performed"])

    def test_review_withdrawal_during_artifact_download_never_publishes_success(self):
        api = FakeAPI()
        api.withdraw_on_zip = True
        result = gate.gate(api, "ice", 7, True)
        self.assertEqual(result["reviewed-head"]["state"], "failure")
        self.assertEqual(result["source-quality"]["state"], "failure")
        self.assertFalse(any(row["state"] == "success" for _, row in api.statuses))

    def test_withdrawal_between_success_writes_revokes_earlier_success(self):
        api = FakeAPI()
        api.withdraw_after_first_success = True
        result = gate.gate(api, "ice", 7, True)
        final = {row["context"]: row["state"] for _, row in api.statuses}
        self.assertEqual(final, {"reviewed-head": "failure", "source-quality": "failure"})
        self.assertEqual(result["source-quality"]["state"], "failure")

    def test_log_header_allowance_does_not_expand_other_file_limits(self):
        raw = b"x" * (gate.MAX_FILE + 100)
        self.assertIn("build.log", gate.zip_files(zipped({"build.log": raw}), "ice"))
        with self.assertRaises(ValueError):
            gate.zip_files(zipped({"build.json": raw}), "ice")

    def test_reviewed_readonly_installation_binds_independently_selected_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            code, manifest = root / "gate.py", root / "install.json"
            code.write_bytes(b"reviewed code\n")
            manifest.write_bytes(gate.canonical({"format": "ice-family-gate-install-v1",
                "source_commit": HEAD, "files": {"gate.py": gate.sha(code.read_bytes())}}))
            code.chmod(0o400)
            manifest.chmod(0o400)
            with patch.object(gate, "__file__", str(code)):
                gate.execution(FakeAPI(), "ice", manifest, gate.sha(manifest.read_bytes()))
                with self.assertRaises(ValueError):
                    gate.execution(FakeAPI(), "ice", manifest, "e" * 64)

    def test_review_is_checked_after_source_authorization_immediately_before_success(self):
        api = FakeAPI()
        authorizations = []
        def authorize():
            authorizations.append(True)
            if len(authorizations) == 3:  # Two initial pending writes, then first success.
                api.reviews = [review(state="DISMISSED")]
        result = gate.gate(api, "ice", 7, True, authorize)
        self.assertEqual(result["reviewed-head"]["state"], "failure")
        self.assertFalse(any(row["state"] == "success" for _, row in api.statuses))

    def test_summary_links_exact_review_run_and_uploaded_artifacts(self):
        result = gate.gate(FakeAPI(), "ice", 7)
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "summary.md"
            with patch.dict("os.environ", {"GITHUB_STEP_SUMMARY": str(output)}):
                text = gate.write_summary([result])
            self.assertEqual(output.read_text(), text)
        for expected in (HEAD, BASE, "#pullrequestreview-99", "/actions/runs/123", "/artifacts/1",
                         MERGE, "No merge or release"):
            self.assertIn(expected, text)

    def test_templates_bind_fixed_repository_default_branch_and_quality_workflow(self):
        with tempfile.TemporaryDirectory() as directory:
            for repo, config in gate.POLICIES.items():
                output = Path(directory) / repo
                result = gate.generate_templates(repo, output)
                self.assertEqual(result["repository"], "binofet/" + repo)
                self.assertFalse(result["installed"])
                control = (output / "family-gate.yml").read_text()
                signal = (output / "family-review-signal.yml").read_text()
                self.assertIn("branches: [" + config["branch"] + "]", control)
                self.assertIn("workflows: [" + config["workflow_name"] + ", Family review signal]", control)
                self.assertIn("github.repository == 'binofet/" + repo + "'", control)
                self.assertIn("github.repository == 'binofet/" + repo + "'", signal)
                self.assertNotIn("__REPOSITORY__", control + signal)
                self.assertNotIn("pull_request.head", control + signal)
                with self.assertRaises(ValueError):
                    gate.generate_templates(repo, output)
            with self.assertRaises(ValueError):
                gate.generate_templates("untrusted/repo", Path(directory) / "other")

    def test_provider_fixture_digest_survives_a_wall_clock_change(self):
        with patch("zipfile.time.localtime", return_value=(2020, 1, 1, 0, 0, 0, 2, 1, -1)):
            api = FakeAPI("metagen")
        with patch("zipfile.time.localtime", return_value=(2030, 1, 1, 0, 0, 0, 1, 1, -1)):
            result = gate.gate(api, "metagen", 7)
        self.assertEqual(result["source-quality"]["state"], "success", result)

    def test_review_body_cli_preserves_the_authenticated_body_exactly(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "review.txt"
            path.write_text("Reviewed the exact CLI emission fix.\n")
            output = io.StringIO()
            with patch("sys.stdout", output):
                result = gate.main(["review-body", "--repo", "ice", "--pr", "7",
                                    "--head", HEAD, "--base", BASE, "--text-file", str(path)])
            self.assertEqual(result, 0)
            self.assertEqual(output.getvalue(), gate.review_body("ice", 7, HEAD, BASE,
                                                                 path.read_text()))
            parsed = gate.native_review([review(body=output.getvalue())],
                                        gate.live_pr(pull(), "ice", 7))
            self.assertEqual(parsed["id"], 99)

    def test_empty_workflow_pr_list_uses_association_api_but_filters_current_head(self):
        class Associated(FakeAPI):
            def pages(self, path, field=None, maximum=500):
                if "/commits/" in path:
                    return [{"number": 7}, {"number": 8}]
                return super().pages(path, field, maximum)
            def call(self, path, data=None, binary=False):
                if path.endswith("/pulls/8"):
                    value = pull(head="d" * 40)
                    value["number"] = 8
                    return value
                return super().call(path, data, binary)
        self.assertEqual(gate.refresh_numbers(Associated(), "ice", {"workflow_run": {
            "name": "Engine quality", "head_sha": HEAD, "pull_requests": []}}), [7])


if __name__ == "__main__":
    unittest.main(verbosity=2)
