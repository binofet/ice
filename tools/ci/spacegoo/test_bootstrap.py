"""Disposable filesystem and fake-account checks; never install or create OS accounts."""

from copy import deepcopy
import contextlib
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
import bootstrap as b
import prepare_bootstrap as prepare


class Accounts:
    uid_max = gid_max = 999

    def __init__(self):
        self.rows = {name: {"group": None, "user": None} for name in b.USERS}
        self.calls = []
        self.fail_after = None

    def prerequisites(self, _fs):
        return [{"path": "fake-account-backend", "sha256": "1" * 64, "mode": 0o755}]

    def snapshot(self, name):
        return deepcopy(self.rows[name])

    def create(self, kind, name):
        identity = 700 + b.USERS.index(name)
        self.calls.append((kind, name))
        self.rows[name][kind] = ({"name": name, "gid": identity, "members": [], "aliases": [],
                                 "primary_users": []} if kind == "group"
                                 else {"name": name, "uid": identity, "gid": identity, "home": "/nonexistent",
                                       "shell": "/usr/sbin/nologin", "locked": True, "aliases": [],
                                       "supplementary": [], "shared_primary": []})
        if kind == "user":
            self.rows[name]["group"]["primary_users"] = [name]
        if self.fail_after == (kind, name):
            raise OSError("Injected failure after account creation")


class Filesystem(b.Filesystem):
    """Real exclusive files/modes, simulated chown: no OS privilege or account changes."""

    def __init__(self, base):
        super().__init__(base, os.getuid(), os.getgid())
        self.owners = {}
        self.calls = []
        self.fail_at = None
        for name in ("etc", "usr/local/libexec", "var/lib"):
            (self.base / name).mkdir(mode=0o755, parents=True)

    def inspect(self, path):
        row = super().inspect(path)
        if row and str(path) in self.owners:
            row["uid"], row["gid"] = self.owners[str(path)]
        return row

    def directory(self, path, uid, gid, mode):
        if str(path) == self.fail_at:
            raise OSError("Injected directory creation failure")
        super().directory(path, self.root_uid, self.root_gid, mode)
        self.owners[str(path)] = (uid, gid)
        self.calls.append(("directory", str(path)))

    def file(self, path, raw, uid, gid, mode):
        super().file(path, raw, self.root_uid, self.root_gid, mode)
        self.owners[str(path)] = (uid, gid)
        self.calls.append(("file", str(path)))


class BootstrapTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        self.fs = Filesystem(self.root / "machine")
        self.accounts = Accounts()
        source = Path(__file__).parent
        files = {name: (source / ("policy.example.json" if name == "payload/policy.json"
                                 else Path(name).name)).read_bytes() for name in b.PACKET_FILES}
        self.packet = self.root / "packet"
        self.receipt = prepare.write_bundle(self.packet, "a" * 40, files)
        self.bundle = b.load_bundle(self.packet, self.receipt["manifest_sha256"])

    def plan(self):
        return b.preflight(self.bundle, self.fs, self.accounts)

    def apply(self):
        plan = self.plan()
        return b.apply(self.bundle, plan, b.digest(b.canonical(plan)), self.fs, self.accounts)

    def test_plan_is_read_only_and_reproducible(self):
        before = sorted(str(p.relative_to(self.fs.base)) for p in self.fs.base.rglob("*"))
        plan = self.plan()
        self.assertEqual(plan, self.plan())
        self.assertEqual(before, sorted(str(p.relative_to(self.fs.base)) for p in self.fs.base.rglob("*")))
        self.assertEqual(self.accounts.calls, [])
        self.assertEqual(self.fs.calls, [])
        self.assertFalse(plan["enabled"])
        self.assertEqual(len([x for x in plan["actions"] if x["kind"] == "user"]), 6)
        self.assertNotIn("sudoers", json.dumps(plan))

    def test_apply_exact_modes_bytes_empty_namespaces_and_idempotency(self):
        result = self.apply()
        self.assertEqual(result["status"], "disabled_bootstrap_complete")
        self.assertEqual(len(self.accounts.calls), 12)
        for row in b.targets(self.bundle):
            actual = self.fs.inspect(row["path"])
            self.assertEqual(actual["mode"], row["mode"])
            if row["kind"] == "file":
                self.assertEqual(actual["links"], 1)
                self.assertEqual(b.digest(self.fs.read(row["path"])), row["sha256"])
        for repo in b.REPOS:
            self.assertEqual(self.fs.children(b.ROOT / "jobs" / repo), [])
            self.assertEqual(self.fs.children(b.ROOT / "receipts" / repo), [])
        count = len(self.fs.calls), len(self.accounts.calls)
        self.assertEqual(self.plan()["actions"], [])
        self.assertEqual(self.apply()["completed"], [])
        self.assertEqual((len(self.fs.calls), len(self.accounts.calls)), count)

    def test_wrong_plan_digest_or_changed_machine_mutates_nothing(self):
        plan = self.plan()
        result = b.apply(self.bundle, plan, "0" * 64, self.fs, self.accounts)
        self.assertEqual(result["status"], "refused")
        self.accounts.create("group", b.USERS[0])
        self.accounts.calls.clear()
        result = b.apply(self.bundle, plan, b.digest(b.canonical(plan)), self.fs, self.accounts)
        self.assertEqual(result["status"], "refused")
        self.assertEqual(self.accounts.calls, [])
        self.assertEqual(self.fs.calls, [])

    def test_existing_account_lock_home_shell_alias_and_membership_mismatch_refuse(self):
        name = b.USERS[0]
        self.accounts.create("group", name)
        self.accounts.create("user", name)
        good = deepcopy(self.accounts.rows[name])
        changes = [("user", "locked", False), ("user", "locked", None), ("user", "uid", 0),
                   ("user", "home", "/home/developer"), ("user", "shell", "/bin/sh"),
                   ("user", "supplementary", ["sudo"]), ("user", "aliases", ["peer"]),
                   ("user", "shared_primary", ["peer"]), ("group", "aliases", ["peer"]),
                   ("group", "members", ["peer"]), ("group", "primary_users", ["peer"]),
                   ("user", "name", "canonical-peer"), ("group", "name", "canonical-peer")]
        for kind, key, value in changes:
            with self.subTest(key=key, value=value):
                self.accounts.rows[name] = deepcopy(good)
                self.accounts.rows[name][kind][key] = value
                with self.assertRaisesRegex(ValueError, "Existing"):
                    self.plan()
        self.assertEqual(self.fs.calls, [])

    def test_nss_group_only_foreign_primary_user_refuses_before_any_action(self):
        name = b.USERS[0]
        group = b.grp.struct_group((name, "x", 700, []))
        foreign = b.pwd.struct_passwd(("foreign", "x", 701, 700, "", "/nonexistent", "/usr/sbin/nologin"))
        accounts = b.Accounts.__new__(b.Accounts)
        with patch.object(b.grp, "getgrnam", return_value=group), \
                patch.object(b.grp, "getgrall", return_value=[group]), \
                patch.object(b.pwd, "getpwnam", side_effect=KeyError(name)), \
                patch.object(b.pwd, "getpwall", return_value=[foreign]):
            row = accounts.snapshot(name)
        self.assertIsNone(row["user"])
        self.assertEqual(row["group"]["primary_users"], ["foreign"])
        self.accounts.rows[name] = row
        with self.assertRaisesRegex(ValueError, "Existing group differs"):
            self.plan()
        self.assertEqual(self.accounts.calls, [])
        self.assertEqual(self.fs.calls, [])

    def test_nss_canonical_names_refuse_even_when_enumeration_omits_aliases(self):
        name = b.USERS[0]
        accounts = b.Accounts.__new__(b.Accounts)
        for kind in ("group", "user"):
            with self.subTest(kind=kind):
                group = b.grp.struct_group(("other" if kind == "group" else name, "x", 700, []))
                user = b.pwd.struct_passwd(("other" if kind == "user" else name, "x", 700, 700,
                                          "", "/nonexistent", "/usr/sbin/nologin"))
                with patch.object(b.grp, "getgrnam", return_value=group), \
                        patch.object(b.grp, "getgrall", return_value=[]), \
                        patch.object(b.pwd, "getpwnam", return_value=user), \
                        patch.object(b.pwd, "getpwall", return_value=[]), \
                        patch.object(b.os, "geteuid", return_value=0), \
                        patch.object(b.Path, "read_text", return_value=name + ":!:fields\n"):
                    row = accounts.snapshot(name)
                self.assertTrue(row["user"]["locked"])
                self.assertEqual(row["user"]["aliases"], [])
                self.assertEqual(row["group"]["aliases"], [])
                self.accounts.rows[name] = row
                with self.assertRaisesRegex(ValueError, "Existing"):
                    self.plan()
        self.assertEqual(self.accounts.calls, [])

    def test_symlink_writable_or_missing_unplanned_ancestor_refuses_before_accounts(self):
        ancestor = self.fs.path(Path("/usr/local"))
        ancestor.rename(ancestor.with_name("saved"))
        ancestor.symlink_to("saved", target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "Unsafe ancestor"):
            self.plan()
        ancestor.unlink()
        ancestor.with_name("saved").rename(ancestor)
        ancestor.chmod(0o777)
        with self.assertRaisesRegex(ValueError, "Unsafe ancestor"):
            self.plan()
        ancestor.chmod(0o755)
        self.fs.path(Path("/usr/local/libexec")).rmdir()
        with self.assertRaisesRegex(ValueError, "Missing unplanned ancestor"):
            self.plan()
        self.assertEqual(self.accounts.calls, [])

    def test_unexpected_managed_content_symlink_modes_and_hardlinks_refuse(self):
        self.assertEqual(self.apply()["status"], "disabled_bootstrap_complete")
        unknown = self.fs.path(b.ROOT / "foreign")
        unknown.write_bytes(b"preserve")
        with self.assertRaisesRegex(ValueError, "Unexpected"):
            self.plan()
        unknown.unlink()
        target = self.fs.path(b.POLICY)
        target.chmod(0o644)
        with self.assertRaisesRegex(ValueError, "Existing target differs"):
            self.plan()
        target.chmod(0o444)
        os.link(target, self.root / "alias")
        with self.assertRaisesRegex(ValueError, "Existing file differs"):
            self.plan()
        (self.root / "alias").unlink()
        target.unlink()
        target.symlink_to(self.packet / "payload/policy.json")
        with self.assertRaisesRegex(ValueError, "unsafe managed entry"):
            self.plan()

    def test_exclusive_publication_never_overwrites_concurrent_target(self):
        self.fs.directory(b.POLICY.parent, self.fs.root_uid, self.fs.root_gid, 0o755)
        target = self.fs.path(b.POLICY)
        target.write_bytes(b"appeared after preflight")
        with self.assertRaises(FileExistsError):
            self.fs.file(b.POLICY, b"replacement", self.fs.root_uid, self.fs.root_gid, 0o444)
        self.assertEqual(target.read_bytes(), b"appeared after preflight")
        self.assertEqual(list(target.parent.glob(".disabled-bootstrap-*")), [])

    def test_partial_account_and_directory_failure_retains_actual_state(self):
        failed = ("user", b.USERS[0])
        self.accounts.fail_after = failed
        result = self.apply()
        self.assertEqual(result["status"], "partial")
        self.assertTrue(result["failed_action_may_have_changed_state"])
        self.assertEqual(result["failed_action"], {"kind": failed[0], "name": failed[1]})
        self.assertIsNotNone(self.accounts.snapshot(failed[1])["user"])
        self.assertFalse(result["rollback_performed"])
        self.accounts.fail_after = None
        self.fs.fail_at = str(b.INSTALL)
        result = self.apply()
        self.assertEqual(result["status"], "partial")
        self.assertTrue(self.fs.path(b.POLICY.parent).is_dir())
        self.assertFalse(self.fs.path(b.INSTALL).exists())
        self.fs.fail_at = None
        self.assertEqual(self.apply()["status"], "disabled_bootstrap_complete")

    def test_interruption_reports_attempted_action_without_rollback(self):
        def interrupt(_kind, _name):
            raise KeyboardInterrupt()
        self.accounts.create = interrupt
        result = self.apply()
        self.assertEqual(result["status"], "partial")
        self.assertEqual(result["completed"], [])
        self.assertTrue(result["failed_action_may_have_changed_state"])
        self.assertFalse(result["rollback_performed"])

    def test_packet_and_manifest_tampering_or_enabled_policy_refuses(self):
        with self.assertRaisesRegex(ValueError, "manifest SHA256 differs"):
            b.load_bundle(self.packet, "0" * 64)
        for policy_change in ({"enabled": True}, {"enabled": 0},
                              {"root": "/tmp/other"}, {"extra": True}):
            with self.subTest(change=policy_change):
                files = dict(self.bundle["files"])
                policy = b.disabled_policy()
                policy.update(policy_change)
                files["payload/policy.json"] = b.canonical(policy)
                with self.assertRaisesRegex(ValueError, "disabled phase-one policy"):
                    prepare.write_bundle(self.root / "invalid", "a" * 40, files)
                self.assertFalse((self.root / "invalid").exists())
        link = self.root / "symlinked-packet"
        link.symlink_to(self.packet, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "Unsafe bundle directory"):
            b.load_bundle(link, self.receipt["manifest_sha256"])
        path = self.packet / "payload/broker.py"
        path.chmod(0o600)
        path.write_bytes(b"changed bytes")
        with self.assertRaisesRegex(ValueError, "Bundle file differs"):
            b.load_bundle(self.packet, self.receipt["manifest_sha256"])

    def test_packet_existing_output_is_not_merged_or_overwritten(self):
        before = (self.packet / "manifest.json").read_bytes()
        with self.assertRaises(FileExistsError):
            prepare.write_bundle(self.packet, "b" * 40, self.bundle["files"])
        self.assertEqual((self.packet / "manifest.json").read_bytes(), before)

    def test_seal_checks_ownership_write_modes_ancestors_and_current_bytes(self):
        # Simulated root identity; actual file modes/bytes and unsafe ancestors are checked.
        fs = b.Filesystem(self.root, os.getuid(), os.getgid())
        bundle = dict(self.bundle, directory=Path("/packet"))
        b.validate_seal(bundle, fs)
        installer = self.packet / "bootstrap.py"
        installer.chmod(0o600)
        with self.assertRaisesRegex(ValueError, "read-only sealed bundle"):
            b.validate_seal(bundle, fs)
        installer.chmod(0o400)
        self.packet.chmod(0o777)
        with self.assertRaisesRegex(ValueError, "Unsafe ancestor"):
            b.validate_seal(bundle, fs)
        self.packet.chmod(0o700)
        with patch.object(fs, "inspect", wraps=fs.inspect) as inspect:
            def wrong_owner(path):
                row = b.Filesystem.inspect(fs, path)
                if path == Path("/packet/bootstrap.py"):
                    row["uid"] += 1
                return row
            inspect.side_effect = wrong_owner
            with self.assertRaisesRegex(ValueError, "root-owned"):
                b.validate_seal(bundle, fs)
        installer.chmod(0o600)
        installer.write_bytes(b"changed after initial read")
        installer.chmod(0o400)
        with self.assertRaisesRegex(ValueError, "Sealed bundle changed"):
            b.validate_seal(bundle, fs)

    def test_preparer_refuses_dirty_checkout_before_output_creation(self):
        with patch.object(prepare.subprocess, "check_output",
                          side_effect=[str(self.root), "a" * 40, b"?? dirty\n"]), \
                contextlib.redirect_stderr(io.StringIO()) as error:
            self.assertEqual(prepare.main(["--output", str(self.root / "not-created")]), 1)
        self.assertIn("clean committed checkout", error.getvalue())
        self.assertFalse((self.root / "not-created").exists())

    def test_cli_explicit_plan_has_no_account_filesystem_or_subprocess_writes(self):
        real_fs = b.Filesystem()
        with patch.object(b, "__file__", str(self.packet / "bootstrap.py")), \
                patch.object(b.sys, "platform", "linux"), \
                patch.object(b, "Filesystem", side_effect=[real_fs, self.fs]), \
                patch.object(b, "Accounts", return_value=self.accounts), \
                patch.object(b.subprocess, "run", side_effect=AssertionError("must not run")), \
                contextlib.redirect_stdout(io.StringIO()) as output:
            self.assertEqual(b.main(["--plan", "--manifest-sha256", self.receipt["manifest_sha256"]]), 0)
        self.assertEqual(json.loads(output.getvalue())["status"], "plan_only")
        self.assertEqual(self.accounts.calls, [])
        self.assertEqual(self.fs.calls, [])


if __name__ == "__main__":
    unittest.main()
