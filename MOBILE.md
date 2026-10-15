# Native application host milestone

ICE now has an explicit `ice::platform::host` in `engine/include/ice/platform/host.h`.
It is built as `ICEPlatformRuntime`, with no window, OpenGL, audio or legacy engine
dependencies. Desktop ICE links it; mobile probes compile exactly the same C++ source.
The historical game entry points remain compatible. EngineTest now uses host callbacks.

## Ownership and scheduling

The platform owns its OS callbacks and calls `Frame(monotonicSeconds)`; the engine
owns no mandatory loop. Active applications receive updates even with no surface.
The host updates once, then renders each visible drawable surface. Inactive/background
applications do not update or render. Resume resets the clock baseline; large frame
gaps are clamped to 100 ms. This is a scheduling boundary, not a fixed-step physics loop.

A surface has a host-scoped identity, API tag, borrowed handle, pixel dimensions,
scale and independent visibility. Its identity is distinct from a desktop window ID.
Destroy/recreate retires the old identity. Resizing to zero suppresses rendering.
Removal invokes renderer cleanup while the platform handle is still valid. A mobile
scene/activity can lose its surface without destroying application or simulation state.
The renderer remains responsible for context/device loss and resource reconstruction.

Device identities and capabilities are independent of surfaces. Sensor events use
application scope; pointer/key events may target a surface. Input carries contact
identity and a monotonic sample timestamp. Pointer coordinates are normalized from
the top-left; motion uses natural device axes rather than screen-rotated axes.
Acceleration is specific force in m/s² including gravity; gyro is radians/second.
Apple acceleration units/sign are converted to the Android convention. Real-device
axis/calibration checks remain necessary before gesture tuning.

Only `PostInput` is thread-safe. All other calls and callbacks run on the platform's
host thread. Do not mutate the host inside callbacks; queue platform work instead.
Stop explicitly before destroying the client or borrowed handles. Suspension, hidden
or removed surfaces and device removal cancel held input. Stale retired IDs are rejected.
The 1024-event queue cancels held input and discards its batch after overflow, so a
lost release cannot leave a touch/key stuck. Sensor capabilities are reported only when
available, and sampling stops when inactive.

## iOS and iPadOS

`Projects/MobileProbe/ios` is a native UIKit scene app. Each scene owns a UIView with
a CAMetalLayer surface; a process-wide display callback drives the host. Foreground
state is aggregated across scenes, and background scenes hide their own surfaces.
The renderer clears/presents a real Metal drawable. Multitouch and CoreMotion feed
host devices. This proves the boundary; it is not yet ICE's complete Metal renderer.

Build an unsigned simulator or device app:

```sh
Projects/MobileProbe/ios/build.sh simulator
Projects/MobileProbe/ios/build.sh device
```

Requires Xcode with iOS SDKs. The default simulator build is Apple Silicon arm64.
Install/run on a booted simulator:

```sh
xcrun simctl install booted build/ios-simulator/Debug-iphonesimulator/ICEMobileProbe.app
xcrun simctl launch --console-pty booted org.ice.MobileProbe
```

The probe now hosts the [companion controller lab](COMPANION.md). The initial
new-surface button has been replaced by controller controls; use iPadOS window
controls to open additional scenes. Rotate, background/resume, and open another scene
to exercise surface/lifecycle behavior. Physical deployment needs Xcode signing with
an appropriate development team/profile; unsigned device builds cannot be installed.

## Android

`Projects/MobileProbe/android` is a NativeActivity app using Android's native app glue.
Activity state, focus and native window lifetime are separate. Multitouch and available
accelerometer/gyro devices feed the shared host. Sensors stop on pause. Native window
pixels currently provide a software diagnostic renderer with a touch marker. Vulkan
swapchain/device/resource support is a subsequent renderer milestone.

```sh
# Override these for your installed SDK/NDK locations if needed.
export ANDROID_SDK_ROOT="$HOME/Library/Android/sdk"
export ANDROID_NDK_ROOT="$PWD/build/android-sdk/ndk/26.3.11579264"
Projects/MobileProbe/android/build.sh
"$ANDROID_SDK_ROOT/platform-tools/adb" install -r build/android-probe/ICE-Mobile-Probe.apk
"$ANDROID_SDK_ROOT/platform-tools/adb" shell am start -n org.ice.mobileprobe/android.app.NativeActivity
"$ANDROID_SDK_ROOT/platform-tools/adb" logcat -s ICEHost
```

The script targets arm64/API 24+, uses SDK/build tools 34, and packages a debug APK
without Gradle or Java application code. It generates an isolated debug signing key
under ignored `build/`; it does not use release signing credentials. A JDK is needed
for Android signing tools; `JAVA_HOME` is configurable. On this Mac the missing NDK
was installed under `build/android-sdk`, leaving the shared SDK untouched.

Use the emulator for callbacks, resize/rotation, surface recreation, touch, disconnect
recovery and synthetic sensors. It cannot establish physical gesture feel, battery
behavior, real GPU performance or device-specific driver correctness.

## Next milestones

1. Companion gestures, touch fallback and recorded intent replay are implemented
   in [COMPANION.md](COMPANION.md). Paired IPv4 local networking and SkeeBall delivery are implemented; a no-install browser touch path is available through desktop F3. Discovery, QR joining and multiplayer remain next.
2. Platform-owned asset/user-data paths, process-death restoration and host migration
   for the other samples. Desktop keyboard/mouse remains the compatibility singleton;
   it has not yet been migrated to the new device stream.
3. A reusable Metal renderer and Android Vulkan renderer: resource lifetime, pipelines,
   shaders, textures, meshes and device loss; then full SkeeBall on the phone.
4. Deepen OrbitalSalvage first for simulation/physics/replay development. Keep the
   other three prototypes building as clients of the same engine interfaces.

SkeeBall physics tuning is intentionally deferred. Mobile is now under native
engine development, but the full legacy game stack is not yet mobile-compatible.

## Verification — 2026-10-14

- Mac and Linux: four CTest suites pass, including host routing/lifecycle tests.
- EngineTest's callback-host smoke passes on Mac and Linux, including background
  suppression and surface retirement/recreation. Existing Linux game smokes pass.
- Windows: all desktop targets and the host tests compile/link with 64-bit MinGW;
  runtime remains unverified.
- iPhone 15 Pro Max (iOS 17.2 simulator): native Metal presentation and actual
  foreground/background/resume callbacks observed.
- iPad Pro 11-inch M4 (iPadOS 18.1 simulator): native 1668×2420 Metal surface and
  presentation observed. Multi-scene UI is implemented; manual interaction remains.
- iPhone/iPad arm64 device target: unsigned build succeeds; physical installation,
  iOS touch interaction and CoreMotion calibration remain to be checked on hardware.
- Android API 34 arm64 emulator: APK install/launch, real Android touch begin/end,
  sensor events, portrait/landscape resize, background/resume and a replacement
  native surface (ID 1 retired, ID 2 attached) observed. Software output captured.

The probes provide local controller pairing. Full game rendering
are not part of this milestone.

## watchOS input companion

`Projects/MobileProbe/watchos` is a native SwiftUI watch-only app with an
Objective-C++ bridge to the existing C++ host and companion reducer. It exercises
application lifecycle, touch and wrist sensors without claiming a render surface
or running a full game renderer. Minimum watchOS: 10.0.

See `COMPANION.md` for build/deploy commands. Both simulator and device builds
compile the shared runtime; simulator touch input and two-shot replay have been
verified. Hardware setup requires Developer Mode on the Watch itself, distinct
from the paired iPhone. Connecting the paired iPhone to the Mac by USB resolved
initial Watch transport preparation failures during setup.
