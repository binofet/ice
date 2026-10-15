# Companion controller milestone

The native mobile probes now contain a local companion controller lab. They use the
same `icCompanionSession` / `icShotGesture` C++ implementation on iOS, Android and
desktop. The existing bundle/package identifiers are retained so installed probes
upgrade in place. App labels now say **ICE Companion**.

## Controls

- **Touch drag:** begin in the lower pad, drag upward, then release. Horizontal
  position sets aim; upward distance sets power. Taps and cancelled touches do not fire.
- **Motion swing:** hold the device still and tap **Calibrate**. Then hold a finger
  in the pad, swing, and release near the swing. Aim still comes from touch position.
  Simulators without sensors disable motion selection. Physical feel is unverified.
- **Record:** starts a fresh bounded take, resetting any active gesture. **Stop & save**
  commits it to the app's writable sandbox. Mode/calibration changes stop recording.
- **Replay:** feeds the recorded samples through the same controller. Live input is
  ignored during replay. **REPLAY MATCHED** means emitted shots match the saved
  reference outputs, with tolerance for floating point differences.
- **Share take** on iOS opens the native share sheet for the saved file.

First launch supplies a clearly labelled two-shot demo take. It is not user input and
is not saved until explicitly shared. Recording replaces it. The dotted guide is an
intent preview, not SkeeBall physics. Every shot is local: no pairing, network delivery
or desktop game control is implemented in this milestone.

Calibration requires at least 20 stationary acceleration samples spanning 250 ms,
plausible gravity magnitude and low variation/rotation. Samples use SI units and
natural device axes. A low-pass filtered gravity-relative acceleration magnitude
produces bounded power; this intentionally does not reconstruct phone displacement.
Thresholds need real-device tuning. Android sensor timestamps are converted from
boot time to the pointer/host monotonic clock, re-correlated after resume so deep
sleep does not make a fresh swing appear stale. OS inactivity cancels the gesture, saves an active
take and invalidates the physical calibration pose. Recording/replay can preserve a
calibration snapshot without requiring sensors during playback.

Only the first active contact controls a shot. Contact/device/surface identity prevents
another finger or another scene from releasing it. Motion releases need recent samples;
stale holds, taps, cancellation and background transitions cannot launch a shot.

## Recording and desktop inspection

Takes are versioned text files (`ICE_COMPANION 1`) containing the mode/calibration
snapshot, raw input in arrival order and expected shot intents. Sample timestamps are
preserved. Replay uses timestamps for gesture calculations and a monotonically clamped
arrival-order schedule for presentation. It calls the consumer directly; it does not
inject OS events or reuse recorded IDs as live host registrations.

Recordings are bounded to 18,000 events and five minutes. Loading rejects malformed,
nonfinite, oversized or unsupported-version data without replacing the current take.
Saving writes a temporary sibling file and replaces the previous completed take.
The parser limits files to 8 MiB. This is a diagnostic format, not a networking protocol.

```sh
cmake --build build/macos -j6
build/macos/ice_take_replay path/to/recording.iceinput
```

The tool emits JSON with the reproduced aim/power values. Exit 0 means a match,
1 means output differs, and 2 means invalid/empty input or usage. This lets engine
changes be checked against a phone recording without launching UIKit or Android.

On iOS the last take lives in `Documents/last-take.iceinput`; use **Share take** or
Xcode's app-container tools. On the debug Android app:

```sh
"$ANDROID_SDK_ROOT/platform-tools/adb" exec-out run-as org.ice.mobileprobe \
 cat files/last-take.iceinput > build/android-take.iceinput
build/macos/ice_take_replay build/android-take.iceinput
```

`tests/data/android-touch.iceinput` and `android-motion.iceinput` were generated
through the emulator's native input/sensor paths. They contain a synthetic drag and
a calibrated injected swing, not personal device data.

## Build and validation

The build/install commands remain in [MOBILE.md](MOBILE.md). Both native apps retain
the platform host's lifecycle/surface paths. The Android UI is a software-rendered
controller diagnostic; iOS uses UIKit over a Metal surface. Vulkan and the complete
Metal game renderer remain separate work.

The companion tests cover touch/motion intent, stationary/moving calibration,
cancellation, stale holds, bounded storage, invalid-file preservation, replay mismatch
and native Android trace replay. Native Android record/save/replay produced one shot
at approximately +35% aim / 45% power and reported a match. The same exported take
replays on desktop. Native Android calibration and an injected sensor swing also
produced one motion shot and a matching replay. The iPhone simulator's demo replay
emitted two matching shots.
iPad split-view layouts were visually checked. Real sensor gestures and physical
signing/install remain pending the developer-account/device setup.

## Next milestone

Pair the controller with a desktop host over the local network. Establish a session,
validate version/capabilities, map a peer to an input device, and deliver bounded shot
intent with sequence numbers and duplicate suppression. Show connection state and
recover from app suspension/disconnect without replaying an old launch. Input takes
remain the reproducible debugging path. Keep SkeeBall physics tuning deferred until
the companion can actually control the game.

## Physical iPhone/iPad deployment

Pair each device with this Mac in Xcode, enable **Settings → Privacy & Security →
Developer Mode**, restart, and confirm on the device. The setting may only appear
after pairing starts. Sign into **Xcode → Settings → Apple Accounts**. A USB data
cable is the simplest initial pairing route; subsequent installs can use the same
Wi-Fi network. Devices must be awake/unlocked when Xcode requests it.

Once an account/team is available, the project accepts configurable signing values:

```sh
export ICE_DEVELOPMENT_TEAM=YOURTEAMID
export ICE_BUNDLE_ID=com.yourname.icecompanion
python3 Projects/MobileProbe/ios/deploy.py --device iPhone
```

Use the device name as shown by `xcrun devicectl list devices`, or its identifier.
The command checks Developer Mode, signs with Xcode automatic provisioning, installs,
and launches on the selected paired device over its available USB/Wi-Fi connection.
Xcode handles account authentication; do not put Apple passwords into this script.
A unique bundle ID may be needed for your team. Automatic provisioning can register
the selected device and create development signing assets in that team.

Without a team, the existing build commands remain unsigned and simulator-friendly.
Signed deployment is implemented but cannot be validated until the account, Developer
Mode and development signing assets are available on this Mac.

### First physical deployment — 2026-10-14

The signed companion built, installed and launched successfully on an iPhone 15 Pro
Max over the local Wi-Fi connection. Automatic provisioning used the configured
Apple development team. Static runtime libraries explicitly disable code signing;
the app target owns signing. The signed build script preserves those per-target
settings instead of forcing signing on every target through an xcodebuild override.
Physical gesture feel and calibration still need hands-on feedback.
