# ICE XR Lab

This optional Android NativeActivity is the first native OpenXR hardware probe for
Quest 3. It uses the Khronos OpenXR loader with the shared ICE **OpenGL ES**
renderer and an optional `ice::xr_android_host` adapter. The desktop OpenGL 4.1
profile does not run on Android. The adapter owns native stereo, anchoring and
private push-to-talk; each client owns its scene, resources and application clock.
The shared-renderer extraction has Android compile/link and native Linux GLES
proof; the historical hardware record below predates this extraction.

The scene has a tracked stereo view, floor tiles, two gold pillars and a rotating
teal cube above a plinth. Walk or lean gently to check parallax. This is a stationary
room view; there is no artificial locomotion. Tracked controller grips appear as
small blue (left) and orange (right) blocks. Squeezing a trigger brightens and widens
its block, proving independent pose and pressure actions. The
headset's system/menu button returns to the shell.

Hold **X on the left controller** for explicit push-to-talk microphone capture.
On the first press, accept the Android microphone permission prompt, then release
X and press/hold it again. Red on the small bar below the cube and the left grip
means recording; amber means permission is needed; green means a clip was saved.
Release saves up to ten seconds of negotiated-rate mono PCM16 to app-private WAV.
The cap stops capture and requires a release before starting another. Pause, focus
loss, controller-action loss and permission revocation cancel/discard unfinished
audio. Returning to the app never starts it automatically. Nothing is uploaded,
transcribed or sent to an LLM by this probe.

OpenXR controls frame timing and supplies both view poses at predicted display
time. The app handles ready/running/stopping, visible/focused transitions and
instance loss; ICE's existing host observes application activity. Opaque stereo blend
support is checked before rendering. Normal exit requests a session stop and observes
STOPPING before ending it; a two-second stop-event deadline permits final handle
destruction if the runtime is lost or stops responding. Head/controller tracking poses and actions
remain explicit OpenXR values in this probe, pending a shared XR pose/action API.
If position/orientation is invalid the app submits no scene layer. Stage space is
preferred; local space uses a synthetic floor 1.5 m below its initial origin.
The room is placed in front of the first fully tracked head pose. Session stop/start
does not place it again. A valid runtime reference-origin change transforms the
existing room into the new coordinates at the event's display time, preserving its
physical placement. If the runtime cannot relate its old and new origins, the probe
retains coordinates and logs the loss of known physical continuity; persistent
spatial anchors are a future capability.

GPU work is bounded: two eyes, at most 1600×1600 each, one sample, at most eight
runtime swapchain images per eye, a fixed cube mesh and at most 17 indexed cube draws per eye.
Color images belong to the runtime; the app acquires, waits, renders, flushes and
releases before submitting its projection layer. Runtime sRGB color remains borrowed;
the renderer owns only its local FBO/depth attachments. The same waited acquisition
lease is expired before native release. Availability checks never release
an image after a timeout: ownership is retained and the same image is retried in
later paced frames. No external art is downloaded.
The candlelit typewriter/ribbon-microphone scene belongs to a later Ignition slice.

A future desktop OpenXR host will support a PC-rendered, tethered headset profile,
followed by a wireless streaming profile. Those run rendering on the PC and use
the headset runtime for views and input. ADB over Wi-Fi only transports development
commands and logs; this APK still renders locally on the Quest.

## Build

The existing Android SDK 34, NDK 26.3.11579264, CMake and Android Studio JBR are used
by default. Set `ANDROID_SDK_ROOT`, `ANDROID_NDK_ROOT` or `JAVA_HOME` to override.
Only the arm64-v8a hardware ABI is packaged. Both ICE language modes are supported:

```sh
examples/xr/android/build.sh
ICE_CXX_STANDARD=20 examples/xr/android/build.sh
```

Outputs are `build/xr/android-23/ICE-XR-Lab.apk` and its `android-20` counterpart.
The script downloads only the pinned Khronos 1.1.63 loader AAR into the build cache,
verifies its SHA-256 before extracting, and packages its required native library
and license. `dependencies.json` records the official origin, version, hash and
selected Apache-2.0 license. The APK also carries GLM, ICE and Android glue/toolchain
notices for its existing math/platform/runtime dependencies. A local isolated development key signs the APK;
release signing/account credentials are not used. Both language configurations
share the same isolated XR development key so one can replace the other. This project is independent
of the desktop root build and adds no mandatory runtime dependency.

The origin-change regression runs on the development host without an Android SDK:

```sh
cmake -S examples/xr/tests -B build/xr/anchor-tests -DICE_CXX_STANDARD=23
cmake --build build/xr/anchor-tests
ctest --test-dir build/xr/anchor-tests --output-on-failure
```

## Deploy and inspect

Enable Quest developer mode and allow this Mac's USB debugging key inside the
headset. Connect by a USB data cable and choose the authorized headset serial:

```sh
adb devices -l
adb -s SERIAL install -r build/xr/android-23/ICE-XR-Lab.apk
adb -s SERIAL shell am start -W -a android.intent.action.MAIN -c com.oculus.intent.category.VR -n org.ice.xrprobe/.CaptureActivity
adb -s SERIAL logcat -s ICE-XR OpenXR-Loader
```

After USB authorization, development commands and deployment can use the same
local Wi-Fi network. Check **Always allow from this computer** in the headset's
debugging prompt, then follow Meta's [ADB TCP/IP setup](https://developers.meta.com/vr/documentation/native/android/ts-adb/):

```sh
adb -s USB_SERIAL shell ip route
adb -s USB_SERIAL tcpip 5555
adb connect HEADSET_IP:5555
adb -s HEADSET_IP:5555 get-state
adb -s HEADSET_IP:5555 shell getprop ro.product.model
```

Use the headset's actual `wlan0` address from the first command. Unplug USB after
the explicitly selected wireless transport reports `device` and the expected
model. Use `HEADSET_IP:5555` as the serial for later install/log commands. A headset
reboot may require USB to enable TCP/IP again, and its assigned address can change.
The APK runs independently of either development connection.

The application is labelled **ICE XR Lab** in the headset's unknown-source apps.
The `ICE-XR` log reports runtime/headset/GL versions, eye dimensions, session
transitions, stereo frame counts, tracked positions and controller activity. Hardware success requires
real submitted stereo frames and visible head-motion parallax; APK build/install
alone is not a tracking/rendering validation. Test suspend/resume with the system
menu and taking the headset off, then launch again after a normal exit.

The development APK also retains its own sparse diagnostic records in two app-private
32 KiB files (64 KiB total). These survive logcat rotation and Wi-Fi/debugger loss.
The startup record identifies build marker `space-anchor-v2`, language mode and PID;
subsequent records cover Android commands, idle state, reference-origin changes,
session transitions, the first submitted frame, every 120th submitted frame and
orderly teardown. No system logs or personal device identifiers are collected.
Read only this app's files through its authorized development identity:

```sh
adb -s SERIAL shell run-as org.ice.xrprobe cat files/ice-xr-trace.log
adb -s SERIAL shell run-as org.ice.xrprobe cat files/ice-xr-trace.log.previous
```

The `voice-v2-state-pump` marker identifies the fixed PTT build. Its latest clip metadata is
`files/voice-latest.json`; it names `voice-0.wav` or `voice-1.wav`. Only these two
bounded slots are retained. After explicit recording, inspect the manifest and
retrieve its selected file yourself (replace `SLOT` with that manifest's filename):

```sh
adb -s SERIAL shell run-as org.ice.xrprobe cat files/voice-latest.json
adb -s SERIAL exec-out run-as org.ice.xrprobe cat files/SLOT > quest-recording.wav
```

The JSON includes consent/source/build/session identity, negotiated format/rate,
frames, dropped frames, observed xruns and signal peak/RMS. It publishes only after
the WAV is written; do not assume a new clip exists after a cancellation or error.
See [audio capture](../../docs/audio-capture.md) for the reusable engine API,
resource/privacy limits and the planned recognizer→typed-command-service path.

The `.previous` file appears after the first rotation. A successful Activity launch
or an IDLE session is initialization proof; a `Stereo frame=...` record in the
identified process is required to claim rendering. If the headset shell keeps the
app paused, open **ICE XR Lab** from its unknown-source library while wearing it.

## First hardware smoke

1. Launch while wearing the headset. Expect a stable floor, gold pillars and a
   rotating teal cube with separate left/right views. Check that a small head lean
   changes parallax while the room remains fixed in the world.
2. Move each controller independently. Expect blue/orange grip blocks; squeeze
   each trigger and check that only its block grows brighter and wider.
3. Open the headset system menu, return to the app, then briefly remove and replace
   the headset. Check focused/visible lifecycle transitions and resumed tracking.
4. Exit normally and launch again. Confirm the first stereo submission log and
   the absence of OpenXR/GLES errors.
5. Press/release X once for permission. After granting, press/hold X, say a short
   test phrase, then release. Confirm the red indicator, green completion and a
   new `Voice saved` record with nonzero frames and a matching private WAV/metadata.
6. While holding X, open the system menu or remove the headset. Confirm cancellation
   and that returning does not resume capture until X is released and pressed again.

Microphone permission, the held-X indicator and intelligible speech through the
PCM/private WAV path have now passed on the Quest. A developer-hosted recognizer
transcribed the user's test phrase; no recognizer is embedded in this APK.
Voice-specific menu/pause cancellation and
the ten-second cap remain pending. The older hardware results below describe the
previously verified graphics/anchoring build; the separate hardware record now
also contains the exact fixed voice APK and capture evidence.

Build verification covers strict arm64 C++20/C++23 compile/link, signed APK
contents/manifest, origin-transform/retained-trace host regressions and real Mesa
GLES3 shader compile/link. Both language builds were deployed to a physical Quest 3
on 2026-10-15, including actual wireless install. Logs confirm stereo tracking,
both grip/trigger actions, startup runtime/GPU identity and stop/start/focus cycles
followed by resumed stereo. That testing exposed the origin-change anchoring bug.
The fix passed both host regressions and a physical menu/remove-rewear retest:
the user confirmed stable floor/pillars, and the retained `space-anchor-v2` trace
matched the live process, origin corrections and post-resume tracked stereo.
See [hardware-validation.md](hardware-validation.md) for the exact APK hash,
record and remaining coverage.
