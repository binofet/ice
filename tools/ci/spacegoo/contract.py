"""Offline contracts for isolated CPU jobs and independently collected merge evidence.

This module has no provider, registration, merge, deployment or shell capability.
Only an installed, trusted controller may supply policy and admission records.
"""

from pathlib import Path
from collections import Counter
import hashlib
import json
import math
import re
import stat


REPOS = {"ice", "metagen", "ignition"}
POLICY = Path("/etc/ice-family-ci/policy.json")
INSTALL = Path("/usr/local/libexec/ice-family-ci")
SHA = re.compile(r"[0-9a-f]{40}\Z")
DIGEST = re.compile(r"[0-9a-f]{64}\Z")
JOB = re.compile(r"[0-9a-f]{32}\Z")
USER = re.compile(r"[a-z][a-z0-9_-]{0,30}\Z")


def need(condition, message):
    if not condition:
        raise ValueError(message)


def measured_time(value):
    return type(value) in (int, float) and 0 <= value <= 86400 and math.isfinite(value)


def bounded_json(path, limit=1024 * 1024):
    with Path(path).open("rb") as stream:
        data = stream.read(limit + 1)
    need(len(data) <= limit, "JSON input exceeds its bound")

    def unique(pairs):
        result = {}
        for name, value in pairs:
            need(name not in result, "Duplicate JSON property")
            result[name] = value
        return result

    return json.loads(data, object_pairs_hook=unique,
                      parse_constant=lambda _: (_ for _ in ()).throw(ValueError("Nonfinite JSON")))


def trusted(path, owner=0, directory=False):
    """Reject symlink, wrong-owner and group/other-writable trusted path components."""
    path = Path(path)
    need(path.is_absolute(), "Trusted paths must be absolute")
    for part in [*reversed(path.parents), path]:
        info = part.lstat()
        need(not stat.S_ISLNK(info.st_mode) and info.st_uid == owner
             and not info.st_mode & 0o022, "Untrusted path: " + str(part))
    info = path.stat()
    need(stat.S_ISDIR(info.st_mode) if directory else stat.S_ISREG(info.st_mode),
         "Wrong trusted path type")
    return path


def policy(value):
    need(isinstance(value, dict) and value.get("format") == "ice-family-cpu-policy-v1",
         "Unsupported CPU policy")
    need(type(value.get("enabled")) is bool, "Explicit enabled state required")
    root = Path(value.get("root", ""))
    need(root.is_absolute() and root.parent == Path("/var/lib")
         and root.name not in {"cosmotomata-ci", "ice", "ignition", "metagen"},
         "A new dedicated /var/lib namespace is required")
    need(isinstance(value.get("repos"), dict) and set(value["repos"]) == REPOS,
         "Three explicit repository policies required")
    identities = set()
    for name, entry in value["repos"].items():
        need(set(entry) == {"repository", "control_user", "job_user", "runner_label", "required_checks"},
             "Unexpected repository policy fields")
        need(entry["repository"] == "binofet/" + name, "Repository policy differs")
        for field in ("control_user", "job_user"):
            user = entry[field]
            need(isinstance(user, str) and USER.fullmatch(user) and not user.startswith("cosmotomata")
                 and user not in {"root", "jmax"} and user not in identities,
                 "Distinct new non-owner control/job identities required")
            identities.add(user)
        need(entry["runner_label"] == name + ".cpu.spacegoo", "Dedicated runner label required")
        checks = entry["required_checks"]
        need(isinstance(checks, list) and checks and len(checks) == len(set(checks))
             and all(isinstance(c, str) and 0 < len(c) <= 200 for c in checks),
             "Explicit unique required checks required")
    tools = value.get("tools")
    need(isinstance(tools, dict) and set(tools) == {
        "python", "git", "cmake", "ctest", "cc", "cxx", "dotnet", "rustc"},
        "Explicit tool paths required")
    for path in tools.values():
        need(isinstance(path, str) and Path(path).is_absolute()
             and (path.startswith(str(root / "tools") + "/") or path.startswith("/usr/")),
             "Tools must be system or dedicated staged tools")
    limits = value.get("limits")
    need(isinstance(limits, dict) and set(limits) == {
        "cpu_percent", "memory_mib", "tasks", "runtime_seconds", "stage_seconds"},
        "Explicit resource profile required")
    for key, low, high in [("cpu_percent", 1, 200), ("memory_mib", 256, 8192),
                           ("tasks", 16, 256), ("runtime_seconds", 60, 3600),
                           ("stage_seconds", 10, 1800)]:
        need(type(limits[key]) is int and low <= limits[key] <= high,
             "Resource profile exceeds the initial commissioning envelope")
    return value


def admission(value, config, repo, job_id, policy_digest):
    policy(config)
    need(repo in REPOS and JOB.fullmatch(job_id), "Invalid repository/job ID")
    need(isinstance(value, dict) and set(value) == {
        "format", "repo", "id", "event", "head", "base", "candidate", "dependencies",
        "policy_sha256", "inventory_sha256", "review_receipt_sha256", "provider_run_id"},
        "Exact sealed job admission required")
    need(value["format"] == "ice-family-cpu-job-v1" and value["repo"] == repo
         and value["id"] == job_id, "Admission identity differs")
    need(value["event"] in {"push", "pull_request"}, "Unsupported source event")
    need(all(isinstance(value[k], str) and SHA.fullmatch(value[k])
             for k in ("head", "base", "candidate")), "Exact Git commits required")
    if value["event"] == "push":
        need(value["candidate"] == value["head"], "Push candidate differs from head")
    need(isinstance(value["dependencies"], dict)
         and set(value["dependencies"]) == ({"ice"} if repo == "ignition" else set()),
         "Dependency policy differs")
    need(all(isinstance(v, str) and SHA.fullmatch(v) for v in value["dependencies"].values()),
         "Exact dependency commits required")
    need(value["policy_sha256"] == policy_digest and DIGEST.fullmatch(policy_digest),
         "Sealed policy digest differs")
    need(all(isinstance(value[k], str) and DIGEST.fullmatch(value[k]) and value[k] != "0" * 64
             for k in ("inventory_sha256", "review_receipt_sha256")),
         "Independent inventory/review receipt digests required")
    need(isinstance(value["provider_run_id"], str)
         and re.fullmatch(r"[1-9][0-9]{0,19}", value["provider_run_id"]), "Provider run ID required")
    return value


def unit_command(config, repo, job_id):
    """Build fixed broker argv; never accept executable, shell, env or unit-property inputs."""
    policy(config)
    need(repo in REPOS and JOB.fullmatch(job_id), "Invalid repository/job ID")
    root = Path(config["root"])
    limits = config["limits"]
    entry = config["repos"][repo]
    # Each job receives only its own root-owned intake, and its own writable job root.
    properties = [f"CPUQuota={limits['cpu_percent']}%", f"MemoryMax={limits['memory_mib']}M",
                  f"TasksMax={limits['tasks']}", f"RuntimeMaxSec={limits['runtime_seconds']}",
                  "TimeoutStopSec=10", "KillMode=control-group", "SendSIGKILL=yes", "Nice=10",
                  "NoNewPrivileges=yes", "PrivateDevices=yes", "PrivateNetwork=yes", "PrivateTmp=yes",
                  "ProtectHome=yes", "ProtectSystem=strict", "ProtectProc=invisible",
                  "RestrictSUIDSGID=yes", "CapabilityBoundingSet=", "AmbientCapabilities=", "UMask=0077",
                  "ReadOnlyPaths=" + str(root / "intake" / repo) + " " + str(root / "tools")
                  + " " + str(root / "dependencies"),
                  "ReadWritePaths=" + str(root / "jobs" / repo),
                  "InaccessiblePaths=" + " ".join(
                      [str(root / "control"), str(root / "receipts"), str(root / "locks"),
                       "/var/lib/cosmotomata-ci", "/opt", "/srv", "/mnt", "/media"]
                      + [str(root / "intake" / other) for other in sorted(REPOS - {repo})]
                      + [str(root / "jobs" / other) for other in sorted(REPOS - {repo})])]
    command = ["/usr/bin/systemd-run", "--system", "--wait", "--collect", "--pipe",
               "--service-type=exec", "--unit=ice-family-ci-" + repo + "-" + job_id,
               "--uid=" + entry["job_user"], "--gid=" + entry["job_user"], "--working-directory=/"]
    command += ["--property=" + p for p in properties]
    return command + ["/usr/bin/env", "-i", "PATH=/usr/bin:/bin", "LANG=C.UTF-8",
                      "/usr/bin/python3", "-I", str(INSTALL / "job.py"), repo, job_id]


def verify_merge(config, snapshot):
    """Validate a TRUSTED collector snapshot, never an author-supplied claim of approval.

    The controller must authenticate provider data, independently collect report hashes,
    then recheck current head/base while serializing the actual merge. This is not a lock,
    a provider signature verifier or a merge operation.
    """
    policy(config)
    need(isinstance(snapshot, dict) and snapshot.get("format") == "ice-family-merge-snapshot-v1",
         "Unsupported merge snapshot")
    repo = snapshot.get("repo")
    need(repo in REPOS, "Unknown repository")
    for field in ("head", "base", "candidate", "current_head", "current_base"):
        need(isinstance(snapshot.get(field), str) and SHA.fullmatch(snapshot[field]),
             "Exact head/base/candidate required")
    need(snapshot["head"] == snapshot["current_head"] and snapshot["base"] == snapshot["current_base"],
         "Head or target advanced; review and CI must refresh")
    review = snapshot.get("review")
    need(isinstance(review, dict) and review.get("head") == snapshot["head"]
         and review.get("state") == "approved" and review.get("changes_requested") is False
         and isinstance(review.get("receipt_sha256"), str)
         and DIGEST.fullmatch(review["receipt_sha256"]) and review["receipt_sha256"] != "0" * 64,
         "Current exact-head approval required")
    rows = snapshot.get("checks")
    required = config["repos"][repo]["required_checks"]
    need(isinstance(rows, list) and len(rows) == len(required)
         and all(isinstance(row, dict) for row in rows)
         and {row.get("name") for row in rows} == set(required), "Required check inventory differs")
    for row in rows:
        need(row.get("head") == snapshot["head"] and row.get("base") == snapshot["base"]
             and row.get("candidate") == snapshot["candidate"]
             and row.get("status") == "completed" and row.get("conclusion") == "success"
             and isinstance(row.get("report_sha256"), str) and DIGEST.fullmatch(row["report_sha256"])
             and row["report_sha256"] != "0" * 64,
             "Missing, stale, unsuccessful or uncollected required check")
    return {"status": "admitted_for_exact_snapshot", "repo": repo,
            "head": snapshot["head"], "base": snapshot["base"], "candidate": snapshot["candidate"],
            "merge_performed": False}


def verify_report(config, request, report):
    """Validate independently collected job JSON; collection/digest trust stays outside jobs."""
    policy(config)
    need(isinstance(report, dict) and report.get("format") == "ice-family-cpu-report-v1"
         and report.get("status") == "passed" and report.get("admission") == request
         and report.get("repo") == request["repo"]
         and report.get("policy_sha256") == request["policy_sha256"], "Passing exact admitted report required")
    need(all(report.get(k) is False for k in ("gpu", "devices", "providers", "merge_performed")),
         "Unexpected CPU report authority")
    rows = report.get("stages")
    need(isinstance(rows, list) and rows and all(isinstance(r, dict) for r in rows),
         "Stage evidence required")
    names = [r.get("name") for r in rows]
    need(len(names) == len(set(names)), "Duplicate stage evidence")
    required = {"clone", "checkout", "source-identity"}
    required.add("merge-parents" if request["event"] == "pull_request" else "base-ancestry")
    if request["repo"] == "ignition":
        required.update({"ice-gitlink", "clone-ice", "checkout-ice"})
    if request["repo"] == "metagen":
        required.update({"dotnet-identity", "rustc-identity", "conformance", "formats-codecs", "rust", "python-cases"})
        profiles = {"conformance"}
    else:
        profiles = {"cpu20", "cpu23"}
        required.update(p + suffix for p in profiles for suffix in ("-configure", "-build", "-inventory", "-tests"))
    need(set(names) == required, "Required stage inventory differs")
    for row in rows:
        need(type(row.get("exit_code")) is int and row["exit_code"] == 0
             and row.get("timed_out") is False and isinstance(row.get("log_sha256"), str)
             and DIGEST.fullmatch(row["log_sha256"]) and measured_time(row.get("seconds")),
             "Failed, timed-out, unmeasured or uncollected stage")
    actual = report.get("profiles")
    need(isinstance(actual, list) and len(actual) == len(profiles)
         and all(isinstance(p, dict) for p in actual)
         and {p.get("name") for p in actual} == profiles, "Required profile inventory differs")
    count = 0
    for profile in actual:
        need(profile.get("status") == "passed" and isinstance(profile.get("cases"), list)
             and profile["cases"], "Passing nonempty profile required")
        identities = []
        for case in profile["cases"]:
            need(isinstance(case, dict) and case.get("failed") is False and case.get("skipped") is False,
                 "Required cases failed or skipped")
            need(all(isinstance(case.get(k), str) and case[k].strip() for k in ("name", "classname"))
                 and measured_time(case.get("time")), "Named measured cases required")
            identities.append((case["classname"], case["name"]))
            count += 1
        need(len(identities) == len(set(identities)), "Duplicate case identity")
        inventory = profile.get("inventory")
        need(isinstance(inventory, list) and all(isinstance(n, str) and n.strip() for n in inventory)
             and Counter(c["name"] for c in profile["cases"]) == Counter(inventory),
             "Exact discovered case inventory differs")
    return {"status": "verified_collected_cpu_report", "repo": request["repo"],
            "candidate": request["candidate"], "tests": count, "merge_performed": False}


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()
