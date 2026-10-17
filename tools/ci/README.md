# ICE CI evidence

`engine-quality.yml` runs on pushes, pull requests and explicit workflow dispatch.
It uses hosted Ubuntu/macOS labels; this configuration does not install or assume
a Spacegoo/self-hosted runner. Provider execution, artifact publication and branch
protection must be commissioned separately. A local helper pass does not prove
that provider loop.

The native matrix builds C++20 and C++23 with `RelWithDebInfo`, strict warnings and
the configured platform compiler. Ubuntu explicitly selects GCC 13. macOS uses
the runner's Xcode compiler and records its version. The separate C++23 Debug
ASan/UBSan job builds only its declared headless ownership/data/physics/Lua/audio
test targets. Its CTest regex is used for both discovery and execution; it does
not represent all CPU tests or graphics coverage. At most three native jobs plus
one sanitizer job run concurrently in one workflow attempt.

The same four native jobs explicitly enable optional audio playback, adding the
real OpenAL Soft loopback, WAV decoder and isolated native-null rejection CTests
to discovered CPU inventory. The latter process requires exact `ALSOFT_DRIVERS=null`
before calling native creation; invoking it without that setting opens nothing.
Ubuntu installs ALSA/PulseAudio development libraries so native output backends
also compile; execution uses loopback or the explicitly forced null backend,
never physical output. Tests measure
spatial panning/distance, low-pass/reverb output and actual HRTF activation, plus
stream ownership, queue/lifetime/lifecycle and malformed WAV behavior. The same
three audio targets are included in the existing scoped sanitizer job.
Instrumentation covers adopted ICE wrapper/decoder/tests, not every upstream DSP translation
unit. No speaker output, microphone or TTS provider is exercised. Matrix names
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
