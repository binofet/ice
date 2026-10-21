# Quest 3 hardware validation — 2026-10-15

Native stereo/tracking and both controller triggers have been exercised on a
physical Quest 3. Both C++20 and C++23 APKs have been deployed. The room anchoring
fix passed the menu and remove/rewear check with correlated fixed-build frames and
the user's confirmation of stable floor/pillars. No headset/account identifiers
are stored here.

- Package: `org.ice.xrprobe`, NativeActivity, version `0.1` / code `1`.
- APK: `build/xr/android-23/ICE-XR-Lab.apk`.
- Final trace-enabled C++23 APK SHA-256:
  `28e4583b14c50ed968e946b0da787b6f534c686cddc66ea370f708457b4118eb`.
- Initial USB-tested C++23 APK SHA-256:
  `cc1d5e273a6d617b6d250f9f7128b2345d449182af9e20cdec29723e27bb81bf`.
- Bundled Khronos Android loader: `1.1.63`, verified against `dependencies.json`.
- Install and cold Activity launch succeeded. The app remained running for the user.

## Observed runtime behavior

Bounded `ICE-XR` logs captured actual stereo frames 120 through 1200, and later
samples through 3976. The records report `views=2`, `focused=1` and view flags `15`
(position/orientation valid and tracked). Head positions changed across samples.
A reference-space recenter was also observed reanchoring the room, followed by
fully tracked frames.

Both grip actions became active. The left trigger reached `1.00` at frame 600 and
returned to `0.00` in the next captured sample. The user confirmed that the scene
worked, then opened the Quest system menu and returned, reporting that scene and
controller tracking resumed normally. The app was left running.

The initial system-menu result was a manual confirmation. The later wireless
C++20 pass retained startup and lifecycle logs before the device buffer rotated:

- Runtime: `Oculus`, packed runtime version `58266595784392704` (`207.297.0`).
- Graphics: Adreno 740, OpenGL ES 3.2, driver `V@0837.0.9`, dated 2026-06-24.
- Reference space: STAGE floor. Both eye targets were 1600×1600, three images,
  `GL_SRGB8_ALPHA8`.
- Actual tracked stereo frames 0 through 7345 were sampled with view flags `15`.
- Both grip actions were active. Right trigger reached `1.00` with left at zero;
  later both reached `1.00` and returned to zero.
- FOCUSED → VISIBLE → FOCUSED transitions resumed tracked stereo. A complete
  STOPPING → IDLE → READY → SYNCHRONIZED → VISIBLE → FOCUSED cycle also resumed
  stereo in the same process. The user confirmed menu and remove/rewear resume.

This pass exposed a real bug: the room was placed at the current gaze again after
reference-origin changes, moving the floor/pillars. The fix retains initial
placement, applies valid origin deltas inversely at their display time, and records
unknown-origin continuity explicitly. Host regressions cover transform direction,
rotation/composition order, event timing, repeated initialization and queue bounds.

An initial fixed-build retest lacked correlated rendered frames after a radio
drop; its visual report was not counted as a pass. The final build added a bounded
app-private retained trace, then repeated the test successfully.

## Correlated anchoring retest

The final trace-enabled C++23 APK was installed over Wi-Fi in 2612 ms and launched
with an explicit MAIN action and VR category. Its app-private trace recorded
`Build marker=space-anchor-v2`, PID, runtime/GPU identity, first stereo frame and
focused stereo samples. Android's foreground Activity/window matched that process.
The NDK compiler reports `__cplusplus=202101` for the selected C++23 mode; its actual
compile command used `-std=c++2b`.

After this drawing proof, the user opened/closed the system menu, removed the
headset for five seconds and put it back on, then confirmed: **everything stays
fixed and resumes**. The retained trace was read afterward through this app's
`run-as` identity, and the live process ID still matched. Post-lifecycle records
showed FOCUSED stereo with valid/tracked flags `15` through frame 4460.

Within that Activity instance, three valid reference-origin corrections were
applied without a new gaze placement. They included nonzero translations, including
a -0.4222 m vertical origin delta. Each recorded a preserved-placement transform,
followed by resumed tracked stereo. No unknown-origin or error record occurred.
This combines logged transform/lifecycle evidence with the user's physical visual
check; it is not an automated external measurement of room drift.

The trace also captured an earlier Android DESTROY after STOPPING/IDLE, complete
probe teardown and NativeActivity recreation in the same process. A fresh Activity
places its ephemeral room once; persistent world placement across a full app exit
is not implemented. The final app was left running, with no extra deployment after
the successful check.

## Wireless development connection

The headset's Wi-Fi address was read over the already authorized USB transport,
then ADB TCP/IP port 5555 was enabled. After the user accepted the debugging
prompt with **Always allow from this computer**, commands explicitly targeting the
wireless serial returned `device` and model `Quest 3`. The app retained its
original process ID; it was not relaunched or reinstalled for this check. USB was
safe to disconnect after those checks.

After physically unplugging USB, sleep/idle made the transport appear offline.
A targeted reconnect while awake restored access without a new authorization or
key changes. The original app process remained alive and had no saved process-exit
entry; the Quest shell was foreground. Sleep-related disappearance therefore did
not prove an app crash.

An actual C++20 APK replacement over the wireless serial succeeded in 2971 ms,
followed by a cold Activity launch and the stereo/input/lifecycle records above.
That pre-fix C++20 APK SHA-256 was
`f17c19b007cd807c579fa104fbb051c5d2684bcf8c99f3267d260750ed3af2d2`.
The initial fixed C++23 replacement also succeeded in 3586 ms. Reconnect after a headset
reboot remains untested; reboot may require enabling TCP/IP over USB again.

## Other validation

- Strict native compile/link and signed APK verification passed in C++20 and C++23.
- Both configurations use the same isolated XR debug certificate.
- APK inspection confirmed the current app library, exact pinned loader and five
  license/notice assets.
- The exact GLES shader strings compiled and linked with real Mesa OpenGL ES 3.2
  on Linux llvmpipe. This is shader validation, not an emulated OpenXR headset run.
- Pinned clang-format18.1.8 and clang-tidy18.1.8 checks passed on the native probe.
- C++20/C++23 host tests passed for origin transforms and persistent, flushed,
  bounded 64 KiB diagnostic-file rotation.

Long sessions, an application-requested stop while actively running, process
recreation, runtime-loss recovery and recenter timing under load still need
dedicated coverage. The current scene intentionally has
no artificial locomotion, hand skeletons, passthrough or general interaction system.

## Voice capture follow-up — 2026-10-15

Version `0.2` / code `2` uses `org.ice.xrprobe/.CaptureActivity` for the standard
Android runtime microphone permission UI. The user granted `RECORD_AUDIO`; no ADB
permission grant or scripted recording input was used. The fixed C++23 APK SHA-256 is
`96bb9831ec894d8147b87a2e4b89e28a3ae7ffef1692c6e0ea077f7126dfd285`.

The first build produced zero PCM despite successful starts. The NDK contract
identified the bug: `AAudioStream_getState` returns cached state; a nonblocking
`waitForStateChange(UNKNOWN, timeout=0)` is needed to update it before the STARTING
guard. The fixed retained trace identifies `voice-v2-state-pump`, process `10770`,
focused tracked stereo, real first-PCM records and completed captures.

The user confirmed the bar/grip stays red while X is held and turns green on
release. Several successful private captures negotiated mono PCM16 at 48000 Hz:

- Recording `10770-8`: 113280 frames / 2.36 seconds, peak 19321, rounded RMS 369,
  zero rejected frames and zero reported xruns; `voice-1.wav`, 226604 bytes.
- Recording `10770-9`: 63360 frames / 1.32 seconds, peak 7825, rounded RMS 242,
  zero rejected frames and zero reported xruns; `voice-0.wav`, 126764 bytes.
- Recording `10770-10`: 188160 frames / 3.92 seconds, peak 7782, rounded RMS 1097,
  zero rejected frames and zero reported xruns; completed by ordinary release
  after a 4.104-second button hold.

Their latest metadata session is `xr-10770-1792110639986`, build
`space-anchor-v2+voice-v2`, explicit user-PTT consent and release completion. These
are actual acoustic PCM/retention results. Short clips 8/9 yielded only non-speech
labels from the developer-hosted recognizer, so those did not prove intelligibility.
The distinct retained recording `10770-10` was then explicitly retrieved for the
requested local transcription. Whisper `tiny.en` recognized an intelligible short
spoken command; the exact transcript is retained in the maintainer record.

The recognizer was independently checked against a known speech sample. This proves
intelligible user speech through the Quest capture/WAV path, followed by an explicit
developer-hosted recognition step. The APK does not contain ASR or automatically
share audio. Only two completed clips survive on-device, with the older slot
overwritten by the next successful recording. Local proof copies use unique
recording IDs so later slot reuse does not replace their evidence.

The observed wall-held durations were about 0.15–0.18 seconds longer than the
retained PCM because the stream opens/starts on demand. Very short taps can record
nothing and publish no file. All these completions were ordinary button releases,
not an unexpected short duration cap.

Menu/pause cancellation, no restart on resume, the ten-second cap and permission
denial/revocation still need dedicated physical voice coverage. C++20/C++23 signed
arm64 APK builds pass, as do portable buffer/WAV tests in both modes, ASan/UBSan,
retained anchoring/trace host regressions, pinned native analysis and public API
documentation checks. The older tracking/anchoring evidence above remains unchanged.
