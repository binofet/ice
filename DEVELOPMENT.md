# Repository and release workflow

ICE keeps its existing `master` default branch. The validated cleanup was merged and
pushed at `249485d`; renderer development continues on `feature/render-parity`. Metagen
and Ignition use `main`. No branch rename is needed.

## Default and feature branches

Use the stable default branch (`master` in ICE) and short-lived `feature/`, `fix/` and
`experiment/` branches. Review meaningful diffs, pass the relevant checks and merge
coherent milestones. Avoid a permanent develop branch and another long-lived catch-all
playground branch. Code can merge without becoming a public release.

Before merging a milestone, check ancestry and reconcile any independent changes.
Preserve commit history; do not force-rewrite the default to manufacture a clean merge. Branch
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

## Linux agent workspace

Spacegoo has a persistent checkout at `~/dev/ice` on
`feature/linux-validation`. Linux agents work and run builds
directly there. Exchange reviewed commits through Git and preserve each host's local
branches; review diffs before pushing. Avoid repeated temporary source copies as the
normal development workflow. The ignored `build/linux-tools/README.md` records the
native dependency setup and Xvfb/GPU commands; it does not depend on `/tmp` tools.
Private Ignition has a separate persistent checkout at `~/dev/ignition`. Reviewed
commits can travel as ordinary Git packs over the owner's existing SSH connection
into its non-checked-out `mac-reviewed` ref, then fast-forward its validation branch.
This needs neither loose source copies nor GitHub credentials installed on the host.

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

## Modern cleanup checkpoint — 2026-10-15

At commit `249485d`, the supported engine contained no OpenGL context or fixed-function renderer. The
native desktop layer owns windows independently of GPU presentation. Airhockey and
SkeeBall retain all 492 relocated files with unchanged contents, plus their port
notes; they are not active build targets. CPU glTF assets retain licenses/provenance.
The retained games need ports onto the new renderer and engine-level physics APIs.

Validation of this source checkpoint:

- macOS: C++20 and C++23 each pass 14 CTest cases and the two-window native smoke.
- Linux/spacegoo: C++23 and C++20 with ASan/UBSan each pass 14 tests and the X11
  two-window smoke under Xvfb. The source copy contains no private Metagen checkout.
- Windows: the complete root project compiles and links with MinGW in both modes;
  no Windows runtime/device validation is available.
- Minimal headless configuration: 7 tests pass without desktop or companion modules.
- iOS/watchOS simulator apps rebuild; Android debug APK builds and passes signature
  verification. No new physical-device deployment is part of this cleanup.
- Formatting and the adopted 16-unit analyzer scope pass. API documentation and
  argument-name checks pass; review comments for semantics as well as coverage.
- Ten Metagen consumer caches verify; the five-test native/RTTR integration passes.

Those cleanup checks did not establish GPU backends, desktop text/IME/pointer support
or game ports. The following work is a separate reviewable milestone.

## Procedural renderer parity — checkpoint `ab6bc38`

Native Metal and OpenGL 4.1+ core share explicit device/pipeline/surface ownership,
URI shader preparation, read-only uniform blocks, procedural draws and RGBA8 capture.
Common device tokens reject foreign pipelines; one common renderer attachment is
allowed per logical window. This is a deliberately small interface; indexed meshes,
material resource sets, scene migration and compute remain to add.

Validation recorded so far:

- Mac: both native backend suites and eight Metal/OpenGL handoffs pass; the same
  logical window and CPU simulation state survive. Failed preparation preserves the
  working pipeline. Borrowed Metal layers reject overlapping common/native renderer
  owners and allow reattachment after retirement. The Ignition consumer also switches
  its actual boot shader.
- Linux/spacegoo: Mesa core-profile tests, OpenGL device replacements and the external
  Ignition executable run under Xvfb. This is software-GPU validation.
- Windows: C++20/23 OpenGL and switching executables cross-compile/link with MinGW;
  runtime graphics, DPI and fullscreen remain unverified.
- Ignition's deterministic Metal/OpenGL boot captures at 1280x800 differ by at most
  3/255 per RGB channel on this Mac. This covers one procedural shader, not future
  materials or cross-driver precision in general.
- Quest: native arm64 OpenXR/GLES probes build/sign in C++20/23, and both have run
  on Quest 3. Recorded stereo/head/controller input and menu/remove-rewear lifecycle
  checks include a wireless C++20 installation. An origin-change anchoring defect was
  found and fixed; host regressions and a correlated fixed-build hardware retest pass,
  including origin-change records and stereo frames after both resume checks.
  [The hardware record](examples/xr/hardware-validation.md) separates these results. Wireless ADB works while awake; reconnect after reboot remains untested.
- iOS/iPadOS: the companion uses the common Metal renderer on physical A17 Pro and
  M2 devices, compiling its URI shader and submitting animated frames. The user
  confirmed touch, rotation and Home/reopen behavior on both. iPad multiwindow
  diagnostics record independent submitted frames, resizes and surface retirement;
  closing one leaves another responsive. Both device and arm64 simulator builds pass.
  See [MOBILE.md](MOBILE.md) for the hardware checkpoint and opt-in diagnostics.

The renderer checkpoint is committed and published. Native text and sampled-image
work builds on that baseline. Ignition pins each published engine checkpoint after
validation; an explicit local checkout override remains available for joint work.
A first submitted frame log measures submission, not physical display
latency or a cold-start benchmark.

## Native input and sampled images — checkpoints `2599509` / `ddfa73e`

The next engine slice adds per-window committed/preedit UTF-8 and interpreted editing,
with native AppKit/IMM/XIM adapters and bounded cancellation-aware queues. It also adds
RGBA8 sampled images to both Metal and modern GL, using fresh storage for safe updates.
Ignition consumes these through its procedural display shader and native font canvases.

Validation of these published commits:

- Mac: C++20/23 each pass 15 CTest cases. Both texture GPU suites and eight
  Metal/OpenGL replacements pass, including orientation, updates, ownership, invalid
  bindings and continued rendering. Format, public API documentation and targeted
  analyzer checks pass.
- Linux/spacegoo: C++20/23 each pass 15 tests, native X11 input and two-window
  lifecycle fixtures, sampled-image tests and eight OpenGL device replacements under
  Xvfb/Mesa. The built-in XIM accepts the exercised commits/edit commands; these
  automated events do not prove interactive candidate selection in a full IME.
- Windows: complete C++20/23 projects compile/link with MinGW, including the native
  IMM adapter and texture/replacement targets. No Windows runtime is available.
- The iOS simulator companion rebuilds against the shared renderer changes.
- Ignition `24d6419` pins `ddfa73e` and consumes native input and sampled images for
  its Unicode terminal and procedural combustion opening. Both application tests pass
  in C++20/23 on Mac; native Metal/OpenGL replacements retain its session.

Native IME interaction is a separate validation scope. The earlier AppKit23 native
delivery fixture passed before late-focus/runloop hardening; an unlocked-desktop rerun
of the final native fixture remains necessary. A locked desktop reports unavailable,
not a pass. No hands-on full IME candidate-selection result is implied.

## Transactional URI shader replacement

`shader_snapshot` reads a canonical requested URI once, validates nonempty UTF-8 source
up to 1 MiB, and retains the exact bytes and SHA-256. Native Metal and GL compile those
bytes without reading the provider again. The provider still owns its allocation/read
budgets; source identity is not a signature or a compiler sandbox.

`live_pipeline` prepares and probes a detached candidate using the caller's actual
bindings. Explicit activation at a host frame boundary checks the expected revision
and logical device identity. One prior pipeline supports undo; successful activation
and undo advance the revision. Invalid source, bindings, stale edits or another device
preserve the active pipeline. Same-device replacement leaves presentation attached.
Renderer replacement reconstructs a new holder from retained source and image data.

Mac C++20/23 strict builds, public headers, CPU snapshot/hash/budget cases and actual
Metal/GL replacement/undo/failure GPU cases pass. The API audit covers 586 declarations
with no missing documentation or argument issues, and the targeted analyzer scope
passes. Ignition consumes the owner through `shader reload` / `shader undo`, retains
authored variants per backend and keeps its window/session through device replacement.
Compilation/probing remains synchronous on the UI/render thread; asynchronous jobs
and a shared agent/session command service remain subsequent work.

Spacegoo C++20/23 each pass all 16 CPU tests, native X11 input, sampled-image GPU
checks, eight device replacements and the transactional pipeline GPU suite. Complete
Windows projects cross-link in both modes; runtime validation remains unavailable.
Ignition `97973f3` also passes both native Linux application tests and shader-command
runs. Native keys in one live window exercise a broken shader file, retained revision,
successful repair and undo to the original hash. Its Windows applications/tests
cross-link in both modes. The shared Metal companion rebuilds for iOS simulator;
fresh graphics-free C++20/23 builds each pass seven CPU tests.

## Recursive data ownership across standard libraries

Hosted CI exposed a recursive `value` relocation defect with the macOS 15.4 SDK's
libc++ 19 headers. An incomplete-type trait was cached as permitting byte relocation;
vector growth then left map nodes pointing at sentinels in released storage. Defining
all `value` special members only after the class is complete preserves proper owned
moves, without changing its storage shape or disabling optimizations globally.

A focused growth/copy/insert/erase/move/empty-record regression precedes the graph
corpus. The older SDK passes optimized C++20/23, Debug C++23 and Address/Undefined
Behavior Sanitizer C++23 checks after the correction; the newer SDK also passes both
headless suites. CI now includes a separate Linux headless sanitizer job, lets every
desktop matrix entry finish independently, and captures native Mac graph crash traces.
