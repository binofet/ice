# Dedicated Spacegoo CPU lane

This directory provides a **candidate**, not an installed or registered runner.
It adapts the reviewed Cosmotomata separation of controller, immutable intake,
bounded source job and evidence collector. It shares no Cosmotomata account,
registration, pins, directories, lock inode or repository policy. Existing
Cosmotomata services must stay running and unchanged during this installation.

`contract.py` validates sealed source admissions and independently collected merge
snapshots. `broker.py` is an owner-installed fixed entrypoint. `job.py` builds and
tests exact offline sources as a dedicated repository user. `unittest_xml.py`
exports real Python cases and timings. `admit.py` exposes offline report/merge
verification to the trusted collector. None has provider or merge credentials.

## Trust and dispatch

Public ICE pull-request code must **never run as the control identity**. A runner
label, branch prefix or checkout-side condition is not authorization. A trusted
default-branch/private controller must collect the current PR head, target base,
synthetic candidate and exact-head approval through its own authenticated provider
connection. It stages only that candidate into a credential-free local mirror,
seals an admission, then dispatches the installed broker. It does not check out or
source scripts from the PR on the controller. A self-hosted runner visible to
arbitrary repository workflows is not made safe by this broker; do not register
one in the public repository as a shortcut. Root owns that provider architecture.

Source jobs have separate `ice-cpu-job`, `metagen-cpu-job`, `ignition-cpu-job`
identities and private homes/workspaces. They cannot read controller credentials,
developer homes, the other repositories' intake/jobs or Cosmotomata state. Network
and GPU devices are disabled; disposable loopback fixtures remain available in
the private network namespace. No ordinary job installs dependencies or fetches
Git/package content. Failure or resource contention is nonzero, never a paid or
GPU fallback.

## Disabled bootstrap: phase one only

`bootstrap.py` and `prepare_bootstrap.py` prepare a reviewable **disabled namespace**.
They have not been installed or exercised with root privileges. The preparer
accepts only a clean committed checkout and writes a fresh packet using `git show`
from that exact commit. It reports the installer SHA256, manifest SHA256 and every
payload hash. It cannot overwrite an existing packet. A failed preparation may
leave a partial packet, which must not be used as an installation source.

The bootstrap has a fixed scope: six distinct system users and primary groups
(`ice-ci-control`, `ice-cpu-job`, `metagen-ci-control`, `metagen-cpu-job`,
`ignition-ci-control`, `ignition-cpu-job`), with locked passwords, nologin shells
and `/nonexistent` homes that it never creates. It creates only the three family
namespaces `/etc/ice-family-ci`, `/usr/local/libexec/ice-family-ci` and
`/var/lib/ice-family-ci`. Code and the exact disabled example policy are root-owned
read-only; the separate family lock is root:root0600. Empty per-repository job
directories are job-owned0700; intake and receipt directories have their separate
read groups. Control directories remain root-only in this phase. No executable
permission is granted to installed Python code.

There is **no sudo delegation**, systemd unit, registration, dispatch, source job,
tool/dependency installation, home symlink, credential, collector or provider
configuration. No enable option exists; any policy variation, including
`enabled=true`, is refused. Existing Cosmotomata state is outside the target set.

### Owner trust handoff

The installer cannot authenticate its own first execution. A manifest shipped
beside it is not an independent trust source. Before any privileged execution,
Joel must review the frozen Git source and obtain the installer and manifest
digests through the separately reviewed handoff. A normal unprivileged preparer
invocation from that clean source is:

```sh
python3 tools/ci/spacegoo/prepare_bootstrap.py --output /ABSOLUTE/NEW/REVIEW-PACKET
```

This is preparation, not an instruction to install. After source review, the owner
must copy **only** the listed packet files into a new root-owned staging directory
under root-owned ancestors, independently check the installer and manifest digests
there using trusted system hashing tools, and verify every payload hash in that
manifest. Do not run a checkout-side sealing helper as root. Do not execute the
installer from a developer-owned checkout or through a symlink. Seal packet files
root-owned and read-only (0400 or0444); directories must be root-owned and not
group/other-writable. The root-owned staging directory is separate from the three
installation targets and is retained as installation evidence.

Only after that handoff may the reviewed installed-packet script be interpreted
with the trusted system `/usr/bin/python3 -I -B`. First use explicit `--plan
--manifest-sha256 EXTERNAL_DIGEST`. That path reads state and prints a canonical
plan plus its SHA256; it changes no account or file. An unprivileged initial plan
works when all six users are absent. Existing users require a root read-only plan
to confirm password lock status; an unknown lock state refuses. Review the full
plan and its digest before a separate `--apply --manifest-sha256 EXTERNAL_DIGEST
--approved-plan-sha256 REVIEWED_PLAN_DIGEST` invocation. This document does not
supply machine-specific privileged commands or claim commissioning is ready.

Apply verifies the sealed packet, serializes on its installer inode, and repeats
preflight before creating anything. Existing target type, owner, mode and bytes
must match exactly. Ancestors must be root-owned directories without group/other
write permission; missing non-family ancestors (for example
`/usr/local/libexec`) are prerequisites, not silently created. Existing users must
have the exact primary group, home and shell, a verifiably locked password, no
UID/GID aliases, foreign group members, supplementary groups or shared primary
group. Account tools and nologin must already be root-owned safe system files;
their hashes appear in the plan. New users create no home, mail spool or login-log
initialization. No preexisting mismatch is repaired or overwritten.

Successful reapplication to this still-empty namespace performs no mutations.
An unexpected entry, including one produced by a future commissioning phase,
refuses the phase-one bootstrap rather than reinterpreting the namespace. On a
failure or catchable interruption, the result lists completed actions and the
current action that may have partially changed state. Accounts/directories are
not deleted and no rollback is attempted. SIGKILL or power loss cannot produce a
report; inspect a fresh read-only plan and actual account/filesystem state before
any reviewed retry. A failure during ownership/mode adjustment can intentionally
require manual owner investigation rather than a destructive automatic repair.

Local tests use disposable real filesystem operations plus simulated ownership
and account operations. They cover read-only plans, refusal, exclusive file
publication, partial state, modes and reruns; they do not qualify real root
account creation, shadow access, systemd isolation or provider integration.

## Remaining owner stages and real commissioning

These are explicit owner actions, not actions performed by importing these files.
Do not execute an uninstalled checkout copy as root.

1. Review this exact Git commit and `policy.example.json`. Select the final new
   `/var/lib/ice-family-ci` namespace and the six distinct control/job identities.
   Review required provider check names; the example `cpu20/cpu23/conformance`
   names are placeholders until bound to actual authenticated provider checks.
   Keep `enabled=false` until offline inputs and isolation are qualified.
2. Review and qualify the disabled bootstrap above to create the six nologin
   users, root-owned non-writable ancestors, `/etc/ice-family-ci/policy.json`, and installed code at
   `/usr/local/libexec/ice-family-ci/{contract,broker,job,unittest_xml,admit}.py` root-owned
   read-only. Later invocation uses the trusted `/usr/bin/python3 -I` interpreter;
   the phase-one bootstrap grants no delegation to invoke it.
   Each repo's job root is owned by only its job user, mode0700. The family lock is
   precreated root:root0600 in a root-owned0755 `locks` directory; only the broker
   holds it, through source-unit closure and sealing the report. Create
   `receipts/<repo>` root-owned0750 with that repo's control group. Root seals its
   closed report/receipt there, unreadable and unwritable by source jobs.
   Never modify the existing Cosmotomata lock, users, groups or services.
3. Stage qualified Linux tools at `tools/`: Python environment with the project's
   pinned requirements, .NET SDK, Rust, quality tools and needed native development
   libraries. System GCC/CMake/CTest/Git may be selected explicitly. Provide fixed
   `tools/bin` links for `dotnet`, `rustc` and `clang-format`. Native desktop headers
   and Pango libraries for Ignition can live under `tools/prefix`; no GPU is used.
   Qualify exact versions/hashes; availability on the developer SSH PATH is not
   qualification. Stage immutable Box2D/Jolt/Lua source trees as
   `dependencies/{box2d,jolt,lua}`. Preserve their upstream commit/archive checksums.
4. Under `intake/<repo>/`, stage root-owned, job-readable, credential-free
   `source.git`, `files.sha256` and `admissions/`. Ignition additionally gets its
   independently published ICE child in `ice.git`. Do not initialize private
   Metagen inside ICE; checked-in consumer caches are the ordinary build boundary.
   `files.sha256` contains reviewed SHA256 plus two spaces plus absolute path for
   every staged tool/dependency file and selected system executable. Its digest
   is recorded independently in the sealed admission. Source objects are bound
   by exact Git commit hashes. Intake must be root-owned/read-only to jobs; use
   per-repo read groups/ACLs without exposing peer intake.
5. Root seals `admissions/<32hex>.json` with exactly the fields validated by
   `contract.admission`: format `ice-family-cpu-job-v1`, repo/id, event
   `push|pull_request`, head/base/candidate40hex, dependencies (`ice`40hex only for
   Ignition), SHA256 of installed policy, offline inventory and independent review
   receipt, and numeric provider run ID as a string. A PR candidate must have the
   exact ordered parents `[base,head]`; a push candidate equals head and contains
   base. An existing job ID cannot be reused or overwritten. Policy changes require
   a new admission; no wildcard branches or descendant approvals exist.
6. Configure narrowly scoped sudo for each control identity to invoke only the
   installed `broker.py`; the broker additionally rejects any repo not assigned
   to that identity and accepts only repo+32hex job ID. No shell, interpreter,
   arbitrary executable, env assignment, systemctl or cgroup property is delegated.
   Keep the merger/publisher identity separate from source jobs. No credentials
   belong in this policy, admission, command line or source-executing environment.
7. Qualify in the real final unit BEFORE registering ordinary dispatch: wrong
   caller/job ID, unadmitted/stale source, peer/control/home/device/network access
   refusal; actual loopback fixture success; family-lock contention; timeout and
   descendant cleanup; missing/failed/skipped/zero-test rejection. Keep a separate
   sentinel alive to prove cleanup ownership. Verify cgroup CPU/memory/pids limits
   with actual owner-inspected unit properties. Do not infer installation from
   the local unit tests or argv builder.
8. Seal one real reviewed ICE job, then Metagen and Ignition. Execute each via the
   actual control identity and installed broker. Require nonzero for one deliberate
   test failure, publish its real failed provider check, fix the branch and repeat
   under a NEW admission. Require JUnit and provider-visible artifact collection,
   not merely a green dispatch receipt. Record provider URL, exact source/base,
   tool inventory, report digest and source-unit termination. Root then enables
   the independently reviewed trusted dispatch path with new repository-specific
   labels. Registration secrets are handled privately, never copied into jobs.

The fixed source-job profile is ≤2CPU, 8GiB, 256tasks and one hour, with low priority.
The family lock serializes these three repos only; Cosmotomata's limits remain
independent. `PrivateDevices`, `PrivateNetwork`, `ProtectHome`, strict filesystem
protection and cgroup descendant cleanup are required. Runtime qualification must
prove the configured systemd properties work on this host. The native desktop
developer checkouts under `/home/jmax/dev` are not CI job directories.
The broker stops and checks its fixed source unit on completion, timeout and
catchable cancellation. SIGKILL cannot be caught; the source unit's finite runtime
remains the fallback. A future root-owned dispatch-unit lifetime binding can make
that case immediate; do not claim it is implemented by this candidate.

## Evidence and merge admission

Results are private under `jobs/<repo>/<id>/results`: stage command/exit/time,
bounded1MiB log tails/hashes, exact source/admission/profile, and JUnit. ICE and
Ignition run optimized strict C++20/23 with actual discovered CTest inventory;
Metagen runs provider/cache, formats/codecs, Rust and independently timed Python
cases. Its C#/Rust self-test programs remain program-stage checks, not fabricated
internal case counts. Embedded Python programs inside a CTest are one CTest case;
use their own XML for finer granularity. Zero tests and required skips reject.

Root's collector waits until the source unit is inactive, copies only intended
project evidence into immutable controller-owned storage, records its digest,
and uploads it to the correct provider run. Source-job JSON is not a signature or
an independently authenticated provider receipt. Never publish cross-repository
files or job/control credentials. Preserve failed evidence until reviewed; define
a bounded retention policy before sustained use. This source does not prune jobs.

`contract.verify_merge(policy,snapshot)` requires a **trusted collector** snapshot
(`ice-family-merge-snapshot-v1`) containing repo, exact head/base/candidate,
current_head/current_base, exact-head approved review receipt, and every required
completed-success check with matching source/base/candidate and independently
collected report digest. Missing, duplicate, stale, cancelled, skipped, queued,
failed or uncollected evidence refuses admission. The output grants no merge.
The controller must serialize target changes, re-fetch head/base immediately
before merging and use provider-native head protection/CAS where available. If
the base changes, refresh the merge candidate and rerun affected integration.
Never merge on the strength of an author-supplied report plus its own hash.

The installed collector can run `admit.py job --policy POLICY --admission SEALED
--evidence REPORT --evidence-sha256 INDEPENDENT_DIGEST`, then `admit.py merge
--policy POLICY --evidence PROVIDER_SNAPSHOT --evidence-sha256 INDEPENDENT_DIGEST`.
The former rejects missing stage/profile evidence in addition to source mismatches;
the latter requires the fresh authenticated provider/review snapshot. Both are
pure refusal/admission checks. Neither authenticates a caller-provided digest or
performs integration. System-library versions and OS identity must also be retained
by commissioning; hashing selected executables is not a fully pinned OS image.

Root must first verify actual repository feature/settings support for native
auto-merge or a merge queue; this source asserts none. Required branch checks and
review settings cannot be implemented by a local helper. Release tags, signing,
public visibility, device deployment, microphones, live providers and spending
retain their separate authorization boundaries. Hosted GPU/native/hardware results
are distinct qualifications; this CPU lane certifies none of them.

## Source-only checks

On UNIX hosts with Python and `BUILD_TESTING=ON`, ordinary CTest registers
`ice_ci_spacegoo_contract` and `ice_ci_spacegoo_bootstrap`, each with a 60-second
timeout and CPU/CI labels. These are portable helper-contract suites using only
disposable local files/processes and fake account/ownership operations. They
perform no root, account, service, dispatch or installation actions; a green
result is not evidence of privileged isolation or commissioning.

`python3 tools/ci/spacegoo/test_contract.py` exercises exact-source/base/review
admission, fixed isolation argv, stale/duplicate/missing checks, bounded real
failure/timeout cleanup and real Python per-case JUnit. It uses only disposable
local test processes/files. It does not install, register, merge or dispatch.
