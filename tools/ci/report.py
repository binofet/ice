#!/usr/bin/env python3
"""Record exact tested source and publish bounded, measured ICE CI evidence."""

import argparse
from collections import Counter
import hashlib
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

from render_checks import cases as render_cases
from run_stage import write_json

NATIVE_STAGES = ("evidence", "path-policy", "dependencies", "quality-tools", "consumer-cache",
                 "metagen-configure", "metagen-build", "metagen-discovery", "metagen",
                 "rust", "configure", "build", "cpu-discovery", "cpu", "format", "api", "lint")
SANITIZER_STAGES = ("evidence", "dependencies", "configure", "build", "cpu-discovery", "cpu")
GLES_STAGES = ("gles-configure", "gles-build", "gles-discovery", "gles")


def git(*args):
    return subprocess.check_output(["git", *args], text=True).strip()


def plan(profile, runner):
    stages = NATIVE_STAGES if profile == "native" else SANITIZER_STAGES
    if profile == "native" and runner == "Linux":
        stages += tuple(name for name, _ in render_cases()) + GLES_STAGES
    suites = [("cpu", "cpu-discovery.log", "cpu.xml")]
    if profile == "native":
        suites.append(("metagen", "metagen-discovery.log", "metagen.xml"))
        if runner == "Linux":
            suites.append(("gles", "gles-discovery.log", "gles.xml"))
    return stages, suites


def identify(output, profile):
    event_path = os.environ.get("GITHUB_EVENT_PATH")
    event = json.loads(Path(event_path).read_text(encoding="utf-8")) if event_path else {}
    pull = event.get("pull_request")
    tested = git("rev-parse", "HEAD")
    parents = git("show", "-s", "--format=%P", "HEAD").split()
    before = event.get("before")
    if before == "0" * 40:
        before = None
    source = pull["head"]["sha"] if pull else tested
    base = parents[0] if pull and parents else before
    candidate = parents[1] if pull and len(parents) == 2 else tested
    environment_commit = os.environ.get("GITHUB_SHA", tested)
    links = {}
    for line in git("ls-tree", "-r", "HEAD").splitlines():
        metadata, name = line.split("\t", 1)
        mode, _, digest = metadata.split()
        if mode == "160000":
            links[name] = digest
    dirty = git("status", "--porcelain=v1", "--untracked-files=no")
    issues = []
    if tested != environment_commit:
        issues.append("checkout differs from the workflow event commit")
    if pull and (len(parents) != 2 or base != pull["base"]["sha"] or candidate != source):
        issues.append("PR merge parents differ from the event's exact base/head")
    if dirty:
        issues.append("tracked source differs from the tested commit")
    run_url = None
    if os.environ.get("GITHUB_RUN_ID"):
        run_url = (f"{os.environ['GITHUB_SERVER_URL']}/{os.environ['GITHUB_REPOSITORY']}"
                   f"/actions/runs/{os.environ['GITHUB_RUN_ID']}")
    record = {"event": os.environ.get("GITHUB_EVENT_NAME", "local"), "tested_commit": tested,
              "source_head": source, "tested_base": base, "tested_candidate_head": candidate,
              "event_base": pull["base"]["sha"] if pull else before,
              "base_role": "PR merge first parent" if pull else "push previous head (not a merge base)",
              "tree": git("rev-parse", "HEAD^{tree}"), "submodule_pins": links,
              "runner_os": os.environ.get("RUNNER_OS", sys.platform),
              "cpp": os.environ.get("CI_CPP", "unspecified"), "profile": profile,
              "run_url": run_url, "run_attempt": os.environ.get("GITHUB_RUN_ATTEMPT"),
              "source_validated": not issues, "source_issues": issues}
    output.mkdir(parents=True, exist_ok=True)
    write_json(output / "source.json", record)
    if issues:
        raise ValueError("; ".join(issues))


def test_cases(root):
    result = []
    for case in root.iter("testcase"):
        seconds = float(case.get("time", "nan"))
        if not math.isfinite(seconds) or seconds < 0:
            raise ValueError("JUnit case has no valid measured duration")
        status = ("failed" if case.find("failure") is not None else
                  "error" if case.find("error") is not None else
                  "skipped" if case.find("skipped") is not None else "passed")
        name = case.get("name")
        if not name:
            raise ValueError("JUnit case has no name")
        result.append({"name": name, "status": status, "duration_seconds": seconds})
    return result


def counts(cases):
    return {"tests": len(cases),
            **{status: sum(case["status"] == status for case in cases)
               for status in ("passed", "failed", "error", "skipped")}}


def cache_provenance(build):
    path = build / "CMakeCache.txt"
    if not path.is_file():
        return None
    selected = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = re.match(r"([^:#]+):[^=]+=(.*)$", line)
        if not match:
            continue
        key, value = match.groups()
        if (key.startswith(("ICE_BUILD_", "FETCHCONTENT_SOURCE_DIR_")) or key in {
                "ICE_CXX_STANDARD", "ICE_WARNINGS_AS_ERRORS", "ICE_ENABLE_SANITIZERS",
                "CMAKE_BUILD_TYPE", "CMAKE_CXX_COMPILER", "CMAKE_C_COMPILER",
                "CMAKE_CXX_FLAGS", "CMAKE_CXX_FLAGS_RELWITHDEBINFO", "CMAKE_CXX_FLAGS_DEBUG",
                "CMAKE_OSX_SYSROOT", "CMAKE_GENERATOR", "CMAKE_MAKE_PROGRAM"}):
            selected[key] = value
    overrides = {key: value for key, value in selected.items()
                 if key.startswith("FETCHCONTENT_SOURCE_DIR_") and value}
    return {"build": str(build), "cache_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "configuration": selected, "source_overrides": overrides}


def append_summary(text):
    print(text)
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with Path(os.environ["GITHUB_STEP_SUMMARY"]).open("a", encoding="utf-8") as target:
            target.write(text)


def md(value):
    return str(value).replace("|", "\\|").replace("\n", " ").replace("\r", " ")[:300]


def xml_text(value):
    return "".join(character if character in "\t\n\r" or ord(character) >= 32 else "\ufffd"
                   for character in value)


def driver_evidence(output):
    result = {}
    for name in ("gpu-environment", "gles"):
        path = output / f"{name}.log"
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines() if path.exists() else []
        result[name] = [line[:500] for line in lines if re.search(
            r"OpenGL .*string:|Actual GLES driver:|direct rendering:|Accelerated:", line)][:32]
    return result


def final_source_issues(source):
    issues = []
    if git("rev-parse", "HEAD") != source.get("tested_commit"):
        issues.append("Checked-out HEAD changed during the job.")
    if git("status", "--porcelain=v1", "--untracked-files=no"):
        issues.append("Tracked source changed during the job; tested bytes are not the declared commit.")
    return issues


def summarize(output):
    issues, suites = [], {}
    source_path = output / "source.json"
    if not source_path.is_file():
        output.mkdir(parents=True, exist_ok=True)
        source = {"runner_os": os.environ.get("RUNNER_OS", "unknown"),
                  "cpp": os.environ.get("CI_CPP", "unknown"),
                  "profile": os.environ.get("CI_PROFILE", "native")}
        issues.append("Source identification did not complete; no exact-source claim is available.")
    else:
        source = json.loads(source_path.read_text(encoding="utf-8"))
        if not source.get("source_validated"):
            issues.append("Source validation failed: " + "; ".join(source.get("source_issues", [])))
        issues += final_source_issues(source)
    expected, suite_plan = plan(source["profile"], source["runner_os"])
    stages = {}
    lines = [f"## ICE · {md(source['runner_os'])} · C++{md(source['cpp'])} · {source['profile']}", "",
             f"Tested commit: `{source.get('tested_commit', 'unavailable')}`",
             f"Source head: `{source.get('source_head', 'unavailable')}`",
             f"Base ({source.get('base_role', 'unavailable')}): `{source.get('tested_base')}`", "",
             "| Stage | Result | Exit | Seconds |", "| --- | --- | ---: | ---: |"]
    for name in expected:
        path = output / f"{name}.json"
        stage = None
        if path.is_file():
            try:
                stage = json.loads(path.read_text(encoding="utf-8"))
                seconds = stage["duration_seconds"]
                if (not isinstance(stage["exit_code"], int) or not math.isfinite(seconds)
                        or seconds < 0):
                    raise ValueError("invalid measured stage result")
            except (ValueError, KeyError, TypeError) as error:
                issues.append(f"Stage {name}: invalid evidence: {error}")
                stage = None
        if stage:
            stages[name] = stage
        code = stage["exit_code"] if stage else None
        status = "not run" if stage is None else (
            "passed" if code == 0 else "skipped (exit 77)" if code == 77 else "failed")
        if code != 0:
            issues.append(f"Required stage {name}: {status}")
        lines.append(f"| {name} | {status} | {code if stage else '—'} | "
                     f"{stage['duration_seconds'] if stage else '—'} |")
    for name, discovery_name, junit_name in suite_plan:
        try:
            discovery = json.loads((output / discovery_name).read_text(encoding="utf-8"))
            inventory = [test["name"] for test in discovery["tests"]]
            if not inventory:
                raise ValueError("CTest inventory is empty")
            actual = test_cases(ET.parse(output / junit_name).getroot())
            mismatch = Counter(inventory) != Counter(case["name"] for case in actual)
            total = counts(actual)
            suites[name] = {"counts": total, "discovered": len(inventory), "cases": actual,
                            "inventory_matches": not mismatch}
            if mismatch:
                issues.append(f"{name}: JUnit names/counts do not match the CTest inventory")
            if total["failed"] or total["error"] or total["skipped"]:
                issues.append(f"{name}: required tests failed, errored or skipped")
            lines += ["", f"### {name} — {total['passed']} passed, {total['failed']} failed, "
                      f"{total['error']} errors, {total['skipped']} skipped; "
                      f"{total['tests']} reported / {len(inventory)} discovered", "",
                      "| CTest case | Result | Measured seconds |", "| --- | --- | ---: |"]
            for case in actual[:200]:
                lines.append(f"| {md(case['name'])} | {case['status']} | "
                             f"{case['duration_seconds']:.6f} |")
            if len(actual) > 200:
                lines.append("Full per-case measurements are in summary.json and JUnit.")
        except (OSError, ValueError, TypeError, KeyError, ET.ParseError) as error:
            issues.append(f"{name}: evidence unavailable or invalid: {error}")
            lines += ["", f"{name}: test counts unavailable; evidence missing or invalid."]
    if source["profile"] == "native" and source["runner_os"] == "Linux":
        drivers = driver_evidence(output)
        write_json(output / "drivers.json", drivers)
        for name in ("gpu-environment", "gles"):
            if stages.get(name, {}).get("exit_code") == 0 and not drivers[name]:
                issues.append(f"{name}: successful stage omitted its actual driver description")
        suite = ET.Element("testsuite", name="ICE explicit X11 and Mesa OpenGL")
        for name, _ in render_cases():
            if name == "gpu-environment":
                continue
            stage = stages.get(name)
            case = ET.SubElement(suite, "testcase", name=name, classname="ICE.Native",
                                 time=str(stage["duration_seconds"] if stage else 0))
            if stage is None or stage["exit_code"] == 77:
                ET.SubElement(case, "skipped", message="not run" if stage is None else "exit 77")
            elif stage["exit_code"]:
                log = output / f"{name}.log"
                ET.SubElement(case, "failure", message=f"exit {stage['exit_code']}").text = xml_text(
                    log.read_text(encoding="utf-8", errors="replace")[-16384:]
                    if log.exists() else "Stage log is unavailable.")
        gpu_cases = test_cases(suite)
        totals = counts(gpu_cases)
        suite.set("tests", str(totals["tests"]))
        suite.set("failures", str(totals["failed"]))
        suite.set("errors", str(totals["error"]))
        suite.set("skipped", str(totals["skipped"]))
        ET.ElementTree(suite).write(output / "native.xml", encoding="utf-8", xml_declaration=True)
        suites["native"] = {"counts": totals, "cases": gpu_cases}
        lines += ["", "### Actual driver observations", ""]
        for name, descriptions in drivers.items():
            lines += [f"- {name}: {md(description)}" for description in descriptions]
        lines += ["", "Linux native tests execute X11 plus Mesa software OpenGL, including HDR, "
                  "textures, skinning and renderer recreation. GLES is a separate EGL configuration "
                  "and CTest suite; its log prints the actual GL driver. Environment logs are "
                  "evidence, not additional GPU test cases. No Metal parity or physical GPU claim."]
    elif source["profile"] == "native":
        lines += ["", "macOS compiles native Metal/OpenGL but GPU/window execution is not configured."]
    else:
        lines += ["", "Sanitizers execute headless ownership/data/physics/script/audio regressions "
                  "only; no GPU, desktop window or optional RTTR runtime is tested in this job."]
    caches = [cache_provenance(Path("build") / directory) for directory in
              ("ci", "metagen-ci", "gles-ci", "sanitizers")]
    caches = [cache for cache in caches if cache]
    for cache in caches:
        if cache["source_overrides"]:
            issues.append("Unattested dependency source overrides: " + str(cache["source_overrides"]))
    write_json(output / "configuration.json", caches)
    lines += ["", "Private Metagen source is not fetched: cached consumer hash verification and "
              "the public optional RTTR/Rust consumers are tested separately. Exact pins remain "
              "in the tested source; configuration.json records selected compiler/options and overrides.",
              "Windows runtime, mobile/Quest hardware, live providers, deployment and release are "
              "not configured. No self-hosted runner is assumed.",
              "Logs retain at most their final 8 MiB; provider console output is capped at 1 MiB "
              "plus its final 4 KiB. Truncation is explicit. JUnit and all per-case times are artifacts."]
    if issues:
        lines += ["", "### Incomplete or failed evidence", "", *["- " + md(issue) for issue in issues]]
    summary = {"source": source, "stages": stages, "suites": suites,
               "report_incomplete": bool(issues), "issues": issues}
    write_json(output / "summary.json", summary)
    text = "\n".join(lines) + "\n"
    (output / "summary.md").write_text(text, encoding="utf-8")
    append_summary(text)
    return int(bool(issues))


def publication(output, artifact_id, artifact_url, outcome):
    summary = json.loads((output / "summary.json").read_text(encoding="utf-8"))
    source = json.loads((output / "source.json").read_text(encoding="utf-8"))
    if summary["source"] != source:
        raise ValueError("summary and uploaded source provenance differ")
    if outcome != "success" or not artifact_id.isdecimal() or int(artifact_id) <= 0:
        raise ValueError("provider did not confirm a successful artifact upload with an artifact ID")
    expected = source.get("run_url", "")
    if not expected or artifact_url != expected + "/artifacts/" + artifact_id:
        raise ValueError("artifact URL does not identify this exact provider run")
    append_summary(f"\nEvidence upload confirmed: [artifact {artifact_id}]({artifact_url}).\n")
    return int(summary["report_incomplete"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    init = sub.add_parser("init")
    init.add_argument("--profile", choices=("native", "sanitizers"), required=True)
    sub.add_parser("summary")
    published = sub.add_parser("publication")
    published.add_argument("--artifact-id", required=True)
    published.add_argument("--artifact-url", required=True)
    published.add_argument("--outcome", required=True)
    args = parser.parse_args()
    output = Path(os.environ.get("CI_EVIDENCE_DIR", "build/ci-evidence"))
    try:
        if args.action == "init":
            identify(output, args.profile)
            return 0
        if args.action == "summary":
            return summarize(output)
        return publication(output, args.artifact_id, args.artifact_url, args.outcome)
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"CI evidence: {error}\n")


if __name__ == "__main__":
    sys.exit(main())
