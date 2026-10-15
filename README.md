# ICE

ICE is a C++20/23 playground engine for native rendering, simulations, live data and
language experiments. This is a breaking rebuild: the fixed-function renderer and
abandoned compatibility APIs have been removed. Metal, Vulkan and D3D12 are the next
renderer work; the current desktop build exercises CPU modules and native windows.

## Build

Requires CMake 3.20+, a C++20/23 compiler, and the platform SDK. Linux desktop windows
also require X11 development headers. The ordinary build uses vendored dependencies
and checked-in Metagen outputs; it needs no network or private submodule checkout.

```sh
cmake -S . -B build/desktop -DICE_CXX_STANDARD=23 -DCMAKE_BUILD_TYPE=Debug
cmake --build build/desktop --parallel 4
ctest --test-dir build/desktop --output-on-failure
build/desktop/bin/ice_window_smoke
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
| `ice::meta`, `ice::data` | Scalar settings proof and validated rich graph preparation/publication |
| `ice::companion`, `ice::companion_web` | Optional gestures, recordings, local pairing, QR/browser controllers |
| `ice::engine` | Convenience aggregate of the core modules and enabled desktop windows |

Disable `ICE_BUILD_DESKTOP`, `ICE_BUILD_COMPANION` and `ICE_BUILD_TOOLS` for a minimal
headless build. Applications can link only the modules they use.

## Applications and development

[Airhockey and SkeeBall](EXAMPLES.md) remain source/art/physics examples awaiting a
port onto the modern renderer. They are not targets in this checkpoint. Native
[iOS/Android/watchOS probes](MOBILE.md) remain independently buildable. Ignition is a
separate private application repository and will drive the new interactive renderer.

[PLATFORM.md](PLATFORM.md) documents ownership and native window limitations;
[GLTF.md](GLTF.md), [MATH.md](MATH.md) and
[Metagen integration](Tools/MetagenIntegration/README.md) describe the retained core.
See [PLAYGROUND.md](PLAYGROUND.md) for implementation order,
[CODING_STANDARD.md](CODING_STANDARD.md) for API/style rules and
[DEVELOPMENT.md](DEVELOPMENT.md) for repository/submodule/version policy.
