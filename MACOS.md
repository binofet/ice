# ICE on macOS

The modern build owns native AppKit windows independently of its Metal and OpenGL
renderers. Both backends implement the same initial programmable slice: URI-loaded
vertex/fragment pipelines, uniform bytes, procedural triangles, presentation and RGBA8
capture. Airhockey/SkeeBall remain retained examples awaiting modern ports.

```sh
cmake -S . -B build/macos-modern -DCMAKE_BUILD_TYPE=Debug -DICE_CXX_STANDARD=23
cmake --build build/macos-modern --parallel 4
ctest --test-dir build/macos-modern --output-on-failure
build/macos-modern/bin/ice_window_smoke
build/macos-modern/bin/ice_metal_tests tests/data/metal
build/macos-modern/bin/ice_gl_tests tests/data/gl
build/macos-modern/bin/ice_render_switch_tests tests/data
```

Requires CMake 3.20+ and Apple's developer tools. The native test needs WindowServer;
the window smoke briefly opens two black windows and verifies logical/backing sizes,
independent routing, close requests and retirement. The separate GPU tests exercise
shader diagnostics, readback, resource ownership, multiple previews and renderer
handoffs while retaining the same window and CPU state. They run explicitly rather
than through headless CTest. OpenGL uses Apple's 4.1 core profile; advanced features
above that baseline need capability-specific paths on other hosts.
Existing ignored `build/macos` executables belong to the previous renderer and do
not establish that the new source tree still builds the games.

Use [MOBILE.md](MOBILE.md) for independently configured iPhone/iPad/watch probes.
See [PLATFORM.md](PLATFORM.md) for lifetime contracts and platform validation limits.
