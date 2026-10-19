#!/usr/bin/env python3
"""Trusted family GitHub gates. No PR checkout/import, shell, merge or release capability.

Independently authored using the reviewed Cosmotomata control/review pattern;
no Cosmotomata code, identities, policies, pins or credentials are copied.
"""

from collections import Counter
from pathlib import Path, PurePosixPath
import argparse
import hashlib
import io
import json
import math
import os
import re
import stat
import subprocess
import sys
import tempfile
import time
import urllib.parse
import xml.etree.ElementTree as ET
import zipfile

ROOT = "https://api.github.com"
POLICIES = {
    "ice": {"id": 40782766, "branch": "master", "private": False, "workflow": "engine-quality.yml", "workflow_name": "Engine quality",
            "jobs": [f"{os} / optimized C++{cpp}" for os in ("ubuntu-24.04", "macos-15") for cpp in (20, 23)]
                    + ["Ubuntu / scoped ASan+UBSan C++23"],
            "skipped_jobs": ["Linux / Xvfb readiness diagnostic only", "Refuse invalid manual quality route"]},
    "metagen": {"id": 1421012730, "branch": "main", "private": True, "workflow": "conformance.yml", "workflow_name": "Standalone conformance",
                "jobs": [f"Metagen conformance ({os})" for os in ("ubuntu-24.04", "macos-15")]},
    "ignition": {"id": 1420999231, "branch": "main", "private": True, "workflow": "quality.yml", "workflow_name": "Ignition quality",
                 "jobs": [f"{os} / optimized C++{cpp}" for os in ("ubuntu-24.04", "macos-15") for cpp in (20, 23)]}}
MARKER = "<!-- ice-family-native-review-v1\n"
OID = re.compile(r"[0-9a-f]{40}\Z")
HASH = re.compile(r"[0-9a-f]{64}\Z")
MAX_ZIP = 64 * 1024 * 1024
MAX_FILE = 8 * 1024 * 1024


def need(value, code):
    if not value:
        raise ValueError(code)


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"), allow_nan=False).encode() + b"\n"


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def decode(raw):
    def unique(pairs):
        value = {}
        for key, item in pairs:
            need(key not in value, "duplicate_json_key")
            value[key] = item
        return value
    return json.loads(raw, object_pairs_hook=unique,
                      parse_constant=lambda _: (_ for _ in ()).throw(ValueError("nonfinite_json")))


def policy(repo):
    repo = repo.removeprefix("binofet/")
    need(repo in POLICIES, "unknown_repository")
    return repo, POLICIES[repo]


def live_pr(value, repo, number):
    _, config = policy(repo)
    need(value.get("number") == number and value.get("state") == "open" and value.get("draft") is False,
         "open_nondraft_pr_required")
    for side in ("head", "base"):
        part = value.get(side, {})
        native = part.get("repo", {})
        need(native.get("id") == config["id"] and native.get("full_name") == "binofet/" + repo
             and native.get("fork") is False and native.get("private") is config["private"]
             and isinstance(part.get("sha"), str) and OID.fullmatch(part["sha"]), "same_repository_exact_source_required")
    need(value["base"].get("ref") == config["branch"], "wrong_target_branch")
    return {"repo": repo, "pr": number, "head": value["head"]["sha"], "base": value["base"]["sha"]}


def review_body(repo, number, head, base, text, decision="approved"):
    repo, config = policy(repo)
    need(type(number) is int and number > 0 and OID.fullmatch(head) and OID.fullmatch(base)
         and decision in {"approved", "rejected"} and text.strip() and MARKER not in text, "invalid_review_payload")
    record = {"format": "ice-family-native-review-v1", "repository": "binofet/" + repo,
              "repository_id": config["id"], "pr": number, "head": head, "base": base,
              "decision": decision, "review_sha256": sha(text.strip().encode())}
    body = text.strip() + "\n\n" + MARKER + canonical(record).decode().strip() + "\n-->\n"
    need(len(body.encode()) <= 32768, "review_body_too_large")
    return body


def native_review(rows, context):
    """Latest owner native review wins, even if its marker was removed or it was dismissed."""
    repo, config = policy(context["repo"])
    owners = [r for r in rows if r.get("user", {}).get("login") == "binofet"
              and r.get("user", {}).get("type") == "User"]
    need(owners and all(type(r.get("id")) is int for r in owners), "native_owner_review_missing")
    latest = max(owners, key=lambda r: r["id"])
    need(latest.get("state") in {"COMMENTED", "APPROVED"}
         and latest.get("commit_id") == context["head"]
         and latest.get("pull_request_url") == ROOT + f"/repos/binofet/{repo}/pulls/{context['pr']}",
         "latest_review_withdrawn_or_wrong_head")
    body = latest.get("body")
    need(isinstance(body, str) and len(body.encode()) <= 32768 and body.count(MARKER) == 1
         and body.endswith("\n-->\n"), "latest_review_marker_invalid")
    text, encoded = body.split(MARKER)
    record = decode(encoded[:-5])
    need(record == {"format": "ice-family-native-review-v1", "repository": "binofet/" + repo,
                    "repository_id": config["id"], "pr": context["pr"], "head": context["head"],
                    "base": context["base"], "decision": "approved", "review_sha256": sha(text.strip().encode())}
         and text.strip(), "latest_review_rejected_stale_or_edited")
    return {"id": latest["id"], "body_sha256": sha(body.encode()), "head": context["head"], "base": context["base"]}


def zip_files(raw, repo):
    need(len(raw) <= MAX_ZIP, "artifact_download_too_large")
    files, total = {}, 0
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        need(len(archive.infolist()) <= 512, "artifact_inventory_too_large")
        for entry in archive.infolist():
            name = entry.filename
            path = PurePosixPath(name)
            need(not path.is_absolute() and ".." not in path.parts and "\\" not in name
                 and str(path) == name.rstrip("/") and not stat.S_ISLNK(entry.external_attr >> 16),
                 "unsafe_artifact_member")
            if entry.is_dir():
                continue
            limit = MAX_FILE + 4096 if path.suffix.lower() == ".log" else MAX_FILE
            need(name not in files and entry.file_size <= limit and not entry.flag_bits & 1,
                 "duplicate_encrypted_or_large_artifact_member")
            if repo == "metagen":
                need(path.parts[0] in {"ci-evidence", "proof", "data-lab"}, "unexpected_metagen_artifact_root")
            else:
                need(len(path.parts) == 1 or path.parts[0] == "images", "unexpected_artifact_directory")
            need(path.suffix.lower() in {".json", ".xml", ".md", ".log", ".ppm", ".png", ".h", ".cpp", ".rs", ".mdb", ".mcp"}
                 or (repo == "metagen" and path.parts[0] == "proof" and path.suffix.lower() in {".txt", ".ini"}),
                 "unexpected_artifact_type")
            total += entry.file_size
            need(total <= MAX_ZIP, "artifact_expansion_too_large")
            files[name] = archive.read(entry)
    return files


def cases(raw, measured=True):
    rows = list(ET.fromstring(raw).iter("testcase"))
    for row in rows:
        need(row.get("name") and all(row.find(k) is None for k in ("failure", "error", "skipped")),
             "junit_failed_errored_or_skipped")
        if measured:
            seconds = float(row.get("time", "nan"))
            need(math.isfinite(seconds) and seconds >= 0, "junit_time_unmeasured")
    identities = [(row.get("classname", ""), row.get("name")) for row in rows]
    need(len(identities) == len(set(identities)), "duplicate_junit_case_identity")
    return rows


def stage_plan(repo, source):
    common = {"evidence", "gate-contract", "dependencies", "configure", "build", "cpu-discovery", "cpu"}
    suites = {"cpu"}
    if repo == "metagen":
        return {"helper-tests", "gate-contract", "dependencies", "core", "formats", "rust-toolchain", "rust",
                "csharp-smoke", "csharp-core", "rust-consumer"}, set()
    native = source.get("profile", "native") != "sanitizers"
    linux = source["runner_os"] == "Linux"
    if repo == "ice" and native:
        common |= {"path-policy", "quality-tools", "consumer-cache", "metagen-configure", "metagen-build",
                   "metagen-discovery", "metagen", "rust", "format", "api", "lint"}
        suites.add("metagen")
    if repo == "ignition":
        common.add("ice")
    if linux and native:
        common |= {"gles-configure", "gles-build", "gles-discovery", "gles"}
        suites |= {"gles", "native"}
        if repo == "ice":
            common |= {"gpu-environment", "native-x11", "native-window", "gpu-gl", "gpu-switch",
                       "gpu-reload", "gpu-mesh", "gpu-textured", "gpu-skinned", "gpu-hdr"}
        else:
            common |= {"gpu-environment", "gpu-labels", "gpu-detached", "gpu-scene", "gpu-model",
                       "gpu-post", "gpu-splash", "quest-configure", "quest-build", "quest-discovery", "quest"}
            suites.add("quest")
    return common, suites


def artifact_evidence(raw, repo, context, run):
    files = zip_files(raw, repo)
    prefix = "ci-evidence/" if repo == "metagen" else ""
    source = decode(files[prefix + "source.json"])
    summary = decode(files[prefix + "summary.json"])
    need(summary.get("source") == source and source.get("event") == "pull_request"
         and source.get("source_head") == context["head"] and source.get("tested_base") == context["base"]
         and source.get("tested_candidate_head") == context["head"]
         and OID.fullmatch(source.get("tested_commit", ""))
         and source.get("run_url") == f"https://github.com/binofet/{repo}/actions/runs/{run['id']}",
         "artifact_source_summary_run_mismatch")
    if repo != "metagen":
        need(source.get("run_attempt") == str(run["run_attempt"]), "artifact_attempt_mismatch")
    if repo == "metagen":
        need(source.get("identity_verified") is True and source.get("identity_problems") == []
             and summary.get("problems") == [], "metagen_evidence_incomplete")
        primary = "python-methods.xml"
    else:
        need(source.get("source_validated") is True and source.get("source_issues") == []
             and summary.get("report_incomplete") is False and summary.get("issues") == [], "evidence_incomplete")
        primary = "cpu.xml"
    required, required_suites = stage_plan(repo, source)
    stages = summary.get("stages")
    need(isinstance(stages, dict) and required <= stages.keys(), "required_stage_evidence_missing")
    for stage in stages.values():
        need(type(stage.get("exit_code")) is int and stage["exit_code"] == 0
             and type(stage.get("duration_seconds")) in (int, float)
             and math.isfinite(stage["duration_seconds"]) and stage["duration_seconds"] >= 0,
             "stage_failed_or_unmeasured")
    primary_cases = cases(files[prefix + primary])
    need(primary_cases, "zero_primary_tests")
    for name, content in files.items():
        if name.startswith(prefix) and name.endswith(".xml"):
            cases(content, measured=name == prefix + primary or repo != "metagen")
    if repo != "metagen":
        suites = summary.get("suites")
        need(isinstance(suites, dict) and set(suites) == required_suites, "suite_summary_inventory_differs")
        for name, suite in suites.items():
            xml = "gpu.xml" if name == "native" and repo == "ignition" else name + ".xml"
            actual = cases(files[prefix + xml])
            need(actual and suite.get("counts") == {"tests": len(actual), "passed": len(actual),
                 "failed": 0, "error": 0, "skipped": 0}, "suite_count_mismatch")
            if name != "native":
                need(suite.get("inventory_matches") is True
                     and Counter(c.get("name") for c in actual) == Counter(c["name"] for c in suite["cases"]),
                     "suite_inventory_mismatch")
    else:
        methods = summary.get("python", {}).get("methods", {})
        need(methods == {"tests": len(primary_cases), "failures": 0, "errors": 0, "skipped": 0}
             and summary["python"].get("discovered_methods") == len(primary_cases)
             and summary["python"].get("executed_methods") == len(primary_cases), "python_method_inventory_mismatch")
        need(summary["python"].get("source_id") == sha(files[prefix + "source.json"]), "python_source_identity_mismatch")
        for group in ("methods", "subtests", "fixtures"):
            actual = cases(files[prefix + "python-" + group + ".xml"], measured=group == "methods")
            need(summary["python"].get(group) == {"tests": len(actual), "failures": 0, "errors": 0, "skipped": 0},
                 "python_observed_counts_mismatch")
        executable = cases(files[prefix + "executables.xml"])
        need(len(executable) == 3 and summary.get("executable_counts") == {
            "tests": 3, "failures": 0, "errors": 0, "skipped": 0}, "executable_counts_mismatch")
        inventory = decode(files[prefix + "generated-artifacts.json"])
        need(isinstance(inventory, list) and 0 < len(inventory) <= 256, "generated_inventory_missing")
        expected = set()
        for row in inventory:
            name = row.get("path", "")
            need(name.startswith(("build/proof/", "build/data-lab/")), "generated_path_outside_project")
            name = name.removeprefix("build/")
            need(name not in expected and name in files and row.get("bytes") == len(files[name])
                 and row.get("sha256") == sha(files[name]), "generated_file_hash_or_size_differs")
            expected.add(name)
        need(expected == {name for name in files if name.startswith(("proof/", "data-lab/"))},
             "generated_file_inventory_differs")
    identity = (source["runner_os"], str(source.get("cpp", "")), source.get("profile", "native"))
    return {"identity": identity, "candidate": source["tested_commit"], "primary_tests": len(primary_cases),
            "zip_sha256": sha(raw), "source_sha256": sha(files[prefix + "source.json"]),
            "summary_sha256": sha(files[prefix + "summary.json"])}


class API:
    """Use the caller's existing gh account/token without inspecting or logging credentials."""
    def call(self, path, data=None, binary=False):
        command = ["gh", "api", "--hostname", "github.com", "-H", "Accept: application/vnd.github+json", path]
        if data is not None:
            command += ["--method", "POST", "--input", "-"]
        environment = dict(os.environ)
        environment.pop("GH_DEBUG", None)
        with tempfile.TemporaryFile() as output, tempfile.TemporaryFile() as errors:
            process = subprocess.Popen(command, stdin=subprocess.PIPE if data is not None else subprocess.DEVNULL,
                                       stdout=output, stderr=errors, env=environment)
            if data is not None:
                process.stdin.write(canonical(data))
                process.stdin.close()
            start = time.monotonic()
            while process.poll() is None:
                if output.tell() > MAX_ZIP or errors.tell() > 65536 or time.monotonic() - start > 90:
                    process.kill()
                    process.wait()
                    raise ValueError("bounded_github_request_exceeded")
                time.sleep(.05)
            need(process.returncode == 0, "github_request_failed")
            output.seek(0)
            raw = output.read(MAX_ZIP + 1)
            need(len(raw) <= MAX_ZIP, "github_response_too_large")
            return raw if binary else decode(raw)

    def pages(self, path, field=None, maximum=500):
        result = []
        for page in range(1, maximum // 100 + 2):
            value = self.call(path + ("&" if "?" in path else "?") + f"per_page=100&page={page}")
            rows = value[field] if field else value
            need(isinstance(rows, list) and len(rows) <= 100, "invalid_github_page")
            result += rows
            need(len(result) <= maximum, "github_inventory_bound_exceeded")
            if len(rows) < 100:
                return result
        raise ValueError("github_page_bound_exceeded")


def quality(api, context):
    repo, config = policy(context["repo"])
    base = f"repos/binofet/{repo}"
    workflow = api.call(base + "/actions/workflows/" + config["workflow"])
    need(workflow.get("path") == ".github/workflows/" + config["workflow"] and workflow.get("state") == "active",
         "quality_workflow_not_active")
    runs = api.pages(base + f"/actions/workflows/{workflow['id']}/runs?event=pull_request&head_sha={context['head']}", "workflow_runs")
    runs = [r for r in runs if r.get("event") == "pull_request" and r.get("workflow_id") == workflow["id"]
            and r.get("head_repository", {}).get("id") == config["id"]
            and r.get("repository", {}).get("id") == config["id"]]
    need(runs, "current_pr_quality_run_missing")
    run = max(runs, key=lambda r: r["id"])
    need(run.get("head_sha") == context["head"] and run.get("status") == "completed"
         and run.get("conclusion") == "success", "latest_quality_run_not_successful")
    jobs = api.pages(base + f"/actions/runs/{run['id']}/attempts/{run['run_attempt']}/jobs", "jobs")
    skipped_jobs = config.get("skipped_jobs", [])
    need(Counter(j.get("name") for j in jobs) == Counter(config["jobs"] + skipped_jobs)
         and all(j.get("status") == "completed"
                 and j.get("conclusion") == ("skipped" if j.get("name") in skipped_jobs else "success")
                 for j in jobs), "required_jobs_missing_failed_cancelled_or_skipped")
    artifacts = api.pages(base + f"/actions/runs/{run['id']}/artifacts", "artifacts", maximum=100)
    if repo != "metagen":
        artifacts = [a for a in artifacts if a.get("name", "").endswith("-" + str(run["run_attempt"]))]
    need(len(artifacts) == len(config["jobs"]), "artifact_inventory_missing_or_extra")
    evidence = []
    for artifact in artifacts:
        need(artifact.get("expired") is False and type(artifact.get("id")) is int
             and 0 < artifact.get("size_in_bytes", 0) <= MAX_ZIP
             and artifact.get("name", "").startswith(repo + "-"), "uploaded_artifact_invalid")
        raw = api.call(base + f"/actions/artifacts/{artifact['id']}/zip", binary=True)
        need(artifact.get("digest") == "sha256:" + sha(raw), "provider_artifact_digest_differs")
        uploaded = artifact.get("workflow_run", {})
        need(uploaded.get("id") == run["id"] and uploaded.get("repository_id") == config["id"]
             and uploaded.get("head_repository_id") == config["id"]
             and uploaded.get("head_sha") == context["head"], "artifact_upload_run_binding_differs")
        item = artifact_evidence(raw, repo, context, run)
        item["artifact_id"] = artifact["id"]
        item["provider_digest"] = artifact["digest"]
        evidence.append(item)
    expected = ({("Linux", "", "native"), ("macOS", "", "native")} if repo == "metagen" else
                {(os, str(cpp), "native") for os in ("Linux", "macOS") for cpp in (20, 23)})
    if repo == "ice":
        expected.add(("Linux", "23", "sanitizers"))
    need(Counter(tuple(e["identity"]) for e in evidence) == Counter(expected), "matrix_evidence_inventory_differs")
    candidates = {e["candidate"] for e in evidence}
    need(len(candidates) == 1, "matrix_candidate_mismatch")
    candidate = candidates.pop()
    commit = api.call(base + "/git/commits/" + candidate)
    need([p.get("sha") for p in commit.get("parents", [])] == [context["base"], context["head"]],
         "quality_tested_an_old_base_or_wrong_candidate")
    return {"run_id": run["id"], "attempt": run["run_attempt"], "candidate": candidate, "artifacts": evidence}


def execution(api, repo, install=None, expected=None):
    """Authorize only default-branch checkout or separately reviewed read-only installation."""
    _, config = policy(repo)
    if install is not None:
        need(expected and HASH.fullmatch(expected), "independent_install_manifest_digest_required")
        raw = install.read_bytes()
        need(len(raw) <= 65536 and sha(raw) == expected, "installed_manifest_differs")
        manifest = decode(raw)
        need(manifest.get("format") == "ice-family-gate-install-v1" and OID.fullmatch(manifest.get("source_commit", ""))
             and manifest.get("files") == {"gate.py": sha(Path(__file__).read_bytes())}
             and Path(__file__).resolve().parent == install.resolve().parent,
             "installed_source_differs")
        need(not install.stat().st_mode & 0o222 and not Path(__file__).stat().st_mode & 0o222,
             "installation_must_be_readonly")
        return
    root = Path(__file__).resolve().parents[3]
    actual = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
    branch = api.call(f"repos/binofet/{repo}/git/ref/heads/{config['branch']}")
    need(actual == branch["object"]["sha"] and os.environ.get("GITHUB_REPOSITORY") == "binofet/" + repo,
         "trusted_default_branch_source_required")
    need(not subprocess.check_output(["git", "-C", str(root), "status", "--porcelain", "--untracked-files=no"], text=True),
         "trusted_source_is_dirty")


def gate(api, repo, number, publish=False, authorize=lambda: None):
    context = live_pr(api.call(f"repos/binofet/{repo}/pulls/{number}"), repo, number)
    def status(name, state, description):
        if publish:
            authorize()
            need(live_pr(api.call(f"repos/binofet/{repo}/pulls/{number}"), repo, number) == context,
                 "pr_changed_before_status_publication")
            if state == "success":
                fresh_review()
            api.call(f"repos/binofet/{repo}/statuses/{context['head']}",
                     {"context": name, "state": state, "description": description[:140],
                      "target_url": f"https://github.com/binofet/{repo}/pull/{number}"})
    results = {"context": context, "merge_performed": False, "published": publish}
    for name in ("reviewed-head", "source-quality"):
        status(name, "pending", "Checking exact current head/base and native evidence")
    review_check = lambda: native_review(api.pages(f"repos/binofet/{repo}/pulls/{number}/reviews"), context)
    for name, check in [("reviewed-head", review_check), ("source-quality", lambda: quality(api, context))]:
        try:
            evidence = check()
            results[name] = {"state": "success", "evidence": evidence}
        except (ValueError, KeyError, TypeError, OSError, ET.ParseError, zipfile.BadZipFile) as error:
            results[name] = {"state": "failure", "diagnostic": str(error)[:2000]}
    expected_review = results["reviewed-head"].get("evidence")
    def fresh_review():
        need(expected_review is not None and expected_review == review_check(),
             "native_review_changed_during_collection_or_publication")
    # Collect expensive artifacts FIRST. No earlier approval success may survive a
    # withdrawal while downloads run, or authorize a later CI success from that run.
    if expected_review is not None:
        try:
            fresh_review()
        except (ValueError, KeyError, TypeError, OSError) as error:
            for name in ("reviewed-head", "source-quality"):
                results[name] = {"state": "failure", "diagnostic": str(error)[:2000]}
    elif publish:
        results["source-quality"] = {"state": "failure", "diagnostic": "current_approval_required_before_ci_success"}
    if publish:
        try:
            for name in ("reviewed-head", "source-quality"):
                status(name, results[name]["state"], results[name].get("diagnostic", "Exact current native evidence verified"))
            if expected_review is not None and any(results[n]["state"] == "success" for n in ("reviewed-head", "source-quality")):
                fresh_review()
        except (ValueError, KeyError, TypeError, OSError) as error:
            for name in ("reviewed-head", "source-quality"):
                results[name] = {"state": "failure", "diagnostic": str(error)[:2000]}
                status(name, "failure", str(error)[:140])
    need(live_pr(api.call(f"repos/binofet/{repo}/pulls/{number}"), repo, number) == context, "pr_changed_after_gate")
    return results


def write_summary(rows):
    lines = ["## Trusted family gates", "", "| PR | Head | Base | Native review | Source CI |", "| --- | --- | --- | --- | --- |"]
    for row in rows:
        context = row["context"]
        url = f"https://github.com/binofet/{context['repo']}/pull/{context['pr']}"
        review = row["reviewed-head"]
        review_status = review["state"]
        if review.get("evidence"):
            review_status = f"[{review_status}]({url}#pullrequestreview-{review['evidence']['id']})"
        ci = row["source-quality"]
        ci_status = ci["state"]
        if ci.get("evidence"):
            ci_status = f"[{ci_status}](https://github.com/binofet/{context['repo']}/actions/runs/{ci['evidence']['run_id']})"
        lines.append(f"| [{context['repo']}#{context['pr']}]({url}) | `{context['head']}` | `{context['base']}` | "
                     f"{review_status} | {ci_status} |")
        evidence = row["source-quality"].get("evidence", {})
        if evidence:
            run = evidence["run_id"]
            lines += ["", f"Quality run: [{run}](https://github.com/binofet/{context['repo']}/actions/runs/{run}); "
                      f"tested merge `{evidence['candidate']}`."]
            for artifact in evidence["artifacts"]:
                lines.append(f"- [Artifact {artifact['artifact_id']}](https://github.com/binofet/{context['repo']}/actions/runs/{run}/artifacts/{artifact['artifact_id']}) "
                             f"— {artifact['identity'][0]} C++{artifact['identity'][1] or 'n/a'}; "
                             f"{artifact['primary_tests']} primary cases; `{artifact['provider_digest']}`")
        for name in ("reviewed-head", "source-quality"):
            if row[name].get("diagnostic"):
                detail = row[name]["diagnostic"].replace("\n", " ").replace("|", "\\|")[:500]
                lines += ["", f"{name}: {detail}"]
    lines += ["", "No merge or release was performed by this controller."]
    text = "\n".join(lines) + "\n"
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with Path(os.environ["GITHUB_STEP_SUMMARY"]).open("a") as stream:
            stream.write(text)
    return text


def refresh_numbers(api, repo, event):
    """Untrusted event numbers route reads only; each PR is authenticated again by gate()."""
    base = f"repos/binofet/{repo}"
    if "pull_request" in event:
        return [event["pull_request"]["number"]]
    run = event.get("workflow_run")
    if run and run.get("name") != "Family review signal":
        head = run.get("head_sha", "")
        need(OID.fullmatch(head), "workflow_event_head_missing")
        rows = run.get("pull_requests") or api.pages(base + "/commits/" + head + "/pulls", maximum=100)
        numbers = []
        for item in rows:
            live = api.call(base + f"/pulls/{item['number']}")
            if live.get("state") == "open" and live.get("head", {}).get("sha") == head:
                live_pr(live, repo, item["number"])
                numbers.append(item["number"])
        return sorted(set(numbers))
    _, config = policy(repo)
    rows = api.pages(base + "/pulls?state=open&base=" + config["branch"], maximum=100)
    return [r["number"] for r in rows if r.get("head", {}).get("repo", {}).get("id") == config["id"]]


def generate_templates(repo, output):
    """Render only this fixed repository's triggers and policy into a new directory."""
    repo, config = policy(repo)
    need(not output.exists(), "new_template_output_required")
    substitutions = {"__REPOSITORY__": "binofet/" + repo,
                     "__DEFAULT_BRANCH__": config["branch"],
                     "__QUALITY_WORKFLOW__": config["workflow_name"]}
    rendered = {}
    for source in (Path(__file__).parent / "templates").glob("*.yml"):
        text = source.read_text()
        for marker, value in substitutions.items():
            text = text.replace(marker, value)
        need(not re.search(r"__[A-Z_]+__", text), "unresolved_workflow_template")
        rendered[source.name] = text
    need(set(rendered) == {"family-gate.yml", "family-review-signal.yml"}, "workflow_template_inventory_differs")
    output.mkdir(parents=True, exist_ok=False)
    for name, text in rendered.items():
        (output / name).write_text(text)
    return {"repository": "binofet/" + repo, "default_branch": config["branch"],
            "quality_workflow": config["workflow"], "output": str(output), "installed": False}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("review-body", "gate", "refresh", "templates"))
    parser.add_argument("--repo", required=True)
    parser.add_argument("--pr", type=int)
    parser.add_argument("--head")
    parser.add_argument("--base")
    parser.add_argument("--decision", choices=("approved", "rejected"), default="approved")
    parser.add_argument("--text-file", type=Path)
    parser.add_argument("--publish", action="store_true")
    parser.add_argument("--install-manifest", type=Path)
    parser.add_argument("--install-sha256")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    try:
        repo, _ = policy(args.repo)
        if args.mode == "templates":
            need(args.output and not args.publish, "new_template_output_required")
            print(json.dumps(generate_templates(repo, args.output)))
            return 0
        if args.mode == "review-body":
            need(args.text_file and args.pr and not args.publish, "review_body_arguments_required")
            raw = args.text_file.read_bytes()
            need(len(raw) <= 30000, "review_text_too_large")
            sys.stdout.write(review_body(repo, args.pr, args.head or "", args.base or "", raw.decode(), args.decision))
            return 0
        api = API()
        authorize = lambda: execution(api, repo, args.install_manifest, args.install_sha256)
        if args.publish:
            authorize()
        if args.mode == "gate":
            need(args.pr and args.pr > 0, "pr_required")
            numbers = [args.pr]
        else:
            path = Path(os.environ.get("GITHUB_EVENT_PATH", ""))
            need(path.is_file() and path.stat().st_size <= 1024 * 1024, "bounded_native_event_required")
            numbers = refresh_numbers(api, repo, decode(path.read_bytes()))
        rows = [gate(api, repo, number, args.publish, authorize) for number in numbers]
        write_summary(rows)
        print(json.dumps(rows, sort_keys=True))
        return 0 if all(r[name]["state"] == "success" for r in rows for name in ("reviewed-head", "source-quality")) else 1
    except (ValueError, OSError, KeyError, TypeError, ET.ParseError, zipfile.BadZipFile) as error:
        print(json.dumps({"status": "refused", "diagnostic": str(error)[:2000], "merge_performed": False}))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
