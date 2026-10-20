# Asynchronous offscreen readback

`device::render_offscreen_async(batch, width, height, readback_limits)` encodes a
separate RGBA8 offscreen composition on the creating render thread. It returns
`expected<readback_submission, diagnostic>`: `submitted` has a move-only ticket;
`busy` has an empty ticket and submits nothing. Dimensions remain 1..4096.
The caller supplies explicit per-device ticket and byte admission limits (zero
turns admission off). All requests should use the same host-approved limits.

Each reservation conservatively charges logical peak color/depth storage,
staging and the future CPU copy. The charge remains through GPU completion and
until extracted CPU pixels are destroyed, including on a writer thread. It is a
logical bound, not a measurement of native driver memory/compression overhead.
Pixels and tickets are move-only. The host separately bounds its nonpixel queue,
encoding scratch and file-writer work. There is no built-in shipping budget.

Poll on the render thread with `ticket.poll()`. A native failure is an unexpected
diagnostic; successful states are `pending`, `ready`, `cancelled`, `consumed`.
When ready, `ticket.take()` returns owned `readback_pixels` exactly once. Taking a
pending/empty/cancelled/consumed ticket diagnoses without consuming another job.
Pixels describe width/height, row_stride, RGBA8/BGRA8 layout and top_left/bottom_left
row origin. Their vector includes explicit row padding. No mapped buffer escapes.
The worker may normalize channels/rows, encode and save; it calls no RHI methods.

All native methods, ticket movement and destruction use the creating render
thread. Submission consumes borrowed batch/resources synchronously; native GPU
resources are retained through completion, independently of caller draw/uniform
storage. Metal uses a retained texture/shared buffer and completion handler.
GL/GLES uses a PBO, fence and zero-timeout ClientWaitSync; only a signalled transfer
may be mapped/copied on its owning context. Missing asynchronous capabilities
return an unavailable diagnostic, with no synchronous readback fallback. Driver
calls and the eventual bounded CPU copy still have cost; no fixed latency is
promised. The existing synchronous render_offscreen API remains unchanged.

Cancellation discards the result; it does not free storage still used by the GPU.
Keep polling a cancelled ticket until `cancelled` or a terminal native diagnostic,
then retire it on the render thread. A dropped pending Metal ticket is retained by
its completion handler; GL retains it in the device's bounded retirement list,
reaped during asynchronous submit/poll. Failed/unsignalled GL fences remain
charged until a later signal or the existing final device teardown drain. Keep
the device alive until tickets retire. Destroying the final native owner can
perform the pre-existing synchronous device shutdown drain; it is not a live
capture/Take Control cancellation primitive.

The host owns identity-keyed job completion, authorization, frame/session/world/
revision/tick/view/device metadata and optional persistence. Freeze that metadata
at encoding, not when disk writing finishes. Include the full world/debug/ID/UI
composition in the submitted batch, and label this as a separate offscreen view,
not a pixel-identical copy of the presented drawable. This API owns no window,
input/agent lease, settling timer, command result or screenshot-file policy.

CPU fixtures cover ticket ownership/admission/cancellation/failure and writer
lifetime without creating any graphics device. Actual Metal/GL/GLES readbacks,
packing/row identity, device teardown under pending work and performance require
separate GPU/device qualification; compilation and CPU tests do not prove them.

The manual `ice_async_readback_gpu_tests` target reuses the renderer test setup
and `tests/data/mesh/probe.metal` / `probe.glsl` shaders. It is intentionally not
registered with headless CTest. With a separately allocated build and GPU window,
invoke one backend per process:

```
<build>/bin/ice_async_readback_gpu_tests metal <source>/tests/data/mesh <new-output-directory>
<build>/bin/ice_async_readback_gpu_tests gl <source>/tests/data/mesh <another-new-output-directory>
```

Each process submits five 63x31 offscreen frames. Two indexed quads produce a red
upper-left region and cyan-blue lower-right region on an asymmetric clear color.
An analytic interior-pixel oracle checks the actual bytes, backend channel order,
row origin and stride (Metal BGRA/top/256 bytes, desktop GL RGBA/bottom/252 bytes).
Padding values are unspecified and excluded from the oracle. The first payload
is moved to a CPU writer, which rechecks it, saves native bytes plus a normalized
PPM and packing record, and retains admission until its thread releases the owner.
Ticket-count and byte pressure, zero bounds, invalid dimensions/batch diagnostics,
single take, logical cancellation, drop and eventual readmission are checked.
The writer owns only pixels and fixture file output; it calls no device methods.

Pending observations and busy counts after dropping are recorded, never forced.
A fast GPU can complete before the first poll. A successful process with zero
pending observations does **not** qualify native pending-to-ready behavior; a
cancel/drop case without a pending observation does **not** qualify that pending
path. Even with observations, these checks do not instrument GPU resource release
or prove every lifetime interleaving. Native lifetime/admission source review and
additional targeted acceptance remain necessary. No repeated/larger campaign is
implicit in a missing observation. This fixture makes no performance claim.

An unavailable backend/capability or unexpected native error fails with its real
diagnostic; no skip, capability injection, GPU hang/device-loss injection or
synchronous readback fallback is used. A capable run qualifies only its observed
validation errors, not absent capability branches, allocation/map/command faults,
device loss or failure cleanup. GLES/EGL and other platforms need their own
target/device acceptance. Full app composition, presentation identity, resize,
device teardown during pending work and app scheduling remain separate gates.
Fixture polling uses ten-second local deadlines; an external process timeout is
still required because a driver/filesystem call can block, including during final
native-device teardown. Preserve partial output and the first actual failure.
