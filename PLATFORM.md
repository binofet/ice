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

The window module requires X11 development headers/libraries on Linux and the Windows
10 SDK with user32/gdi32 on Windows. Enabling the separate OpenGL renderer also requires
the system OpenGL development libraries; there is no old DirectX SDK dependency.
Visual Studio builds place multi-configuration executables under the
configuration directory. Windows runtime/DPI/fullscreen behavior needs hardware
validation; cross-compilation alone cannot establish it.

## Renderer attachment and replacement

`ice::render::device` selects Metal or OpenGL explicitly. A presentation surface borrows
its native window; pipelines and surfaces independently retain their backend device.
The common API grants one renderer attachment per logical window and rejects pipelines
from another device. Keep simulation state and authored resource descriptions outside
the GPU objects so a replacement can reconstruct them.

The tested handoff prepares a candidate shader through a URI, renders a detached probe,
drains the old surface, detaches it and attaches the candidate to the same window. Keep
the prior device/pipeline until attachment succeeds so the application can restore them
on failure. This sequence is currently application code, not a general scene migration
service. Preparation and draining are synchronous. DLL unloading, mesh/texture resource
migration and recovery from arbitrary driver hangs are not implemented.

Metal creates a CAMetalLayer for desktop windows, or borrows an existing layer through
the platform-surface adapter for an Apple mobile host. OpenGL uses native core-profile
contexts: NSOpenGL on macOS, GLX on Linux and WGL on Windows. No GLFW or fixed-function
drawing is involved. The Quest probe uses a separate OpenGL ES/OpenXR bootstrap; it
does not imply the desktop GL backend runs on mobile.

X11 display-server failure is not crash-contained. Native window callbacks and
in-process C++ remain trusted engine code.
