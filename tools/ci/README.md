# ICE CI evidence

`engine-quality.yml` prepares owned Linux/X64 and macOS/ARM64 source jobs on
same-repository events. Runner registration, toolchain readiness and actual provider
qualification are separate Nova/Root work; this source change commissions no host.
Legacy `ubuntu-24.04`/`macos-15` job/artifact names preserve the current trusted gate
inventory and do not identify actual images. `source.json.owned_runtime` and the
job summary report the observed OS release, architecture and Python instead.

The explicit `day` plan runs PR/default-push compilation (including public
headers), native/API/Metagen contract tests, formatting, sanitizers and native
graphics as before. It does not produce API-documentation or Clang-Tidy results.
The `nightly` plan additionally requires the real expensive API documentation,
readability/argument-documentation checks (`api`) and Clang-Tidy sweeps (`lint`)
in each native profile. An absent plan marker retains both legacy strict
requirements; unknown or event-conflicting markers fail. No passing/skipped
analysis record is synthesized for a day job. Producer and trusted-gate plans are checked
independently, and only a complete PR event can satisfy protected PR quality.

Routine implementation, focused local configure/build/test, PRs and default
pushes use C++23 for every teammate. Do not routinely repeat C++20 validation.
C++20 compatibility runs nightly. If an actual nightly compatibility failure
appears, assign one agent to diagnose/fix it and run the targeted C++20
reproduction/recheck; other agents continue their C++23 work.

New day evidence declares `standards_plan=day-cpp23-v1` and requires exactly
Linux C++23 native, Mac C++23 native and Linux C++23 sanitizers: three real
profiles/artifacts. Nightly declares `nightly-cpp20-23-v1`, running both native
standards on both platforms plus C++23 sanitizers: five profiles. C++20 day jobs
and artifacts are absent, not synthesized as green/skipped. The two existing
manual auxiliaries remain completed/skipped on PR runs. Missing standards
metadata retains the strict five-profile inventory, including accepted 5b
evidence that already declared day analysis but ran both standards. Analysis
and standards markers have separate compatibility meanings. Unknown/null/mixed
or conflicting standards markers fail; nightly never substitutes for PR quality.

The existing five profiles run nightly at `17 9 * * *` UTC (03:17 Boise during
daylight time, 02:17 during standard time), only while `master` is the fixed
default branch. Manual qualification selects `quality_plan=nightly`,
`diagnostic=none`, the `master` ref and `expected_head` equal to the event SHA.
Other refs, mixed diagnostic/nightly inputs or a stale expected head are refused.
Day and nightly concurrency lanes are separate. This source configuration does
not dispatch, publish, commission or replace any already running cohort.

Native Mac builds use ten workers on the reviewed 12-core/32-GiB host; Linux
builds remain at two. CTest and API analysis each retain two workers explicitly.
Mac engine and optional-consumer builds run through the existing stage helper
and native `/usr/bin/time -l`: real exit status and wall/user/system seconds plus
the reported maximum RSS remain in bounded logs and `build-timings.json`. RSS is
the time-reported process/child maximum, not aggregate parallel-build memory or
system peak usage. `configuration.json` retains CMake generator and hashed
compile-database entry/unique-file counts. Those are configured translation
units, not executed compiler-process counts. Existing stage tables retain the
full measured breakdown; no per-translation-unit profiler is introduced.

The native matrix builds C++23 by day and C++20/C++23 nightly with `RelWithDebInfo`, strict warnings and
the configured platform compiler. Linux requires preprovisioned GCC 13. macOS uses
the runner's Xcode compiler and records its version. The separate C++23 Debug
ASan/UBSan job builds only its declared headless ownership/data/physics/Lua/audio
test targets. Its CTest regex is used for both discovery and execution; it does
not represent all CPU tests or graphics coverage. At most three native jobs plus
one sanitizer job run concurrently in one workflow attempt.

All native profiles explicitly enable optional audio playback, adding the
real OpenAL Soft loopback, WAV decoder and isolated native-null rejection CTests
to discovered CPU inventory. The latter process requires exact `ALSOFT_DRIVERS=null`
before calling native creation; invoking it without that setting opens nothing.
Nova provisions ALSA/PulseAudio development libraries so native output backends
also compile; execution uses loopback or the explicitly forced null backend,
never physical output. Tests measure
spatial panning/distance, low-pass/reverb output and actual HRTF activation, plus
stream ownership, queue/lifetime/lifecycle and malformed WAV behavior. The same
three audio targets are included in the existing scoped sanitizer job. Native
and sanitizer configurations also enable speech jobs and run the portable
`ice_audio_speech` fixture: fake worker callbacks test copied PCM, generation
cancellation, terminal/drained state and budgets; Linux exercises the explicit
unavailable Apple factory. macOS compiles and statically analyzes the installed
Apple provider and its manual qualifier, but never runs native synthesis in CI.
The manual installed-voice executable is deliberately absent from CTest.
Instrumentation covers adopted ICE wrapper/decoder/tests, not every upstream DSP translation
unit. No speaker output, microphone or native TTS provider is exercised. Matrix names
remain unchanged; the trusted controller requires the exact three-profile day
or five-profile legacy/nightly inventory selected by verified source markers.

Each job records the checked-out commit/tree, source head, tested base and declared
submodule pins. A PR must be the exact event merge commit with matching base/head
parents; a push's previous head is recorded as such, not presented as a tested
merge base. Finalization rejects a changed HEAD or tracked source. The folder
policy checks directories, retaining only explicit vendored Khronos/Box2D/Bullet
case-sensitive include contracts; filenames are unaffected. The job never
initializes private Metagen source. Cache hashes are
checked from committed consumer artifacts; optional RTTR and Rust consumers use
their existing pinned public dependencies. FetchContent source overrides are
recorded and rejected as unattested by this hosted profile; the workflow supplies
none. Separate trusted worker profiles need their own source attestation.

`run_stage.py` saves the real process result, wall-clock duration, deadline,
output-byte count and an explicitly bounded log. It retains the last 8 MiB, shows
at most 1 MiB plus the final 4 KiB in provider output, and treats deadline expiry as
exit 124. It terminates only its own spawned process group on POSIX. Stage names
cannot contain paths, and duplicate result IDs are rejected. Commands must not
contain credentials: the helper deliberately records their argument vector.

CTest discovery is retained as JSON. The report checks JUnit case names and
multiplicities against that exact inventory, rejects missing/failed/skipped
required cases, and publishes actual per-CTest measured durations. It does not
invent subcase counts from assertions or stdout. Direct native executables each
become one measured JUnit case; the driver environment is not counted as a test.

Ubuntu executes X11 and Mesa software OpenGL, including texture/skin/HDR/reload
checks, and a separately configured surfaceless EGL GLES suite. Each explicit
native executable runs independently after a successful build, even if another
case fails. GLX/GLES driver descriptions remain in their logs. These checks do
not establish physical Adreno/Quest behavior, Metal parity, Windows execution or
mobile lifecycle coverage. macOS builds the native renderers but does not run GUI
or GPU tests in this workflow. No live provider, microphone, deployment or release
activity is included.

Finalization always attempts a readable job summary plus `summary.json`, selected
compiler/configuration provenance, stage logs and JUnit artifacts after ordinary
stage failures. The upload uses only `build/ci-evidence/`, not developer worktrees,
device recordings, environment dumps or credentials. The final step verifies the
provider's successful upload outcome and artifact ID/URL for the exact run. A
missing upload or incomplete report fails the job. Runner loss, hard cancellation
or provider failure can prevent finalizers; those attempts are not passing gates.
Artifacts expire after seven days. JUnit is downloadable and the per-case table
is visible on the job page; no external test-publishing service is configured.

Focused helper checks need only Python:

```sh
python3 tools/ci/test_evidence.py
```

For a local stage, use a fresh ignored evidence directory:

```sh
CI_EVIDENCE_DIR=build/ci-evidence/local-probe \
  python3 tools/ci/run_stage.py --timeout-seconds 120 probe -- cmake --version
```

The generic helpers do not merge branches or change provider settings.
`spacegoo/`, when configured, is a separate deployment lane with its own intake
authorization and job ownership; it must not reuse another project's runner.

### ICE Xvfb diagnostic preflight

The guarded `xvfb-readiness-v1` manual route remains diagnostic only. Its existing
60-second stage now includes preflight and the subsequent Xvfb/client process
tree. Under the actual owned account (UID/eUID 984 and matching GID/eGID), it
reads the existing Mesa EGL vendor descriptor and intended helper/library files
without executing or loading a graphics image. It requires a bounded valid
version 1.0.x JSON descriptor identifying `libEGL_mesa.so.0`, matching intended
X64 ELF files, root-owned system files without group/other write or setid bits,
executable helpers, and unshadowed Xvfb/xauth commands. Missing, malformed,
mismatched or conflicting graphics-selection inputs fail before Xvfb, with exit
64 and a retained reason when the evidence directory is writable. No privileged
probe or package/socket/driver change is needed.

`xvfb-preflight.json` records descriptor bytes (base64, at most 4096 bytes) and
SHA-256, opened file identities/hashes (at most 32 MiB each), account identity,
and only a fixed graphics/loader selector allowlist (256 characters per value).
Its total bound is 32768 bytes. Package versions are optional observations from
the bounded installed package database; they are explicitly database claims,
not proof of a binary's version. No full environment or credentials are exported.

After successful preflight, the process sets
`__EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json` in
the environment used to execute `xvfb-run`. This restricts EGL vendor discovery
before server startup, independently of `LIBGL_ALWAYS_SOFTWARE=1`.
[libglvnd documents the selector and vendor descriptors](https://github.com/NVIDIA/libglvnd/blob/master/src/EGL/icd_enumeration.md).
Conflicting inherited loader/driver overrides stop the diagnostic; the existing
GL version overrides are removed as before. The Xvfb screen, client order,
direct/wrapped exits, stderr, deadline and original failed evidence are retained.

File observations are intended library identities, not loaded-library or
renderer proof; a bare ICD library name is resolved by the loader later. A
successful preflight permits the diagnostic to continue and does not establish
X11 readiness, GLX success, native quality or an upstream fault's root cause.
The original red run remains red. Actual readiness/renderer output and separate
post-run idle evidence still require a separately admitted owned attempt.

The normal Linux `render_checks.py` parent now uses the same bounded Mesa
descriptor validator and sets the exclusive EGL filename before launching each
Xvfb stage. Invalid or missing descriptors return 64 before any stage, with a
refusal reason on stderr; existing finalization treats missing required results
as failure. Successful validation prints only the descriptor path/hash and
permits all ten original stages, each with its existing deadline and real exit.
It does not run the full diagnostic host preflight or infer renderer coverage
from that printout. Actual GLX output and native results remain required. The
separate GLES workflow, normal job matrix, source guards and report semantics
are unchanged. `graphics_commands_invoked` in future diagnostic records describes
only that preflight phase's own commands, without claiming prior loader or GPU
initialization was measured.


## Owned executor source contract

Source selectors are `[self-hosted,Linux,X64,<repo>.cpu]` and
`[self-hosted,macOS,ARM64,<repo>.cpu]`. Trusted default-branch gate/review signals
use `[self-hosted,Linux,X64,<repo>.control]`; maintained templates generate the
same selectors. No hosted fallback exists. Source job guards exclude fork heads;
these guards/labels are not an isolation boundary against modified untrusted
workflow code. Nova owns provider approval, repository/UID isolation and host trust.

Candidate source run steps use `/usr/local/libexec/ice-family-native-ci/owned-shell {0}`.
It inherits the environment and serializes run children with a 1200-second lock wait;
it is not a security sandbox or CPU quota and does not coordinate human build/GPU
use. Compile worker budgets are Linux2/Mac10, including the Rust proof's nested
build through `CMAKE_BUILD_PARALLEL_LEVEL`. CTest and API workers remain two.
Native CI starts still need a separately admitted resource budget from Root;
this source configuration allocates nothing. The nightly API audit receives
`--workers "$CI_API_PARALLEL"` explicitly; its default outside
that invocation remains four workers. Direct Rust proof compilation passes
`-Ccodegen-units=1` on its actual compiler command.
Trusted gate and review-signal control jobs instead use
`/bin/bash --noprofile --norc -e -o pipefail {0}`. Control accounts intentionally
have no shared CPU-lock access and must not invoke the candidate wrapper.
Fresh dedicated-UID checkout steps set `set-safe-directory: false`, preserving
normal Git ownership checks and avoiding checkout's global safe-directory write.
Pinned checkout actions and `persist-credentials: false` remain unchanged.

Bootstrap uses only the declared Python3.12.15 executable: Linux's versioned
`/opt/ice-family-ci/toolchains/native-20261018-v1/bin/python3.12`, or Mac's
`/opt/homebrew/bin/python3.12`. The dependencies stage checks actual native tools,
creates fresh `build/ci-tools/venv`, then installs exactly reviewed package pins
with its explicit Python and `pip --require-virtualenv`. Personal pip configuration
and indexes are disabled. No job sudo/apt, setup-python action, global pip install
or Rust auto-install exists. Every later source run explicitly activates the venv;
summary/publication use the declared bootstrap Python so early failures can report.

`toolchain.json` binds actual tool paths/versions, package/requirements pins and
runtime facts to the exact source fingerprint. Missing, mismatched or incomplete
owned evidence rejects reporting. Linux native profiles retain software Mesa/Xvfb
GL and surfaceless EGL/GLES; Ignition Quest stages are native EGL/GLES adapter
execution, not Android NDK cross-compilation. Mac retains Metal/OpenGL compilation
and CPU tests; no native Metal/window rendering claim is added. Dependencies must
be provisioned separately; a missing tool fails rather than installing or weakening
coverage. Python tests simulate tools/packages, never install them or compile.

The privileged workflow still checks out current default source and runs the
unchanged trusted gate with only the workflow's short-lived GH_TOKEN. A reviewed
migration-branch workflow is separately privileged YAML; checkout of default
`gate.py` alone does not authorize its steps. `refresh --publish` retains its bulk
open-PR scope. Bootstrap/dispatch trust, current native review/head/base/artifacts,
protected statuses and default selector activation remain independent review gates.
