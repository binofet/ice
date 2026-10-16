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
These adapters provide insertion/preedit, without a rich-editor surrounding-document or
reconversion model. Text-only cancellation carries `input_cancellation::text`: disabling
text while navigating does not release physical keys or pointer capture.

## Physical controls and relative pointer capture

The same per-window stream carries `physical_key` down/up, `pointer_button`, `pointer_move`
and `scroll`. Physical keys identify keyboard positions using USB-HID-style values; `w`
means the US-QWERTY W position even when another layout produces a different character.
Use committed UTF-8 for text. Both paths remain independent, and physical controls work
with text input disabled. AppKit maps native key positions, Win32 maps scan codes and
X11 maps XKB key names; unavailable positions remain `unknown`, with no character-based
guess. Keyboard-repeat events are marked, and key-up is never a repeat.

Pointer positions use logical client units from the top-left. Relative deltas are positive
right/down; the first normal-position sample after cancellation starts with zero delta.
`delta_units` distinguishes logical displacement from Win32 raw device counts, so camera
sensitivity must account for that distinction. Scroll intent is positive right/up;
`precise_scroll` distinguishes precise logical UI units from nominal wheel steps.

Request `window.set_pointer_mode(pointer_mode::relative)` only for a focused, visible
window and handle its diagnostic on failure. The request never steals focus. Query
`relative_pointer_capability()` for the available path and `pointer_mode()` for effective
state. Relative capture belongs to that window; there is no root-window hook or global
input monitor. The native implementations are:

| Platform | Relative path | Reported delta units |
| --- | --- | --- |
| AppKit | Cursor/mouse disassociation with balanced cursor hiding, native event deltas | Logical; not a promise of unaccelerated counts |
| Win32 | Focused-window raw mouse input, cursor confinement and capture | Device counts |
| X11 | Explicit confined pointer-grab/center-warp fallback, synthetic warp events suppressed | Logical; not XI2 raw input |

Focus loss, hide, minimize, close, destruction, all-input cancellation and queue overflow
release capture and reset native held/repeat state. Win32 move/resize and X11 geometry
changes also release capture rather than retain stale confinement/warp coordinates.
Returning to a window never captures automatically. `cancel_input()` releases all input;
`cancel_text_input()` only discards composition. Consumers must reset their held controls
for `input_cancellation::all`, including application-scope cancellation, and preserve them
for text-only cancellation. No synthetic key-up sequence is promised after cancellation.

All capture requests, queries and event pumping belong on the UI thread. Posting an
invalid/overflowing event marks cancellation under the queue lock; native cursor operations
are deferred to UI pumping or the effective mode query. This also releases capture if an
application fails to poll the cancellation notification. Pointer payloads must be finite
and have absolute values no greater than one billion; physical events need a nonzero
window, a defined key/button identity and a valid displacement-unit tag.

The shared host's `device_input` reuses optional typed `physical_key`/`mouse_button`
fields alongside existing contact codes. This does not create another polling system or
automatically bridge desktop events into a host device. Application input policy owns that
routing; existing mobile probes continue feeding their own touch/sensor devices.
The host distinguishes held inputs by device, surface, kind, contact, physical key and mouse
button together. Distinct typed controls may use contact zero; a release/change must preserve
the matching typed identity. The default `unknown`/`none` fields preserve legacy contacts.

## Validation

```sh
cmake -S . -B build/desktop -DICE_CXX_STANDARD=23
cmake --build build/desktop --parallel 4
ctest --test-dir build/desktop --output-on-failure
build/desktop/bin/ice_window_smoke
# On a headless Linux test machine:
xvfb-run -a build/desktop/bin/ice_window_smoke
# Native X11 key/text and capture regression; use its own display server:
xvfb-run -a build/desktop/ice_platform_input_x11_tests
# Native AppKit physical-input regression via LaunchServices:
open -n -W build/desktop/ice_platform_physical_macos_tests.app
```

The physical-input CPU regression exercises typed mappings, text-only versus all-input
cancellation, finite payload rejection, window routing and bounded-queue capture release.
The AppKit fixture uses only its own windows, native queued keys and responder events; it
checks independent physical/text delivery and focus/overflow/hide/close capture release.
Run it through LaunchServices so its application can activate. Exit 77 means native focus
or capture was unavailable and is a skip, not a passing hardware result. The isolated X11
fixture temporarily remaps its server's physical W position to produce text `z`, restores
the mapping, and exercises real focus, pointer warps and capture release on hide/resize.

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
