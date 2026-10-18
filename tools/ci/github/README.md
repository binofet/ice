# Trusted family review and CI gates

This is independently authored source adapted from the reviewed Cosmotomata
control/review pattern. No Cosmotomata code, repository identity, policy, pin,
credential or runner is reused. Nothing in this directory registers a runner,
changes settings, performs a merge or authorizes a release.

The fixed repositories are `binofet/ice`40782766/master/public,
`binofet/metagen`1421012730/main/private and
`binofet/ignition`1420999231/main/private. Automatic integration accepts only
open, non-draft, same-repository, non-fork PRs into their fixed default branches.
Root configures native auto-merge/protections separately after proving capabilities.

## Bootstrap a real native review

Use `gate.py review-body --repo ice --pr NUMBER --head FULL_SHA --base FULL_SHA
--text-file REVIEW.txt` after independently reviewing the exact diff. This command
prints a bounded native review body, and does not post it. Root submits it through
GitHub's actual PR review API with event `COMMENT` (own-account reviews cannot use
`APPROVE`). The record says `decision=approved` or `rejected`, binds both head and
base and hashes the readable review. It is agent review evidence submitted by the
authorized owner, not a claim of a second human approval.

The gate fetches authenticated native review objects. Only `binofet` user reviews
qualify. The latest owner review must retain the exact structured body, commit,
base and approved decision. Edits removing the marker, dismissed/rejected reviews,
unstructured replacements and newer stale-head reviews invalidate older approval.
Issue comments and author-supplied JSON cannot approve a change.

`gate.py gate --repo ice --pr NUMBER` is a read-only audit. Add `--publish` only
from the trusted default-branch checkout or a separately reviewed read-only
installation. For bootstrap, Root copies the exact committed `gate.py` into a new
directory, writes an external manifest with format `ice-family-gate-install-v1`,
`source_commit`40hex and `files:{"gate.py":SHA256}`, makes both files read-only,
and supplies `--install-manifest PATH --install-sha256 INDEPENDENT_DIGEST`.
Root independently selects that digest after reviewing the source; a file and hash
provided by a PR author do not authorize execution. No credential is copied into
the installation. Owned workflow jobs supply only their short-lived `GH_TOKEN`; no personal login or credential file is installed.

## CI evidence

The gate resolves the actual default quality workflow ID/path, selects its latest
`pull_request` run for the current source head and requires every configured job
to complete successfully. Push runs never substitute for merge-candidate tests.
It downloads only the finite current run's actual unexpired artifact IDs, with
bounded ZIP bytes/expansion/file counts, safe paths/types, and project-only roots.
Source/summary identity must match the current head/base/run; all matrix jobs must
produce distinct expected identities and one exact synthetic merge with parents
`[base,head]`. Required stages/suites, measured JUnit, actual counts and no failures,
errors, skips or empty primary suites are checked. Actual GitHub artifact/run
association is the upload receipt; an author-written `publication.json` is not.

On publication the controller refreshes current head/base before every status
write. `reviewed-head` and `source-quality` start pending, then independently pass
or fail. A base/head change prevents stale success. Expensive artifact collection finishes before either success. Native review is
re-fetched immediately before each success write and after publication; a changed
review revokes both contexts. Subsequent edits trigger refresh. Root must serialize
integration and enforce exact-head/current-base checks at merge; status publication
cannot atomically lock a future review edit or target update. This script never
calls a merge endpoint.

Generate repository-specific workflows with `gate.py templates --repo metagen
--output NEW_DIRECTORY`. The new directory receives only that fixed repository
name, default branch and quality-workflow triggers; existing output is refused.
Copy the reviewed controller and generated files into that repository through its
reviewed source change. Templates alone do not install or enable automation.

`templates/family-gate.yml` is an owned Linux control-role, trusted-default-branch controller template;
it never checks out PR head code. It refreshes on PR target changes, quality run
completion, default pushes and manual dispatch. Empty workflow-run PR associations
use the bounded authenticated associated-PR API and exact-head/repository checks.
The read-only `family-review-signal.yml` relays native review changes into a separate
default-branch gate run; no privileged source executes from the review-event PR.
An event number only routes reads; it grants no authority. These templates are not
installed merely by being present here. Fork heads are excluded from owned source jobs and cannot enter automatic owner integration; no hosted fallback is configured. Provider approval and runner/UID isolation remain Nova-owned.

Keep physical hardware, live providers, GPU qualification not explicitly run,
deployment, paid services and release/visibility changes as separate gates.
Root must observe a real failed branch blocking integration, then a fix with native
review, uploaded evidence and fresh passing statuses before claiming commissioning.
