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
