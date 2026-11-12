# ICE

ICE (Infinite Combustion Engine) is a C++20/23 playground engine for native rendering, simulations, live data and
language experiments. This is a breaking rebuild: the fixed-function renderer and
abandoned compatibility APIs have been removed. Native Metal and OpenGL 4.1+ core
backends now share a small programmable rendering interface. An optional
[first Win32 Vulkan slice](docs/vulkan-first-slice.md) uses that interface for bounded mesh
presentation; full Vulkan rendering and D3D12 remain planned. Indexed meshes, explicit depth/blending and procedural overlays share one
render pass on both backends.

## Build

Requires CMake 3.22+, C17/C++20/23 compilers, and the platform SDK. Linux desktop windows
also require X11 and OpenGL development headers. Enabled physics modules download
immutable, hash-checked Box2D/Jolt source archives at first configure; see the
[offline overrides and provenance](docs/physics.md). Checked-in Metagen outputs need
no private SDK/submodule checkout.
The optional interpreted Lua module also fetches pinned official source; its
[runtime and offline build contract](docs/scripting.md) documents the limits.

```sh
cmake -S . -B build/desktop -DICE_CXX_STANDARD=23 -DCMAKE_BUILD_TYPE=Debug
cmake --build build/desktop --parallel 4
ctest --test-dir build/desktop --output-on-failure
build/desktop/bin/ice_window_smoke
# Explicit GPU/display tests, outside headless CTest:
build/desktop/bin/ice_gl_tests tests/data/gl
build/desktop/bin/ice_render_switch_tests tests/data
build/desktop/bin/ice_mesh_tests tests/data/mesh
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
| `ice::input` | CPU [semantic actions and contexts](docs/input-actions.md), independent source contributions and explicit tick frames |
| `ice::math` | Pinned GLM configuration and direct types |
| `ice::uri` | URI parsing, providers and ordered project mounts |
| `ice::assets` | CPU glTF import, animation/skinning, primitive meshes and camera math |
| `ice::render` | Backend selection, URI pipelines, indexed/procedural batches, depth, blending and captures |
| `ice::physics2d`, `ice::physics3d` | Default Box2D/Jolt adapters with explicit fixed ticks and owned body identities |
| `ice::script_lua` | Protected interpreted Lua calls with explicit memory/instruction budgets |
| `ice::audio` | Bounded mono PCM/WAV recording buffers and Android AAudio capture |
| `ice::metal`, `ice::opengl` | Native backends with independently retained resource lifetimes |
| `ice::meta`, `ice::data` | Scalar settings proof and validated rich graph preparation/publication |
| `ice::commands` | Optional headless command discovery, schema validation, session revisions and bounded retry receipts |
| `ice::companion`, `ice::companion_web` | Optional gestures, recordings, local pairing, QR/browser controllers |
| `ice::engine` | Convenience aggregate of the core modules and enabled native backends/windows |

Disable `ICE_BUILD_DESKTOP`, `ICE_BUILD_METAL`, `ICE_BUILD_OPENGL`,
`ICE_BUILD_COMPANION` and `ICE_BUILD_TOOLS` for a minimal headless build. The
`headless-debug` preset sets all five. `headless-tiny` also disables both physics
modules and `ICE_BUILD_SCRIPTING`, including their downloads. Applications can link
only the modules they use. Additional .NET/C#, CPython, V8 and LuaJIT adapters remain
planned; Metagen source providers do not embed those gameplay runtimes.

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
[iOS/Android/watchOS probes](MOBILE.md) remain independently buildable.
The independently built [Quest OpenXR probe](examples/xr/README.md)
now runs on Quest 3 with tracked stereo views, controller input and verified Wi-Fi
debug access. Its [hardware record](examples/xr/hardware-validation.md) distinguishes
observed behavior from the remaining device/lifecycle tests.

[PLATFORM.md](PLATFORM.md) documents ownership and native window limitations;
[GLTF.md](GLTF.md), [MATH.md](MATH.md) and
[Metagen integration](tools/metagenintegration/README.md) describe the retained core.
See [PLAYGROUND.md](PLAYGROUND.md) for implementation order,
[CODING_STANDARD.md](CODING_STANDARD.md) for API/style rules and
[DEVELOPMENT.md](DEVELOPMENT.md) for repository/submodule/version policy.
