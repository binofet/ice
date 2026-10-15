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

## Native text and interpreted editing

Enable `desktop_window::enable_text_input()` for committed UTF-8, marked/preedit text
and interpreted edit commands. Set `set_text_input_rect()` with a logical top-left
caret rectangle for candidate placement. `platform_events` owns bounded text payloads
and routes by window ID; native key codes are never synthesized into printable text.
Composition-owned arrows/Return must not become application history/accept commands.

AppKit uses `NSTextInputClient`; Win32 uses Unicode messages and IMM composition;
X11 uses XIM/XIC with UTF-8 lookup and preedit callbacks when offered by the input
method. X11 may use IM-owned preedit UI or an explicit ASCII native-lookup fallback.
Candidate placement is best-effort and reports unsupported input-method capability.
On first X11 initialization, a C/POSIX `LC_CTYPE` follows the environment for XIM;
other locale categories, including numeric parsing, stay unchanged.

Focus loss, hide, disable, close/retire and overflow cancel pending composition rather
than commit it. The stream permits 1024 events, 256 KiB total text and 4096 bytes per
text event. Overflow discards the batch and emits application-scope input cancellation.
Consumers should clear marked text for their window or application-wide cancellation.
Clipboard shortcuts are actions only; clipboard ownership/transfers are not implemented.
These adapters provide input/preedit, not a rich-editor surrounding-document or reconversion
model. Pointer/capture and physical held-key/device routing into the shared host are
subsequent work; mobile probes already feed touch/sensor devices into that host.

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
10 SDK with user32/gdi32/imm32 on Windows. Enabling the separate OpenGL renderer also requires
the system OpenGL development libraries; there is no old DirectX SDK dependency.
Visual Studio builds place multi-configuration executables under the
configuration directory. Windows runtime/DPI/fullscreen behavior needs hardware
validation; cross-compilation alone cannot establish it.

## Renderer attachment and replacement

`ice::render::device` selects Metal or OpenGL explicitly. A presentation surface borrows
its native window; pipelines and surfaces independently retain their backend device. Sampled image owners do as well.
The common API grants one renderer attachment per logical window and rejects pipelines
from another device. Keep simulation state and authored resource descriptions outside
the GPU objects so a replacement can reconstruct them.

The tested handoff prepares a candidate shader through a URI, renders a detached probe,
drains the old surface, detaches it and attaches the candidate to the same window. Keep
the prior device/pipeline until attachment succeeds so the application can restore them
on failure. This sequence is currently application code, not a general scene migration
service. Preparation and draining are synchronous. DLL unloading, general mesh/material resource
migration and recovery from arbitrary driver hangs are not implemented.

Metal creates a CAMetalLayer for desktop windows, or borrows an existing layer through
the platform-surface adapter for an Apple mobile host. OpenGL uses native core-profile
contexts: NSOpenGL on macOS, GLX on Linux and WGL on Windows. No GLFW or fixed-function
drawing is involved. The Quest probe uses a separate OpenGL ES/OpenXR bootstrap; it
does not imply the desktop GL backend runs on mobile.

X11 display-server failure is not crash-contained. Native window callbacks and
in-process C++ remain trusted engine code.

## Sampled image foundation

The procedural pipeline accepts one normalized float `texture2d`/`sampler2D` at slot
zero in either shader stage. RGBA8 uploads and captures are top-row-first; UV y=0
samples that first uploaded row. Sampling is linear, clamped and has no mipmaps.
Dimensions are in [1,4096], with exact tightly packed byte counts. A same-size update
prepares fresh native storage before publishing it, preserving preceding submissions;
resize requires a replacement owner. Foreign-device/missing required images fail before
encoding. This is enough for a native text canvas/CRT shader, not a complete material,
texture-array, streaming or general resource-binding system.
