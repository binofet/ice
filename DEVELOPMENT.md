# Repository and release workflow

This is the proposed workflow for the rebuilt ICE ecosystem. ICE keeps its existing
`master` default branch; current engine work is on `codex/ice-playground`. Metagen and
Ignition were created on `main`. No engine merge or branch rename has been performed.

## Default and feature branches

Use the stable default branch (`master` in ICE) and short-lived `feature/`, `fix/` and
`experiment/` branches. Review meaningful diffs, pass the relevant checks and merge
coherent milestones. Avoid a permanent develop branch and another long-lived catch-all
playground branch. Code can merge without becoming a public release.

After repository separation and the current cleanup have a reviewed, tested checkpoint,
check that the old default branch is an ancestor, reconcile any independent changes,
then merge the engine milestone into the existing `master`. Preserve commit history;
do not force-rewrite the default to manufacture a clean merge. Branch
protections/release automation are follow-up configuration, not already enabled policy.

## Three repositories and explicit dependency commits

- Metagen owns the generic SDK, portable profile examples and independent conformance CI.
- ICE owns reusable runtime/platform/render/data APIs, native integration and tests.
- Private Ignition owns the application experience, scenes and agent orchestration.

ICE pins Metagen at `Tools/Metagen`. Ignition pins ICE at `deps/ice`. Git records exact
commits, not automatically floating branches. Work on named branches inside submodules;
commit/push a child before committing the parent's pointer. Review generated-cache and
behavior changes in that parent update. A normal ICE build uses checked-in consumer
caches and does not require private Metagen source; source regeneration is explicit.

```sh
git submodule update --init Tools/Metagen
# Inside Tools/Metagen: create/use a named branch, edit, validate, commit and push.
python3 Tools/MetagenIntegration/check.py --update
git add Tools/Metagen Tools/MetagenIntegration/Generated
python3 Tools/MetagenIntegration/check.py --cached-only
# Validate, review and commit the engine dependency update.
```

Do not blindly run recursive `update --remote`: it may select a branch unrelated to the
consumer's validated baseline. Explicitly select the intended child commit and record it.

## Versions

Each repository owns separate `vMAJOR.MINOR.PATCH` tags and release notes. Begin on a
0.x development line; no new engine release tag is created by this plan. Metagen's
initial package development version is `0.1.0-alpha.1`. Before 1.0, minor versions may
break the documented API, with changes explained; patches preserve its advertised
contract. No backward compatibility with the abandoned legacy engine API is required.

Keep package version, serialized schema/profile version, file-format version and native
module ABI version separate. A package tag does not migrate saved data. Pin exact
submodule commits even when selecting a released version. Make Metagen public only after
its explicit licensing, documentation, packaging and conformance release review.
