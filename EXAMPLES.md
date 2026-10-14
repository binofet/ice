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
these desktop targets. Wii input and phone input are not implemented in this pass.

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
