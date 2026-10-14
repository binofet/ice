# Airhockey on macOS

The CMake build retains ICE's fixed-function OpenGL renderer and existing game assets. ICE now owns the AppKit window, NSOpenGLContext, event pump and CoreGraphics cursor capture directly. CoreText supplies font glyphs and OpenAL supplies audio. No window toolkit or network download is needed to build on macOS. See [PLATFORM.md](PLATFORM.md) for all three native backends and their validation status.

## Build and play

Requires CMake 3.20 or newer and Apple's command-line developer tools.

```sh
cmake -S . -B build/macos -DCMAKE_BUILD_TYPE=Debug
cmake --build build/macos -j6
ctest --test-dir build/macos --output-on-failure
open build/macos/bin/Airhockey.app
```

Choose **One Player**, move the mouse to control your paddle, and press Escape to pause. In Two Player mode, the second paddle uses the arrow keys. The window close button and Exit menu both terminate the application cleanly.

The build places assets beside the executable inside the app. The executable selects this directory itself, so launch does not depend on your terminal's working directory. This is a local development bundle: settings are also saved there, and another build can restore the source defaults. Distribution packaging, user-specific settings storage, signing and notarization remain future work.

## Verification

The bounded smoke test opens a real window, renders the menu, single-player game and pause screen, verifies that physics advances, rejects OpenGL errors and empty captures, saves PPM captures from the back buffer before presentation, and shuts down:

```sh
mkdir -p /tmp/ice-macos-smoke
build/macos/bin/Airhockey.app/Contents/MacOS/Airhockey --smoke-test /tmp/ice-macos-smoke
```

Verified on an Apple M2 Max with Apple's OpenGL 2.1 implementation. Core runtime tests cover signed failure results, file read length and cursor preservation, non-destructive opens, CRLF settings parsing and persistence, monotonic timing, and key-release behavior. The smoke test drives state transitions directly; physical mouse/keyboard gameplay and audible sound still require a manual check.

This revival builds the runtime used by Airhockey. Experimental event, reflection and scripting subsystems are excluded. Native Windows/Linux build and test details are in PLATFORM.md; network multiplayer and the entire historical engine interface remain unverified.

## First gameplay revision

Local play now uses 120 Hz physics, zero gravity, a low-drag puck with continuous collision detection and a speed cap, and responsive bounded paddle targets. Mouse displacement is sampled once per frame rather than multiplied by wall-clock time; arrow-key movement uses the physics timestep. The CPU predicts side-rail rebounds and moves behind the puck to strike, with a 100 ms reaction interval and limited speed and acceleration.

Goals reset paddles and allow a short ready interval. Match scoring stops at ten, and replay resets the score display. Pause/resume restores the captured mouse correctly. These changes retain the original artwork and network mode; network multiplayer remains unverified.

The gameplay test exercises a CPU return, rail rebound prediction, fast puck collisions and paddle boundaries. The real-window smoke test also verifies goals, the winning score limit, new-match reset, frozen pause physics and continued simulation after resume. Manual playtesting is still needed to judge feel, difficulty and fun.

The rail-trap revision directs CPU shots toward the goal instead of along the side rail. A puck held near a side rail with little lateral velocity for one second receives a small inward velocity; a stationary corner puck also receives an away-from-end-wall component. This is deliberate arcade recovery for the square-corner geometry, not a change to Box2D's contact solver. A regression fixture uses the original right and lower-right wall dimensions, reproduces the stationary trap without recovery, and verifies escape with recovery. Separate checks cover moving parallel rail trajectories and preserve ordinary rebounds.

## Native-window regression test

```sh
build/macos/bin/ice_platform_smoke
```

This requires access to WindowServer. It creates two native windows with independent OpenGL contexts, verifies framebuffer colors, parent association, title and size changes, fullscreen restoration, native key handlers, focus reset, preview-only close and idempotent cleanup.

## More examples

The same build also produces EngineTest, SkeeBall, OrbitalSalvage, WarehouseStealth,
KartRacer and VoxelSandbox. See [EXAMPLES.md](EXAMPLES.md) for controls, scope and
smoke-test instructions.
