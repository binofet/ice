# ICE CI evidence

`engine-quality.yml` prepares owned Linux/X64 and macOS/ARM64 source jobs on
same-repository events. Runner registration, toolchain readiness and actual provider
qualification are separate Nova/Root work; this source change commissions no host.
Legacy `ubuntu-24.04`/`macos-15` job/artifact names preserve the current trusted gate
inventory and do not identify actual images. `source.json.owned_runtime` and the
job summary report the observed OS release, architecture and Python instead.

The native matrix builds C++20 and C++23 with `RelWithDebInfo`, strict warnings and
the configured platform compiler. Linux requires preprovisioned GCC 13. macOS uses
the runner's Xcode compiler and records its version. The separate C++23 Debug
ASan/UBSan job builds only its declared headless ownership/data/physics/Lua/audio
test targets. Its CTest regex is used for both discovery and execution; it does
not represent all CPU tests or graphics coverage. At most three native jobs plus
one sanitizer job run concurrently in one workflow attempt.

The same four native jobs explicitly enable optional audio playback, adding the
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
and the trusted controller's exact five-job inventory remain unchanged.

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


## Owned executor source contract

Source selectors are `[self-hosted,Linux,X64,<repo>.cpu]` and
`[self-hosted,macOS,ARM64,<repo>.cpu]`. Trusted default-branch gate/review signals
use `[self-hosted,Linux,X64,<repo>.control]`; maintained templates generate the
same selectors. No hosted fallback exists. Source job guards exclude fork heads;
these guards/labels are not an isolation boundary against modified untrusted
workflow code. Nova owns provider approval, repository/UID isolation and host trust.

All run steps use `/usr/local/libexec/ice-family-native-ci/owned-shell {0}`. It
inherits the environment and serializes run children with a 1200-second lock wait;
it is not a security sandbox or CPU quota and does not coordinate human build/GPU
use. Initial CMake/CTest worker budgets are Linux2/Mac1, including the Rust proof's
nested build through `CMAKE_BUILD_PARALLEL_LEVEL`. The Mac CI start still needs a
released lane from Root; this preparation starts nothing.

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
