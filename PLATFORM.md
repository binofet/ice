# Native platform and window ownership

`ice::platform::host` owns application activity, borrowed surface registrations,
input devices and frame scheduling. It has no mandatory OS loop or root window.
`desktop_window` separately owns native desktop windows; it creates no GPU device,
context or swapchain. A renderer attaches presentation endpoints explicitly.

| Platform | Native endpoint | Present implementation |
| --- | --- | --- |
| macOS | AppKit NSWindow / layer-backed NSView | Native lifecycle, focus, resize/backing size and fullscreen requests |
| Windows | Win32 HWND | Native lifecycle, focus, resize/DPI and monitor fullscreen requests |
| Linux | Xlib Display / Window | Native lifecycle, focus, resize and EWMH fullscreen requests |

All window operations, pumping and destruction belong on the UI thread. Pump once
for the process: windows have independent process-local routing IDs, and ID zero
means application scope. Closing a window requests closure without destroying it.
Quit requests close all registered windows. Destroy presentation resources before
their window; borrowed OS handles are valid only while that owner/native window lives.

The bounded `platform_events` stream is a notification queue, not an event broadcast
bus. A host should consume it once and route events. `should_close()` remains
authoritative if notifications overflow. `dropped_events()` reports lost notifications;
query `extent()` for current size. There is no hidden window used as a global hook.
OS-wide hotkeys or input monitoring would be explicit additional integrations.

Logical client dimensions and drawable pixels are separate. AppKit reports Retina
backing dimensions; Win32 uses per-monitor DPI. The initial X11 implementation reports
scale 1; native Wayland and Linux fractional scaling remain work. Fullscreen requests
may complete asynchronously or be declined by the desktop environment.

The new desktop window API currently reports lifecycle/focus/resize/close. Native
keyboard, text/IME, pointer, capture and device routing into the shared host are the
next input work. Mobile probes already feed touch and sensor devices into that host.
Do not infer desktop input support from the retired platform implementations.

## Validation

```sh
cmake -S . -B build/desktop -DICE_CXX_STANDARD=23
cmake --build build/desktop --parallel 4
ctest --test-dir build/desktop --output-on-failure
build/desktop/bin/ice_window_smoke
# On a headless Linux test machine:
xvfb-run -a build/desktop/bin/ice_window_smoke
```

Linux desktop configuration requires X11 development headers/libraries. Windows
uses the Windows 10 SDK APIs and user32/gdi32; no OpenAL, OpenGL or old DirectX SDK
is needed. Visual Studio builds place multi-configuration executables under the
configuration directory. Windows runtime/DPI/fullscreen behavior needs hardware
validation; cross-compilation alone cannot establish it.

X11 display-server failure is not crash-contained. Native window callbacks and
in-process C++ remain trusted engine code. Renderer loss/replacement will have a
separate state/resource contract; no modern GPU backend is implemented here yet.
