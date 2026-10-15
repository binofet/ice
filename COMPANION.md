# Companion controller milestone

The native mobile probes now contain a companion controller with optional local desktop delivery. They use the
same `icCompanionSession` / `icShotGesture` C++ implementation on iOS, Android and
desktop. The existing bundle/package identifiers are retained so installed probes
upgrade in place. App labels now say **ICE Companion**.

## Controls

The default screen emphasizes Connect and the shot pad. Open **Tools** for motion
selection, calibration, recording, replay and sharing.

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
intent preview, not SkeeBall physics. Replay remains local. Live gestures can drive the desktop SkeeBall game after pairing.

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
Signed deployment was verified on the iPhone and iPad below.

### First physical deployment — 2026-10-14

The signed companion built, installed and launched successfully on an iPhone 15 Pro
Max over the local Wi-Fi connection. Automatic provisioning used the configured
Apple development team. Static runtime libraries explicitly disable code signing;
the app target owns signing. The signed build script preserves those per-target
settings instead of forcing signing on every target through an xcodebuild override.
Physical gesture feel and calibration still need hands-on feedback.

### iPad hardware and watchOS companion — 2026-10-14

The iOS companion was signed, installed and launched on an 11-inch iPad Pro
(4th generation); the user confirmed the Companion Lab screen on hardware.

`Projects/MobileProbe/watchos` adds a watch-only SwiftUI application around the
same C++ `icHost` and `icCompanionSession`. It provides an upward-drag shot pad,
optional calibrated wrist motion, recording to the app's Documents directory,
and replay (a clearly labelled demo is available when no take exists).
It is an input/lifecycle companion, with no game renderer or network delivery yet.
Sensors and ticking stop outside the active scene; suspension cancels gestures
and invalidates calibration. It requests no workout/background session.

Build with `Projects/MobileProbe/watchos/build.sh simulator`, or deploy to an
unlocked, trusted Watch with Developer Mode enabled:

```sh
python3 Projects/MobileProbe/watchos/deploy.py \
  --device '<Watch name or identifier>' --team '<development team>'
```

Minimum watchOS is 10.0. Device and simulator builds succeeded with Xcode 26.3,
including the shared C++ runtime. On a Watch SE simulator (watchOS 11.1), an
upward drag produced a shot and the two-shot demo replay reported matched outputs.
Physical Watch installation remains pending. Developer Mode was enabled on
the Watch, and recreating its Mac developer pairing restored the connection
and mounted developer services. Both Xcode's shared-cache symbol copy and the
app transfer then failed when the connection dropped (IXRemoteErrorDomain 6 /
CoreDevice transport timeout). Charging the unlocked Watch near the USB-connected
iPhone did not resolve the repeated failure. No physical Watch launch is claimed.
A successful build does not validate wrist gestures on hardware.


## Local desktop pairing — 2026-10-14

In SkeeBall press **F2** (or launch with `--companion`). A dedicated screen pauses
the game and displays an IPv4 address, port, and fresh six-digit pairing code on
separate readable lines. On the phone/tablet tap **Connect**, enter those values,
and allow Local Network access if iOS asks. If the permission prompt interrupts
pairing, tap Connect again. The pairing screen dismisses when connected; start a
match from the desktop menu if needed. F2 disconnects and closes the listener.
No port is opened until explicitly enabled. Choose an address on the same LAN;
this first transport uses IPv4 and manual pairing, without discovery or QR codes.

`icCompanionLink` lives in `ICEPlatformRuntime`. It uses nonblocking native UDP
sockets on macOS/iOS, Linux/Android, and Windows. A fixed 40-byte network-order
packet checks magic, version and shot-intent capability before a nonce/session
handshake. Aim/power use bounded 1/10000 quantization, never native object layouts.
One peer owns a session; its session ID identifies the remote input source.
Pairing attempts are rate-limited. The six-digit code is local pairing convenience,
not an encrypted or Internet-facing authentication protocol.

Each live shot has a sequence number; duplicates retain the original accepted or
rejected result. There is at most one pending client shot and one queued host shot.
Client retries and queued host actions expire after 350 ms. An unacknowledged shot
may already have arrived; acknowledgement confirms receipt, not a scoring outcome.
Idle peers expire after three seconds. App inactivity closes the mobile connection
and clears pending intent. A host polling gap over one second invalidates its peer
before draining packets. Reconnecting establishes a fresh session and cannot replay
an old session's launch. The retry limit is not a synchronized network-wide age bound.
Replay never sends shots. SkeeBall rejects input in menus, on focus loss, while paused,
or while a ball is in flight; rejected inputs are not saved for a future ball.

### Verification and diagnostics

Mac and spacegoo Linux pass eight CTest suites, including real UDP packet-loss tests
that drop acknowledgements and duplicate shots. Windows cross-compiles and links;
Windows runtime testing remains unavailable. iOS simulator live touch reached the
Mac receiver, while connected replay produced no network shots. A debug Android
native transport fixture reached the same receiver. That fixture is explicitly
synthetic, not a physical gesture test. A network fixture also triggered a real
SkeeBall Bullet impulse on macOS. The dedicated pairing screen was checked using
the game's framebuffer capture (`--pairing-smoke output.ppm`).

For a standalone receiver:

```sh
build/macos/ice_companion_host --port 47519 --code 726418 --shots 1 --timeout 120
```

It prints the pairing details and received shots as JSON. `--send <IPv4> <port>
<code>` sends a labelled synthetic transport fixture (aim .25 / power .5).
For debug Android automation, explicitly supply `files/network-smoke.txt` in the
app's private directory, containing `<IPv4> <port> <code>`. On launch the debug build
consumes and removes that file, connects and sends one labelled synthetic fixture.
Normal use and release builds never enable this fixture. `connection.txt` with the
same fields pre-fills the native numeric pairing panel without auto-connecting.

The revised controller (Tools hidden by default) was signed, installed and launched
on both the physical iPhone 15 Pro Max and iPad Pro after the networking milestone.
Hands-on LAN gesture feel still needs user feedback; simulator and synthetic
transport fixtures are distinguished above from physical device verification.
