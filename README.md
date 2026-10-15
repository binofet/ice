# ICE

ICE (Infinite Combustion Engine) is a C++20/23 playground engine for native rendering, simulations, live data and
language experiments. This is a breaking rebuild: the fixed-function renderer and
abandoned compatibility APIs have been removed. Native Metal and OpenGL 4.1+ core
backends now share a small programmable rendering interface. Vulkan and D3D12 remain
planned; this first slice draws procedural geometry, rather than complete game scenes.

## Build

Requires CMake 3.20+, a C++20/23 compiler, and the platform SDK. Linux desktop windows
also require X11 and OpenGL development headers. The ordinary build uses vendored dependencies
and checked-in Metagen outputs; it needs no network or private submodule checkout.

```sh
cmake -S . -B build/desktop -DICE_CXX_STANDARD=23 -DCMAKE_BUILD_TYPE=Debug
cmake --build build/desktop --parallel 4
ctest --test-dir build/desktop --output-on-failure
build/desktop/bin/ice_window_smoke
# Explicit GPU/display tests, outside headless CTest:
build/desktop/bin/ice_gl_tests tests/data/gl
build/desktop/bin/ice_render_switch_tests tests/data
# On macOS:
build/desktop/bin/ice_metal_tests tests/data/metal
```

The window smoke test needs a real display (or Xvfb on Linux). CTest is headless,
though the optional companion protocol tests use local sockets. Select
`-DICE_CXX_STANDARD=20` for the other supported language mode. Ninja presets provide
`desktop-debug`, `desktop-cxx20` and `headless-debug` configurations.

## Modules

| CMake target | Responsibility |
| --- | --- |
| `ice::platform` | Explicit host, lifecycle, surfaces, input devices and window event stream |
| `ice::window` | Native AppKit/Win32/X11 windows, independent of a GPU device |
| `ice::math` | Pinned GLM configuration and direct types |
| `ice::uri` | URI parsing, providers and ordered project mounts |
| `ice::assets` | CPU glTF import, animation/skinning and camera math |
| `ice::render` | Backend selection, URI pipeline preparation, procedural draws, presentation and captures |
| `ice::metal`, `ice::opengl` | Native backends with independently retained resource lifetimes |
| `ice::meta`, `ice::data` | Scalar settings proof and validated rich graph preparation/publication |
| `ice::commands` | Optional headless command discovery, schema validation, session revisions and bounded retry receipts |
| `ice::companion`, `ice::companion_web` | Optional gestures, recordings, local pairing, QR/browser controllers |
| `ice::engine` | Convenience aggregate of the core modules and enabled native backends/windows |

Disable `ICE_BUILD_DESKTOP`, `ICE_BUILD_METAL`, `ICE_BUILD_OPENGL`,
`ICE_BUILD_COMPANION` and `ICE_BUILD_TOOLS` for a minimal headless build. The
`headless-debug` preset sets all five. Applications can link only the modules they use.

Metal is enabled by default on supported Apple SDKs; desktop OpenGL defaults to the
desktop-window option. Linux GPU tests can run with Mesa under `xvfb-run -a` and
`LIBGL_ALWAYS_SOFTWARE=1`. The common test performs Metal/OpenGL handoffs on macOS
and OpenGL device replacements on Linux. Windows currently has compile/link coverage.

[The shared command service](docs/commands.md) has no window, GPU, network or model
dependency. Hosts register their operations and invoke them between frames; clients
use the same immutable descriptors and structured results. Disable it with
`ICE_BUILD_COMMANDS=OFF` when unused. `ice::data` also provides bounded native JSON
encoding/decoding through `ice/data/json.h`, alongside the experimental MDB1 format.

## Applications and development

[Airhockey and SkeeBall](EXAMPLES.md) remain source/art/physics examples awaiting a
port onto the modern renderer. They are not targets in this checkpoint. Native
[iOS/Android/watchOS probes](MOBILE.md) remain independently buildable. Ignition is a
separate private application repository; its procedural boot study drives this first
renderer slice. The independently built [Quest OpenXR probe](examples/xr/README.md)
now runs on Quest 3 with tracked stereo views, controller input and verified Wi-Fi
debug access. Its [hardware record](examples/xr/hardware-validation.md) distinguishes
observed behavior from the remaining device/lifecycle tests.

[PLATFORM.md](PLATFORM.md) documents ownership and native window limitations;
[GLTF.md](GLTF.md), [MATH.md](MATH.md) and
[Metagen integration](Tools/MetagenIntegration/README.md) describe the retained core.
See [PLAYGROUND.md](PLAYGROUND.md) for implementation order,
[CODING_STANDARD.md](CODING_STANDARD.md) for API/style rules and
[DEVELOPMENT.md](DEVELOPMENT.md) for repository/submodule/version policy.
