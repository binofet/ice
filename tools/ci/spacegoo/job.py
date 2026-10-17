#!/usr/bin/python3 -I
"""Installed CPU job. Execute only as the dedicated repo job user inside its unit."""

from collections import deque
from collections import Counter
from pathlib import Path
import hashlib
import json
import os
import pwd
import selectors
import signal
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parent))
from contract import DIGEST, INSTALL, JOB, POLICY, admission, bounded_json, digest, measured_time, need, policy, trusted

LOG_BYTES = 1024 * 1024


class Runner:
    def __init__(self, output, environment, timeout):
        self.output, self.environment, self.timeout = output, environment, timeout
        self.stages = []

    def run(self, name, command, cwd=None):
        """Keep bounded tails; terminate only this stage's process group on every exit."""
        started = time.monotonic()
        tail, retained, total = deque(), 0, 0
        process = subprocess.Popen([str(x) for x in command], cwd=cwd, env=self.environment,
                                   stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, start_new_session=True)
        timeout = False
        poller = selectors.DefaultSelector()
        poller.register(process.stdout, selectors.EVENT_READ)
        try:
            while poller.get_map():
                remaining = self.timeout - (time.monotonic() - started)
                if remaining <= 0:
                    timeout = True
                    break
                for key, _ in poller.select(min(remaining, .25)):
                    block = os.read(key.fileobj.fileno(), 65536)
                    if not block:
                        poller.unregister(key.fileobj)
                        continue
                    tail.append(block)
                    retained += len(block)
                    total += len(block)
                    while retained > LOG_BYTES:
                        retained -= len(tail.popleft())
            result = process.wait(timeout=max(.01, self.timeout - (time.monotonic() - started)))
        except subprocess.TimeoutExpired:
            timeout = True
            result = 124
        finally:
            # A child retaining stdout cannot keep the stage alive indefinitely. Escaped
            # sessions remain in the systemd cgroup and are killed when that unit stops.
            for sig in (signal.SIGTERM, signal.SIGKILL):
                try:
                    os.killpg(process.pid, sig)
                except ProcessLookupError:
                    pass
            process.wait()
            poller.close()
            process.stdout.close()
        if timeout:
            result = 124
        log = self.output / (name + ".log")
        log.write_bytes((b"[truncated: final 1 MiB]\n" if total > LOG_BYTES else b"") + b"".join(tail))
        row = {"name": name, "command": [str(x) for x in command], "exit_code": result,
               "timed_out": timeout, "seconds": round(time.monotonic() - started, 6),
               "log": log.name, "log_sha256": digest(log), "output_bytes": total,
               "log_truncated": total > LOG_BYTES}
        self.stages.append(row)
        print(json.dumps(row), flush=True)
        need(result == 0, "Stage failed: " + name)
        return log.read_text(errors="replace")


def inventory(path, expected, root, required_tools):
    """Verify reviewed offline tools/dependencies; a job cannot bless replacement hashes."""
    need(digest(path) == expected, "Offline input inventory digest differs")
    rows = path.read_text().splitlines()
    need(0 < len(rows) <= 100000, "Invalid offline inventory size")
    seen = set()
    for line in rows:
        need(len(line) >= 67 and line[64:66] == "  ", "Invalid offline inventory row")
        value, name = line[:64], line[66:]
        need(DIGEST.fullmatch(value), "Invalid inventory SHA256")
        need(name not in seen and Path(name).is_absolute(), "Duplicate/relative inventory path")
        seen.add(name)
        need(name.startswith(str(root / "tools") + "/")
             or name.startswith(str(root / "dependencies") + "/") or name.startswith("/usr/"),
             "Inventory path outside staged inputs")
        need(digest(name) == value, "Offline input changed: " + name)
    need(set(required_tools) <= seen, "Offline inventory omits a selected tool")


def test_cases(path):
    cases = list(ET.parse(path).getroot().iter("testcase"))
    need(cases, "Zero tests are not a passing CPU lane")
    result = []
    for case in cases:
        row = dict(case.attrib)
        try:
            row["time"] = float(row.get("time", "nan"))
        except ValueError:
            raise ValueError("Invalid case timing") from None
        need(measured_time(row["time"]) and all(isinstance(row.get(k), str) and row[k].strip()
                                                for k in ("name", "classname")),
             "Named finite measured cases required")
        row.update(failed=case.find("failure") is not None or case.find("error") is not None,
                   skipped=case.find("skipped") is not None)
        need(not row["failed"] and not row["skipped"], "Required test failed or skipped")
        result.append(row)
    identities = [(row["classname"], row["name"]) for row in result]
    need(len(identities) == len(set(identities)), "Duplicate case identity")
    return result


def main(argv):
    report = {"format": "ice-family-cpu-report-v1", "status": "failed", "stages": [],
              "profiles": [], "gpu": False, "devices": False, "providers": False,
              "merge_performed": False}
    output = None
    started = time.monotonic()
    try:
        need(os.geteuid() != 0 and Path(__file__).resolve() == INSTALL / "job.py",
             "Only installed non-root CPU jobs may run")
        need(len(argv) == 2, "Expected repository/job ID")
        repo, job_id = argv
        need(JOB.fullmatch(job_id), "Invalid job ID")
        config = policy(bounded_json(trusted(POLICY)))
        need(config["enabled"] and repo in config["repos"], "Repository is not commissioned")
        need(pwd.getpwuid(os.geteuid()).pw_name == config["repos"][repo]["job_user"],
             "Wrong source job identity")
        root = Path(config["root"])
        intake = trusted(root / "intake" / repo, directory=True)
        request = admission(bounded_json(trusted(intake / "admissions" / (job_id + ".json")), 65536),
                            config, repo, job_id, digest(POLICY))
        jobs = root / "jobs" / repo
        info = jobs.lstat()
        need(jobs.is_dir() and not jobs.is_symlink() and info.st_uid == os.geteuid()
             and info.st_mode & 0o777 == 0o700, "Private repo job root required")
        work = jobs / job_id
        work.mkdir(mode=0o700)  # Single use; a retry needs a newly sealed admission.
        output = work / "results"
        output.mkdir(mode=0o700)
        report.update(repo=repo, admission=request, policy_sha256=digest(POLICY))
        selected = {"python", "git", "dotnet", "rustc"} if repo == "metagen" else {
            "python", "git", "cmake", "ctest", "cc", "cxx"}
        inventory(trusted(intake / "files.sha256"), request["inventory_sha256"], root,
                  [config["tools"][name] for name in selected])
        home = work / "home"
        home.mkdir(mode=0o700)
        tools = config["tools"]
        # No inherited auth, SSH agent, provider URL, user config or developer HOME.
        environment = {"PATH": str(root / "tools/bin") + ":/usr/bin:/bin", "LANG": "C.UTF-8",
                       "HOME": str(home), "TMPDIR": str(work), "CC": tools["cc"],
                       "CXX": tools["cxx"], "GIT_CONFIG_NOSYSTEM": "1", "GIT_CONFIG_GLOBAL": "/dev/null",
                       "GIT_TERMINAL_PROMPT": "0", "GIT_SSH_COMMAND": "/bin/false",
                       "DOTNET_CLI_HOME": str(home), "DOTNET_SKIP_FIRST_TIME_EXPERIENCE": "1",
                       "DOTNET_CLI_TELEMETRY_OPTOUT": "1", "PYTHONDONTWRITEBYTECODE": "1"}
        runner = Runner(output, environment, config["limits"]["stage_seconds"])
        report["stages"] = runner.stages
        source = work / "source"
        runner.run("clone", [tools["git"], "-c", "core.hooksPath=/dev/null", "clone",
                             "--no-hardlinks", "--no-checkout", str(intake / "source.git"), source])
        runner.run("checkout", [tools["git"], "-C", source, "checkout", "--detach", request["candidate"]])
        actual = runner.run("source-identity", [tools["git"], "-C", source, "rev-parse", "HEAD"]).strip()
        need(actual == request["candidate"], "Source checkout differs")
        if request["event"] == "pull_request":
            parents = runner.run("merge-parents", [tools["git"], "-C", source, "show", "-s",
                                                   "--format=%P", "HEAD"]).strip().split()
            need(parents == [request["base"], request["head"]], "Exact merge parents differ")
        else:
            runner.run("base-ancestry", [tools["git"], "-C", source, "merge-base", "--is-ancestor",
                                         request["base"], request["candidate"]])
        ice = source
        if repo == "ignition":
            pinned = runner.run("ice-gitlink", [tools["git"], "-C", source, "rev-parse", "HEAD:deps/ice"]).strip()
            need(pinned == request["dependencies"]["ice"], "ICE gitlink differs from admission")
            ice = source / "deps/ice"
            runner.run("clone-ice", [tools["git"], "clone", "--no-hardlinks", "--no-checkout",
                                     str(intake / "ice.git"), ice])
            runner.run("checkout-ice", [tools["git"], "-C", ice, "checkout", "--detach", pinned])
        if repo == "metagen":
            # Root stages PATH links for dotnet/rustc/clang-format to these reviewed tools.
            for name in ("dotnet", "rustc"):
                runner.run(name + "-identity", [tools[name], "--version"])
            runner.run("conformance", [tools["python"], source / "check.py"], source)
            runner.run("formats-codecs", [tools["python"], source / "data_lab.py", "--all-codecs"], source)
            runner.run("rust", [tools["python"], source / "check_rust.py", "--rustc", tools["rustc"]], source)
            xml = output / "python.xml"
            runner.run("python-cases", [tools["python"], INSTALL / "unittest_xml.py",
                                         source / "python", xml], source)
            cases = test_cases(xml)
            report["profiles"].append({"name": "conformance", "status": "passed", "cases": cases,
                                       "inventory": json.loads((output / "python.inventory.json").read_text())})
        else:
            for standard in (20, 23):
                name = "cpu" + str(standard)
                build = work / name
                profile = {"name": name, "status": "failed"}
                report["profiles"].append(profile)
                try:
                    command = [tools["cmake"], "-S", source, "-B", build,
                               "-DCMAKE_BUILD_TYPE=RelWithDebInfo", "-DICE_CXX_STANDARD=" + str(standard),
                               "-DICE_WARNINGS_AS_ERRORS=ON", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
                               "-DFETCHCONTENT_FULLY_DISCONNECTED=ON", "-DICE_BUILD_COMPANION=OFF",
                               "-DICE_BUILD_TOOLS=OFF", "-DPython3_EXECUTABLE=" + tools["python"],
                               "-DCMAKE_PREFIX_PATH=" + str(root / "tools/prefix"),
                               "-DICE_BOX2D_SOURCE_DIR=" + str(root / "dependencies/box2d"),
                               "-DICE_JOLT_SOURCE_DIR=" + str(root / "dependencies/jolt"),
                               "-DICE_LUA_SOURCE_DIR=" + str(root / "dependencies/lua")]
                    if repo == "ice":
                        command += ["-DBUILD_TESTING=ON", "-DICE_BUILD_DESKTOP=OFF",
                                    "-DICE_BUILD_OPENGL=OFF", "-DICE_BUILD_METAL=OFF", "-DICE_BUILD_GLES=OFF"]
                    else:
                        command += ["-DBUILD_TESTING=OFF", "-DIGNITION_BUILD_TESTS=ON",
                                    "-DIGNITION_ICE_SOURCE_DIR=" + str(ice)]
                    runner.run(name + "-configure", command)
                    runner.run(name + "-build", [tools["cmake"], "--build", build, "--parallel", "2"])
                    planned = json.loads(runner.run(name + "-inventory", [tools["ctest"], "--test-dir", build,
                                                                         "--show-only=json-v1"]))
                    xml = output / (name + ".xml")
                    runner.run(name + "-tests", [tools["ctest"], "--test-dir", build, "--output-on-failure",
                                                 "--no-tests=error", "--timeout", "120", "--output-junit", xml])
                    profile["cases"] = test_cases(xml)
                    profile["inventory"] = [t["name"] for t in planned["tests"]]
                    need(Counter(c["name"] for c in profile["cases"]) == Counter(profile["inventory"]),
                         "JUnit inventory differs")
                    profile["status"] = "passed"
                except ValueError as error:
                    profile["diagnostic"] = str(error)[:2000]
            need(all(p["status"] == "passed" for p in report["profiles"]), "A CPU profile failed")
        report["status"] = "passed"
    except (ValueError, OSError, KeyError, TypeError) as error:
        report["diagnostic"] = str(error)[:2000]
    finally:
        report["seconds"] = round(time.monotonic() - started, 6)
        if output is not None:
            for xml in output.glob("*.xml"):
                report.setdefault("junit", []).append({"file": xml.name, "sha256": digest(xml)})
            (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"status": report["status"], "diagnostic": report.get("diagnostic"),
                      "report": str(output / "report.json") if output else None}), flush=True)
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
