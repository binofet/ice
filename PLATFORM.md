# ICE native platform layer

ICE owns its platform code again. The toolkit wrapper and vendored dependency have been removed. The public window factory and the per-platform classes remain the entry points; Airhockey uses the common window API rather than a toolkit-specific main-window singleton.

| Platform | Window / events | Graphics | Input | Validation |
| --- | --- | --- | --- | --- |
| macOS | AppKit, Objective-C++ | NSOpenGLContext, legacy compatibility profile | NSEvent plus CoreGraphics cursor capture | Built and run on Apple M2 Max |
| Linux | Xlib, X11 window-manager protocols | GLX compatibility context | X events, pointer grab and relative motion | Built and run on spacegoo under Xvfb/Mesa llvmpipe |
| Windows | Win32, per-HWND message dispatch | WGL | Win32 keys/buttons and Raw Input mouse motion | x86-64 MinGW application and platform test cross-compiled and linked; not run |

Box2D, the historical allocator/Lua code, and OpenAL are existing engine dependencies. No replacement window toolkit has been introduced. Linux follows ICE's original X11 design and can run through XWayland; a direct Wayland backend has not been implemented.

## Interfaces and ownership

- `icWindow::GetWindow(def, parent)` still chooses the native subclass. Failed native creation returns null.
- `icWindow::PumpEvents()` drains application-wide native events on the UI thread. It does not choose a renderer or update the game.
- `ShouldClose()` and `RequestClose()` belong to each window. Closing one preview does not terminate the whole application. Application quit requests close all registered windows.
- `GetClientSize()` and `GetFramebufferSize()` provide explicit sizing. The Mac default deliberately retains the legacy one-render-pixel-per-view-point behavior; Retina backing is not enabled yet.
- Each GL device/window pair has its own context. Frame begin makes that context current. Device cleanup releases it; Mac/Linux window cleanup also provides a fallback if the window is destroyed first.
- Parent pointers express association, not C++ ownership. Callers own each window and should destroy graphics devices before their windows.
- Mac construction, native window operations and event pumping belong on the main thread. Windows/X11 use the thread owning their native windows and display.

Platform-specific access remains available. Mac `GetRef()`/`GetView()` return borrowed NSWindow/NSView pointers for use from Objective-C++. Linux exposes Display and X Window handles. Windows exposes HWND, HDC, the borrowed HGLRC and its virtual WindowProc. Mac and Linux also have a `HandleNativeEvent` hook that can consume an event before normal dispatch. These permit native appearance, notifications, window-manager properties, DWM/taskbar integration and other OS-specific work without waiting for a common abstraction to support it. DWM thumbnail handlers themselves have not been implemented as part of this port.

The Mac implementation uses Apple's [NSOpenGLContext API](https://developer.apple.com/documentation/appkit/nsopenglcontext?language=objc) and [AppKit event pump](https://developer.apple.com/documentation/appkit/nsapplication/nextevent%28matching%3Auntil%3Ainmode%3Adequeue%3A%29?language=objc). Carbon/AGL have been replaced rather than emulated.

## Build and test

macOS needs CMake 3.20+ and Apple's developer tools:

```sh
cmake -S . -B build/macos -DCMAKE_BUILD_TYPE=Debug
cmake --build build/macos -j6
ctest --test-dir build/macos --output-on-failure
build/macos/bin/ice_platform_smoke
open build/macos/bin/Airhockey.app
```

Linux needs a C/C++ compiler, CMake, X11 development headers, OpenGL/GLU development libraries and OpenAL development headers/libraries. The font implementation uses X's native core fonts, with a fixed-font fallback. On Debian/Ubuntu the relevant development packages are `libx11-dev libgl-dev libglu1-mesa-dev libopenal-dev`.

```sh
cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Debug
cmake --build build/linux -j8
ctest --test-dir build/linux --output-on-failure
build/linux/bin/ice_platform_smoke
mkdir -p /tmp/ice-smoke
build/linux/bin/Airhockey --smoke-test /tmp/ice-smoke
build/linux/bin/Airhockey
```

The spacegoo checks ran in an isolated test checkout under `/tmp/ice-native.pnGE8W`. Missing packages and Xvfb were extracted there without changing installed system packages. The virtual display was removed after testing. Audio used OpenAL's null driver, so audible output and physical controls were not tested on that host.

Windows needs a Windows compiler and an OpenAL SDK/runtime. CMake's OpenAL discovery accepts explicit `OPENAL_INCLUDE_DIR` and `OPENAL_LIBRARY` paths. The current CMake profile uses OpenGL rather than requiring the old DirectX SDK.

```powershell
cmake -S . -B build/windows -DCMAKE_BUILD_TYPE=Debug -DOPENAL_INCLUDE_DIR=C:/OpenAL/include -DOPENAL_LIBRARY=C:/OpenAL/lib/OpenAL32.lib
cmake --build build/windows --config Debug
build/windows/bin/Debug/ice_platform_smoke.exe
build/windows/bin/Debug/Airhockey.exe
```

Single-configuration generators omit `Debug/` in those paths. `OpenAL32.dll` must be available at runtime, along with any compiler runtime DLLs required by the chosen toolchain. Windows requires an OpenGL driver supporting vertex buffer objects; the unsupported legacy CPU-buffer path does not deliberately crash anymore.

The Windows validation used a staged MinGW compiler on spacegoo and an import library generated from the public OpenAL DLL entry points. This verifies compilation and linking, not the availability of the actual OpenAL DLL, Windows runtime behavior, DPI handling or DWM integration.

The core tests cover file/settings/timing/input and state-pop behavior. Native platform tests cover independent windows and contexts, pixel readback, owner association, resize/fullscreen restoration, key events, preview close and cleanup. The Airhockey smoke test covers scoring, match reset, pause/resume, advancing physics, GL errors and nonempty menu/game/pause captures. Windows test executables were built but could not be executed.

## Proposed next design changes

Preserve the small common interface and keep rich native extension points. The next useful changes would be:

1. Add a window ID to events and provide input state per window, while retaining the singleton as a compatibility view of the focused window. This matters for several interactive previews.
2. Replace short dimensions/positions and the platform-specific 32-bit flag field with explicit logical sizes, framebuffer sizes and typed native creation options. Keep the current definitions as a compatibility adapter.
3. Separate graphics surfaces/context ownership from window ownership, so a window can host OpenGL today and Metal, Vulkan or another renderer later.
4. Add a dedicated application lifecycle object for menus, native quit requests and window registration. The current static event pump is intentionally a small bridge to the original game loop.

These are proposals, not changes imposed on the existing engine API in this pass.
