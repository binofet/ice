# ICE on macOS

The modern build owns native AppKit windows without creating OpenGL contexts. It
builds CPU modules, regression tests and optional companion tools. Metal rendering
is the next backend milestone; Airhockey/SkeeBall are retained examples awaiting port.

```sh
cmake -S . -B build/macos-modern -DCMAKE_BUILD_TYPE=Debug -DICE_CXX_STANDARD=23
cmake --build build/macos-modern --parallel 4
ctest --test-dir build/macos-modern --output-on-failure
build/macos-modern/bin/ice_window_smoke
```

Requires CMake 3.20+ and Apple's developer tools. The native test needs WindowServer;
it briefly opens two black windows, verifies logical/backing sizes, independent
routing, close requests and retirement, then exits. It does not render a GPU scene.
Existing ignored `build/macos` executables belong to the previous renderer and do
not establish that the new source tree still builds the games.

Use [MOBILE.md](MOBILE.md) for independently configured iPhone/iPad/watch probes.
See [PLATFORM.md](PLATFORM.md) for lifetime contracts and platform validation limits.
