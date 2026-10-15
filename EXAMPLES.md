# ICE examples

Configure with `cmake -S . -B build/macos -DCMAKE_BUILD_TYPE=Debug`, then
`cmake --build build/macos -j6`. Executables live under `build/macos/bin`;
on macOS each is an `.app`. Linux and Windows use the same CMake targets; EngineTest and SkeeBall each have
a private folder beneath `bin` to isolate their resources.
Run `ctest --test-dir build/macos --output-on-failure` for headless regression checks.

Every restored/new sample accepts `--smoke-test /absolute/existing/output-directory`.
It opens a real native window, runs a bounded simulation, checks GL errors, captures
its back buffer as PPM, and returns nonzero on failure. Linux requires a GLX display;
Windows needs the OpenAL runtime DLL as with Airhockey.

## Restored samples

**EngineTest** exercises native window creation, model/texture loading, text,
perspective rendering, Lua scene reload, and the existing grid pathfinder.
Space pauses rotation; R reloads `Resource/scripts/scene.lua`; Tab shows the route;
Escape closes. Script reload creates a candidate VM and only replaces the running
VM after validation. Change `rotation_speed` to experiment.

The old unsafe thread/allocator benchmark main has been replaced. The unfinished
ObjectModel/delegate tests remain in source but are excluded from the runnable
sample. This does not claim those APIs have been restored. The original Lua sample
required custom modules; the new scene uses the vendored Lua runtime directly.
Assets were recovered from `~/oldev/icu/Projects/EngineTest/Resource`.

**SkeeBall** restores the existing Bullet game and its menu, scores, audio, and
ramp assets. Aim with the mouse or Left/Right arrows. Hold Space or the left mouse
button to charge; release to launch. Escape pauses. Simulation now advances at
real elapsed time, with fixed Bullet substeps, rather than multiplying time by four.
The smoke run enters gameplay, charges/releases a shot, and requires it to enter play.

Runtime resources were recovered from `~/oldev/icu/Projects/SkeeBall/GameFolder`.
WiiYourself source remains in the project, but its Windows driver is not part of
these desktop targets. Wii input remains unverified. F2 enables the native phone
companion; F3 enables a no-install browser touch controller. Both use the same
validated shot-intent path. See [COMPANION.md](COMPANION.md) for local pairing and
current limitations. SkeeBall physics tuning remains deferred.

## New playable prototypes

These four targets share a small, deterministic simulation layer and procedural
OpenGL rendering. They have objectives and reset/pause controls; they are starting
points for development, not finished games. R resets, Tab pauses, Escape closes.

| Target | Current play | Development pressure |
| --- | --- | --- |
| OrbitalSalvage | W/S thrust, A/D rotate; Space attaches/releases nearby cargo. Tow three crates slowly into the green central dock. | Stable fixed-step simulation, constraints, collisions, replay and world serialization. |
| WarehouseStealth | WASD movement, Shift sneak. Take the gold crate and return to the green exit. Walls block patrol visibility; sustained detection loses. | Navigation, perception, behavior debugging, animation and scripting. |
| KartRacer | W/S throttle/reverse, A/D steering. Reach each yellow checkpoint and complete three laps. | Vehicle physics, contact handling, camera, controller input and timing. |
| VoxelSandbox | Arrows select a column, Q/E select height, Space toggles a block. P saves and L loads `voxel-world.bin` beside the executable. | Chunk meshes, streaming, picking, editing, versioned assets and user storage. |

The prototype kart uses a simple kinematic handling model, stealth uses scripted
patrols rather than a navigation planner, and voxels use a small bounded grid.
Upgrade those systems when their game demands it. The first cleanup before mobile
packaging is to move writable files into OS application-data directories; packaged
assets and user data must have distinct paths.

## Mobile direction

Treat iOS and Android as engine targets, not merely controller hosts. Before adding
new window APIs, separate application lifecycle, render surfaces and input devices.
Support OS-driven callbacks, suspension/resumption, surface loss/recreation,
timestamped touch/motion input, capabilities and application-data paths.

Use SkeeBall to prove that design in two stages: a paired phone controller that
produces calibrated shot intent, then the full game running on a phone. Metal on
iOS and Vulkan on Android are planned backends, not currently implemented support.
Physical-device build/run checks are required before calling either target supported.

First deepen one game at a time: SkeeBall launch/scoring and remote input; salvage
physics/replay; stealth navigation; kart vehicle contacts; voxel chunk rendering.
Keep all four buildable while engine capabilities grow.

## Verification checkpoint — 2026-10-14

All six sample targets built and completed real-window smoke runs on macOS
(Apple M2 Max/OpenGL 2.1) and Linux on spacegoo (Xvfb/GLX/Mesa llvmpipe).
Core runtime, Airhockey gameplay and playground gameplay CTest suites passed on both.
All targets also compiled and linked with 64-bit MinGW on spacegoo. Windows runtime,
physical input feel and audible sound still need hands-on validation.

## SkeeBall arcade presentation

The title screen offers solo play, phone party, settings and quit, using mouse or
Up/Down and Return. The cabinet keeps the original mesh/UV layout with a new
walnut, brass and navy/teal diffuse atlas, matching title art and application icons.
Source art lives in `Projects/SkeeBall/GameFolder/Resource/art`; the title, cabinet
and icon were generated with OpenAI ImageGen for this project. Build-format assets
are checked in so ordinary builds need no image tools or generation service.
The Android launcher, iOS asset catalog, browser icon, macOS bundle and Windows
resource use the matching icon. Watch artwork/deployment remains deferred.

`icShaderGL` is the engine's first small programmable rendering building block:
context-owned vertex/fragment GLSL compilation, link diagnostics, uniform lookup
and explicit binding. Replacement is transactional: bad source leaves the previous
program intact. Release GL objects before destroying their context. The current
implementation targets desktop compatibility OpenGL 2.0+ / GLSL 1.20, using native
symbols on Mac/Linux and context-resolved functions on Windows.

SkeeBall's `ArcadeGrade` copies the backbuffer into a resize-aware texture and draws
a programmable fullscreen color grade, restrained highlight glow and vignette.
The title and QR lobby bypass this pass. This is an initial desktop shader path;
scene geometry still uses the compatibility renderer, with Metal/Vulkan/DX12 and
material/shader reflection remaining future work. Unsupported GL or a shader compile
failure preserves the original rendering. `ICE_DISABLE_SHADERS=1` explicitly tests
that fallback. The game smoke test also checks failed shader replacement diagnostics
and retention of a valid program.

Capture the title/game with `SkeeBall --smoke-test <existing-directory>` or the QR
lobby with `SkeeBall --party-smoke <output.ppm>`. Asset-only builds refresh the
runtime Resource directory. No physics tuning is included in this presentation pass.

### Desktop depth-state regression

The GL device now applies its declared depth-test default immediately after creating
its context. It previously left GL's default (disabled) in place until a UI widget
happened to enable it; the new title screen exposed that dependency, letting back
surfaces overwrite foreground ramps and cage geometry. No mesh winding change was
needed. Run `build/macos/bin/ice_render_gl_tests` in a WindowServer session (or the
Linux target under Xvfb) to verify pixel occlusion in both submission orders, a
negative control with depth disabled, and font-overlay state restoration. This
context-dependent test is separate from headless CTest.

### SkeeBall live surface layout

Title, settings, pause and the solo HUD now use the current drawable dimensions,
not the saved startup resolution. Mouse hit testing converts client coordinates
into the same drawing coordinates. The title artwork uses aspect-preserving cover
cropping; the settings and pause screens use matching opaque arcade panels, avoiding
legacy transparent atlas widgets and underlying scene bleed. The lane projection
also updates its aspect ratio on resize and fullscreen changes.

Solo score and balls-left now share a labeled top-left HUD; they no longer float
over guessed cabinet positions. Balls-left excludes a ball already in flight.
The ring atlas patch now supplies enamel and point values without painted circular
rims: the real mesh provides the ring shapes, so projected UVs no longer bend painted
bands across the walls. Mesh geometry and physics are unchanged.

`SkeeBall --layout-smoke <existing-directory>` captures twelve windowed, compact,
ultrawide, fullscreen and restored layouts. Synthetic in-process input fixtures check
resized mouse hit testing, settings back navigation, volume adjustment and an
in-flight shot surviving pause → settings → back → resume. These checks passed on
macOS (actual fullscreen surface 3072×1280) and Linux/Xvfb (2560×1080), alongside the
normal game smoke. Windows cross-compiles; runtime remains unverified.
