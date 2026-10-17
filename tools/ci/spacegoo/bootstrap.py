#!/usr/bin/python3 -IB
"""Disabled namespace bootstrap. No sudo delegation, services, dispatch or provider access."""

import sys
sys.dont_write_bytecode = True

import argparse
import fcntl
import grp
import hashlib
import json
import os
from pathlib import Path
import pwd
import signal
import stat
import subprocess
import tempfile


ROOT = Path("/var/lib/ice-family-ci")
INSTALL = Path("/usr/local/libexec/ice-family-ci")
POLICY = Path("/etc/ice-family-ci/policy.json")
REPOS = ("ice", "metagen", "ignition")
CODE = ("contract.py", "broker.py", "job.py", "unittest_xml.py", "admit.py")
USERS = tuple(name for repo in REPOS for name in (repo + "-ci-control", repo + "-cpu-job"))
PACKET_FILES = ("bootstrap.py", *("payload/" + name for name in CODE), "payload/policy.json")


def disabled_policy():
    """The only policy this phase can install; future commissioning is a separate change."""
    return {"format": "ice-family-cpu-policy-v1", "enabled": False, "root": str(ROOT),
            "limits": {"cpu_percent": 200, "memory_mib": 8192, "tasks": 256,
                       "runtime_seconds": 3600, "stage_seconds": 1800},
            "tools": {"python": str(ROOT / "tools/python/bin/python3"), "git": "/usr/bin/git",
                      "cmake": "/usr/bin/cmake", "ctest": "/usr/bin/ctest", "cc": "/usr/bin/cc",
                      "cxx": "/usr/bin/c++", "dotnet": str(ROOT / "tools/dotnet/dotnet"),
                      "rustc": str(ROOT / "tools/rust/bin/rustc")},
            "repos": {repo: {"repository": "binofet/" + repo, "control_user": repo + "-ci-control",
                             "job_user": repo + "-cpu-job", "runner_label": repo + ".cpu.spacegoo",
                             "required_checks": ["conformance"] if repo == "metagen"
                             else ["cpu20", "cpu23"]} for repo in REPOS}}


def need(condition, message):
    if not condition:
        raise ValueError(message)


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode()


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def json_object(raw):
    def unique(pairs):
        output = {}
        for name, value in pairs:
            need(name not in output, "Duplicate JSON property")
            output[name] = value
        return output
    return json.loads(raw, object_pairs_hook=unique,
                      parse_constant=lambda _: (_ for _ in ()).throw(ValueError("Nonfinite JSON")))


class Filesystem:
    """Fixed absolute paths in production; tests can map them into a disposable directory."""

    def __init__(self, base=Path("/"), root_uid=0, root_gid=0):
        self.base, self.root_uid, self.root_gid = Path(base), root_uid, root_gid

    def path(self, path):
        path = Path(path)
        need(path.is_absolute() and ".." not in path.parts, "Absolute normalized path required")
        return self.base / path.relative_to("/")

    def inspect(self, path):
        try:
            entry = self.path(path).lstat()
        except FileNotFoundError:
            return None
        return {"kind": "directory" if stat.S_ISDIR(entry.st_mode) else
                "file" if stat.S_ISREG(entry.st_mode) else "unsafe",
                "uid": entry.st_uid, "gid": entry.st_gid, "mode": stat.S_IMODE(entry.st_mode),
                "links": entry.st_nlink, "size": entry.st_size}

    def read(self, path, limit=1024 * 1024):
        fd = os.open(self.path(path), os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
        with os.fdopen(fd, "rb") as source:
            entry = os.fstat(source.fileno())
            need(stat.S_ISREG(entry.st_mode) and entry.st_nlink == 1 and entry.st_size <= limit,
                 "Unsafe or oversized file: " + str(path))
            raw = source.read(limit + 1)
            after = os.fstat(source.fileno())
            need(len(raw) == entry.st_size and (entry.st_ino, entry.st_size, entry.st_mtime_ns)
                 == (after.st_ino, after.st_size, after.st_mtime_ns), "File changed: " + str(path))
            return raw

    def ancestors(self, path, include_leaf=False, allowed_missing=()):
        path = Path(path)
        parts = [*reversed(path.parents), *([path] if include_leaf else [])]
        for part in parts:
            entry = self.inspect(part)
            if entry is not None:
                need(entry["kind"] == "directory" and entry["uid"] == self.root_uid
                     and not entry["mode"] & 0o022, "Unsafe ancestor: " + str(part))
            else:
                need(part in allowed_missing, "Missing unplanned ancestor: " + str(part))

    def children(self, path):
        return [Path(path) / child.name for child in self.path(path).iterdir()]

    def directory(self, path, uid, gid, mode):
        self.ancestors(path)
        parent = os.open(self.path(Path(path).parent), os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        try:
            os.mkdir(Path(path).name, 0o700, dir_fd=parent)
            child = os.open(Path(path).name, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=parent)
            try:
                os.fchown(child, uid, gid)
                os.fchmod(child, mode)
                os.fsync(child)
            finally:
                os.close(child)
            os.fsync(parent)
        finally:
            os.close(parent)

    def file(self, path, raw, uid, gid, mode):
        self.ancestors(path)
        parent = self.path(Path(path).parent)
        fd, temporary = tempfile.mkstemp(prefix=".disabled-bootstrap-", dir=parent)
        try:
            with os.fdopen(fd, "wb") as output:
                output.write(raw)
                output.flush()
                os.fchown(output.fileno(), uid, gid)
                os.fchmod(output.fileno(), mode)
                os.fsync(output.fileno())
            # Exclusive publication: never overwrite a file that appeared after preflight.
            os.link(temporary, self.path(path), follow_symlinks=False)
            os.unlink(temporary)
            directory = os.open(parent, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
            try:
                os.fsync(directory)
            finally:
                os.close(directory)
        finally:
            if os.path.exists(temporary):
                os.unlink(temporary)


class Accounts:
    def __init__(self):
        # These are the system-account ranges used by Ubuntu's useradd/groupadd.
        self.uid_max = self.gid_max = 999
        for line in Path("/etc/login.defs").read_text().splitlines():
            fields = line.split()
            if len(fields) >= 2 and fields[0] in ("SYS_UID_MAX", "SYS_GID_MAX"):
                setattr(self, "uid_max" if fields[0] == "SYS_UID_MAX" else "gid_max", int(fields[1]))

    def prerequisites(self, fs):
        result = []
        for path in ("/usr/sbin/groupadd", "/usr/sbin/useradd", "/usr/sbin/nologin"):
            fs.ancestors(path)
            row = fs.inspect(path)
            need(row and row["kind"] == "file" and row["uid"] == fs.root_uid
                 and not row["mode"] & 0o022 and row["mode"] & 0o111,
                 "Missing or unsafe system account prerequisite: " + path)
            result.append({"path": path, "sha256": digest(fs.read(path, 4 * 1024 * 1024)),
                           "mode": row["mode"]})
        return result

    def snapshot(self, name):
        try:
            group = grp.getgrnam(name)
            group = {"name": group.gr_name, "gid": group.gr_gid, "members": sorted(group.gr_mem),
                     "aliases": sorted(g.gr_name for g in grp.getgrall()
                                       if g.gr_gid == group.gr_gid and g.gr_name != name),
                     "primary_users": sorted(p.pw_name for p in pwd.getpwall()
                                             if p.pw_gid == group.gr_gid)}
        except KeyError:
            group = None
        try:
            user = pwd.getpwnam(name)
        except KeyError:
            return {"group": group, "user": None}
        locked = None
        if os.geteuid() == 0:
            # Never return, print or retain password fields; only the conservative lock boolean.
            for line in Path("/etc/shadow").read_text().splitlines():
                fields = line.split(":")
                if fields[0] == name:
                    locked = fields[1].startswith(("!", "*"))
                    break
        return {"group": group, "user": {"name": user.pw_name, "uid": user.pw_uid, "gid": user.pw_gid,
                "home": user.pw_dir, "shell": user.pw_shell, "locked": locked,
                "supplementary": sorted(g.gr_name for g in grp.getgrall()
                                        if name in g.gr_mem and g.gr_gid != user.pw_gid),
                "aliases": sorted(p.pw_name for p in pwd.getpwall()
                                  if p.pw_uid == user.pw_uid and p.pw_name != name),
                "shared_primary": sorted(p.pw_name for p in pwd.getpwall()
                                         if p.pw_gid == user.pw_gid and p.pw_name != name)}}

    def create(self, kind, name):
        if kind == "group":
            args = ["/usr/sbin/groupadd", "--system", name]
        else:
            args = ["/usr/sbin/useradd", "--system", "--no-create-home", "--gid", name,
                    "--home-dir", "/nonexistent", "--shell", "/usr/sbin/nologin",
                    "--no-log-init", "--key", "CREATE_MAIL_SPOOL=no", "--password", "!", name]
        result = subprocess.run(args, stdin=subprocess.DEVNULL, capture_output=True, timeout=30,
                                cwd="/", env={"PATH": "/usr/sbin:/usr/bin:/sbin:/bin", "LC_ALL": "C"})
        need(result.returncode == 0, "Account creation failed (exit " + str(result.returncode) + "): " + name)


def validate_account(name, row, accounts):
    group, user = row["group"], row["user"]
    if group:
        need(group["name"] == name and 0 < group["gid"] <= accounts.gid_max
             and not set(group["members"]) - {name} and not group["aliases"]
             and not set(group["primary_users"]) - {name},
             "Existing group differs: " + name)
    if user:
        need(group and user["name"] == name and 0 < user["uid"] <= accounts.uid_max
             and user["gid"] == group["gid"]
             and user["home"] == "/nonexistent" and user["shell"] == "/usr/sbin/nologin"
             and user["locked"] is True and not user["supplementary"] and not user["shared_primary"]
             and not user["aliases"],
             "Existing account differs or lock cannot be verified: " + name)


def load_bundle(directory, manifest_sha256):
    directory = Path(os.path.abspath(directory))
    need(len(manifest_sha256) == 64 and all(c in "0123456789abcdef" for c in manifest_sha256),
         "An independently selected manifest SHA256 is required")
    fs = Filesystem()
    # Do not resolve away a symlink in the packet path. Planning permits a user-owned packet,
    # while apply separately requires every ancestor to be root-owned and non-writable.
    for path in (*reversed(directory.parents), directory, directory / "payload"):
        row = fs.inspect(path)
        need(row and row["kind"] == "directory", "Unsafe bundle directory: " + str(path))
    raw = fs.read(directory / "manifest.json")
    need(digest(raw) == manifest_sha256, "Bundle manifest SHA256 differs")
    manifest = json_object(raw)
    need(isinstance(manifest, dict) and set(manifest) == {"format", "source_commit", "files"}
         and manifest["format"] == "ice-family-disabled-bootstrap-v1"
         and isinstance(manifest["source_commit"], str) and len(manifest["source_commit"]) == 40
         and all(c in "0123456789abcdef" for c in manifest["source_commit"])
         and isinstance(manifest["files"], dict)
         and set(manifest["files"]) == set(PACKET_FILES), "Unexpected bundle contract")
    allowed = {directory / name for name in (*PACKET_FILES, "manifest.json", "payload")}
    need(all(path in allowed for path in fs.children(directory))
         and all(path in allowed for path in fs.children(directory / "payload")),
         "Unexpected bundle entry")
    files = {}
    for name in PACKET_FILES:
        files[name] = fs.read(directory / name)
        need(digest(files[name]) == manifest["files"][name], "Bundle file differs: " + name)
    need(files["bootstrap.py"] == Path(__file__).read_bytes(), "Running installer differs from bundle")
    policy = json_object(files["payload/policy.json"])
    need(canonical(policy) == canonical(disabled_policy()),
         "Only the disabled fixed family policy may be installed")
    return {"directory": directory, "manifest": manifest, "manifest_sha256": manifest_sha256,
            "files": files}


def targets(bundle):
    items = []
    def directory(path, owner="root", group="root", mode=0o755):
        items.append({"path": str(path), "kind": "directory", "owner": owner, "group": group, "mode": mode})
    directory(POLICY.parent)
    directory(INSTALL)
    directory(ROOT)
    for name in ("control", "locks", "intake", "jobs", "receipts"):
        directory(ROOT / name, mode=0o700 if name == "control" else 0o755)
    for repo in REPOS:
        directory(ROOT / "control" / repo, mode=0o700)
        directory(ROOT / "intake" / repo, group=repo + "-cpu-job", mode=0o750)
        directory(ROOT / "intake" / repo / "admissions", group=repo + "-cpu-job", mode=0o750)
        directory(ROOT / "jobs" / repo, owner=repo + "-cpu-job", group=repo + "-cpu-job", mode=0o700)
        directory(ROOT / "receipts" / repo, group=repo + "-ci-control", mode=0o750)
    for path, raw, mode in [(POLICY, bundle["files"]["payload/policy.json"], 0o444),
                            *((INSTALL / name, bundle["files"]["payload/" + name], 0o444) for name in CODE),
                            (ROOT / "locks/cpu.lock", b"", 0o600)]:
        items.append({"path": str(path), "kind": "file", "owner": "root", "group": "root",
                      "mode": mode, "sha256": digest(raw)})
    return items


def owner_ids(item, accounts, fs):
    owner = accounts.get(item["owner"], {}).get("user")
    group = accounts.get(item["group"], {}).get("group")
    return (fs.root_uid if item["owner"] == "root" else owner["uid"] if owner else None,
            fs.root_gid if item["group"] == "root" else group["gid"] if group else None)


def validate_seal(bundle, fs):
    """Check a preexisting owner-sealed packet; never seal or repair it ourselves."""
    fs.ancestors(bundle["directory"], include_leaf=True)
    for name in ("manifest.json", *PACKET_FILES):
        path = bundle["directory"] / name
        fs.ancestors(path)
        entry = fs.inspect(path)
        need(entry and entry["kind"] == "file" and entry["uid"] == fs.root_uid
             and not entry["mode"] & 0o222, "Apply requires a root-owned read-only sealed bundle: " + str(path))
        expected = (bundle["manifest_sha256"] if name == "manifest.json"
                    else bundle["manifest"]["files"][name])
        need(digest(fs.read(path)) == expected, "Sealed bundle changed: " + str(path))


def preflight(bundle, fs, accounts):
    prerequisites = accounts.prerequisites(fs)
    rows = {name: accounts.snapshot(name) for name in USERS}
    uids, gids = set(), set()
    actions = []
    for name, row in rows.items():
        validate_account(name, row, accounts)
        for kind, ids, field in (("user", uids, "uid"), ("group", gids, "gid")):
            if row[kind]:
                need(row[kind][field] not in ids, "Family account identities overlap")
                ids.add(row[kind][field])
            else:
                actions.append({"kind": kind, "name": name})
    # Groups must exist before their corresponding no-home users.
    actions.sort(key=lambda item: item["kind"] != "group")
    expected = targets(bundle)
    allowed = {Path(item["path"]) for item in expected}
    for item in expected:
        fs.ancestors(item["path"], allowed_missing=allowed)
    for root in (POLICY.parent, INSTALL, ROOT):
        pending = [root]
        while pending:
            path = pending.pop()
            entry = fs.inspect(path)
            if entry:
                need(path in allowed and entry["kind"] in {"directory", "file"},
                     "Unexpected or unsafe managed entry: " + str(path))
                if entry["kind"] == "directory":
                    pending.extend(fs.children(path))
    for item in expected:
        path = Path(item["path"])
        entry = fs.inspect(path)
        if not entry:
            actions.append({"kind": item["kind"], "path": item["path"]})
            continue
        uid, gid = owner_ids(item, rows, fs)
        need(uid is not None and gid is not None and entry["kind"] == item["kind"]
             and entry["uid"] == uid and entry["gid"] == gid and entry["mode"] == item["mode"],
             "Existing target differs: " + str(path))
        if item["kind"] == "file":
            need(entry["links"] == 1 and digest(fs.read(path)) == item["sha256"],
                 "Existing file differs: " + str(path))
    return {"format": "ice-family-disabled-plan-v1", "bundle_sha256": bundle["manifest_sha256"],
            "source_commit": bundle["manifest"]["source_commit"], "enabled": False,
            "system_prerequisites": prerequisites,
            "accounts": rows, "targets": expected, "actions": actions,
            "excluded": ["sudo delegation", "services", "registration", "dispatch", "tools",
                         "dependencies", "credentials", "provider/collector", "source jobs"]}


def apply(bundle, plan, approved_digest, fs, accounts):
    """One-way disabled bootstrap; failures retain and report partial state, never roll it back."""
    completed, current = [], None
    try:
        need(digest(canonical(plan)) == approved_digest, "Reviewed plan digest differs")
        need(preflight(bundle, fs, accounts) == plan, "Machine changed after reviewed preflight")
        indexed = {item["path"]: item for item in plan["targets"]}
        for current in plan["actions"]:
            if current["kind"] in ("user", "group"):
                row = accounts.snapshot(current["name"])
                need(row[current["kind"]] is None, "Account appeared after preflight")
                accounts.create(current["kind"], current["name"])
                validate_account(current["name"], accounts.snapshot(current["name"]), accounts)
            else:
                item = indexed[current["path"]]
                need(fs.inspect(item["path"]) is None, "Target appeared after preflight")
                rows = {name: accounts.snapshot(name) for name in USERS}
                uid, gid = owner_ids(item, rows, fs)
                need(uid is not None and gid is not None, "Missing prepared identity")
                if item["kind"] == "directory":
                    fs.directory(item["path"], uid, gid, item["mode"])
                else:
                    raw = (b"" if item["path"].endswith("/cpu.lock") else
                           bundle["files"]["payload/policy.json" if item["path"] == str(POLICY)
                                           else "payload/" + Path(item["path"]).name])
                    fs.file(item["path"], raw, uid, gid, item["mode"])
            completed.append(current)
        after = preflight(bundle, fs, accounts)
        need(not after["actions"], "Bootstrap left uncompleted actions")
        return {"status": "disabled_bootstrap_complete", "enabled": False,
                "completed": completed, "post_plan_sha256": digest(canonical(after)),
                "services_started": False, "sudo_delegation": False}
    except (Exception, KeyboardInterrupt) as error:
        return {"status": "partial" if completed or current else "refused", "enabled": False,
                "completed": completed, "failed_action": current,
                "diagnostic": str(error) or type(error).__name__,
                "failed_action_may_have_changed_state": current is not None,
                "rollback_performed": False, "services_started": False}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest-sha256", required=True)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--plan", action="store_true", help="Read-only preflight (the default)")
    mode.add_argument("--apply", action="store_true")
    parser.add_argument("--approved-plan-sha256")
    args = parser.parse_args(argv)
    mutations_started = False
    try:
        need(sys.platform == "linux", "This bootstrap targets Linux only")
        bundle = load_bundle(Path(__file__).parent, args.manifest_sha256)
        fs, accounts = Filesystem(), Accounts()
        if args.apply:
            need(os.geteuid() == 0, "Apply requires Linux root")
            # The installer cannot authenticate itself. The owner must first verify the external
            # installer digest and seal these reviewed bytes; then enforce that root-only handoff.
            validate_seal(bundle, fs)
            need(args.approved_plan_sha256 is not None, "Apply requires the reviewed plan SHA256")
        else:
            need(args.approved_plan_sha256 is None, "Plan does not accept an apply approval")
        plan = preflight(bundle, fs, accounts)
        if not args.apply:
            print(json.dumps({"status": "plan_only", "plan_sha256": digest(canonical(plan)),
                              "plan": plan}, indent=2))
            return 0
        # Lock the sealed installer inode, not an unreviewed writable path on the host.
        with Path(__file__).open("rb") as lock:
            fcntl.flock(lock.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            # Catchable termination uses the same honest partial-action report. SIGKILL or power
            # loss cannot report: the next read-only plan must inspect actual retained state.
            def interrupted(_number, _frame):
                raise InterruptedError("Bootstrap interrupted")
            signal.signal(signal.SIGTERM, interrupted)
            mutations_started = True
            result = apply(bundle, plan, args.approved_plan_sha256, fs, accounts)
        print(json.dumps(result, indent=2))
        return 0 if result["status"] == "disabled_bootstrap_complete" else 1
    except (Exception, KeyboardInterrupt) as error:
        print(json.dumps({"status": "partial" if mutations_started else "refused",
                          "diagnostic": str(error), "mutations_started": mutations_started,
                          "rollback_performed": False}))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
