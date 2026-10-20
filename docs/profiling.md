# Optional CPU profiling

`ICE_ENABLE_PROFILING` defaults to `OFF`. The disabled facade erases macro arguments and links
no Tracy client. Existing Lua, asset and render behavior remains governed by the original
contracts. Enabling profiling adds CPU timing and telemetry; it does not add GPU timestamps.

The backend is pinned to Tracy 0.14.1 at
`30997d5ca6bb632cc10807a1da8a6d3de0aeeb3c`. The dependency and license identities are recorded in
[Tracy.lock.json](../cmake/Tracy.lock.json). Supply a pristine local Git checkout through
`ICE_TRACY_SOURCE_DIR`. Configuration rejects missing, dirty or differently pinned source,
and rejects an already defined Tracy client target. ICE never fetches Tracy automatically.

```sh
cmake -S . -B build/profile \
  -DICE_ENABLE_PROFILING=ON \
  -DICE_TRACY_SOURCE_DIR=/absolute/path/to/pinned-tracy
```

Compilation alone leaves the real client inactive. A host must explicitly create
`ice::profiling::capture_session{true}` before instrumented resources or workers are created.
It must destroy those resources and join their workers before destroying the session. Only
one serial host owner may start or stop a session. This is a quiescent lifetime boundary,
not a concurrent runtime toggle. The enabled client uses manual lifetime, localhost only,
no broadcast and on-demand zones. Callstacks, source transfer, system tracing, sampling,
vsync capture, frame images and crash hooks are disabled for this initial profile.
Optional Fortran/fiber support, GPU auto-calibration and upstream ROCm profiler detection
are also disabled, including when a reused CMake cache requested them.

`ICE_PROFILE_ZONE` uses a static label and source location. Values attach existing numeric
identifiers; bounded text annotations borrow existing strings and the backend copies them.
The application does not format object labels or maintain another object/asset registry.
Text over 4096 bytes is omitted rather than truncated. Integer plots omit values above
`INT64_MAX`; numeric zone values retain the full unsigned 64-bit range.

Lua prepare/call zones enclose protected calls from their C++ caller. The host-invoke zone
encloses the C++ helper that returns before the C trampoline may call `luaL_error`. Do not
put an RAII zone in a Lua C frame that can be crossed by `longjmp`. Allocator accounting is
noexcept and has no scope destructor. Program trace IDs attach to existing canonical source
URIs on the first captured preparation or callback, including a program prepared before a
collector connects. The URI association is retained by the capture backend, not an app catalog.

The Lua allocator plots describe that custom allocator alone: live and peak allocated bytes,
including retained allocation headers; allocation/reallocation attempts; frees; quota and
native allocation failures; and failed shrinks that retain their original capacity. Failed
growth changes no live-byte count. A retained shrink changes no live-byte count. These are
atomic counters sampled at frame boundaries; simultaneous multi-field reads are approximate.
They do not report process-wide heap allocations or GPU residency.
When profiling is compiled in, allocator counters are maintained even before a session starts,
so an eventual capture includes existing Lua allocations. Runtime inactivity does not erase
that accounting cost; compile-time `OFF` erases it.

RHI zones cover shader preparation, mesh/texture upload calls and surface submission on the
CPU. Byte plots describe requested payloads and validated draw counts. They do not establish
successful GPU allocation, completed GPU work or resident storage. The first cut adds no
per-object render-loop zones or native backend timestamp API.

Enabled CMake targets embed configure-time source revision, clean/dirty state, language
standard and build configuration. The host can use `ice_profiling_source_identity` to add
the same fields to its own target. Reconfigure after any source edit. A qualified capture
must use a clean, sealed engine/host source pair and retain the executable hash and build
configuration; a dirty label identifies a development build, not a qualified source receipt.
Cold application metadata records the Tracy pin and those source/build fields. Shader and
session identity must come from the host's existing receipts.

`tests/profiling` has disabled, private recording, and real-client-inactive modes. The
recording harness exercises zone lifetime, copied annotations, on-demand URI association,
host exceptions and Lua quota/allocator accounting without a socket or collector. The real
mode only verifies that the pinned client stays unstarted while actual Lua regressions run.
These CPU checks establish no capture, FPS, overhead or device result. Native captures,
enabled/disabled overhead measurement and GPU timestamp acceptance remain separate work.
