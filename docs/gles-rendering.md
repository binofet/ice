# Reusable GLES and borrowed native images

`ICE_BUILD_GLES=ON` selects the EGL-backed embedded profile through
`device::create(backend_kind::opengles)`. It reuses the same GL resource ownership,
reflection, indexed batches, textures and uniform encoder as desktop OpenGL.
The option is off by default and currently supports Android and Linux EGL SDKs.
There is no mobile desktop-window emulation or implicit native buffer swap.

The baseline is GLES3.0, authored GLSL300es with high-precision float/int/sampler2D
prefixes, uint32 indices,24/32/64-byte vertex layouts, four explicit sampled slots,
16KiB std140 blocks, depth testing and premultiplied blending. Shader sources
still own their explicit zero-to-one clip conversion (`gl_Position.z=2*z-w`).
RGBA8 capture/upload and owned RGBA8 targets retain the existing top-origin
contract. Float color targets are capability/allocation checked; ES3.0 alone does
not guarantee renderable RGBA16F. Unsupported allocations return diagnostics.

## Native adapter contract

`egl_context()` activates the device-owned EGL root on the calling render thread
and returns borrowed display/config/context handles plus a unique context-lifetime
identity. A native XR session must bind this exact context. Handles are read-only
ownership references: callers never destroy them. Separate logical GLES devices
own separate contexts, while their EGL display is retained until the last context
or prepared-resource owner retires.

`import_destination(gl::destination_description)` validates an existing,
single-sample, single-layer `GL_TEXTURE_2D` image, exact dimensions/internal format,
context identity and framebuffer completeness. Exact texture-level interrogation
requires GLES3.1 or newer; a GLES3.0-only device reports unsupported import.
One native color name has one live wrapper/lease owner per device; duplicate
imports reject until that wrapper retires. The wrapper owns only its cached FBO and local Depth24 storage. It never deletes the
runtime color image. No OpenXR SDK handles enter this API.

The native adapter remains responsible for runtime acquisition/wait/release:

1. Acquire an image natively, then call `begin_acquisition(generation)` on its
   wrapper with a strictly increasing nonzero generation.
2. Wait natively. A timeout retains the **same image and unwaited lease**, for a
   later wait. It must not start another acquisition or release an unwaited image.
3. Only after native wait success, call `lease.mark_waited()`.
4. Call `render_into(destination, lease, batch)`. The complete batch, logical
   device/context, destination/generation, waited/live state and feedback are
   checked before its clear. Invalid requests preserve the image.
5. Explicitly call `device.flush()` before handing the image back to the runtime.
   This submits GL work without CPU/GPU completion waiting.
6. Expire the lease **before native release**, on every success/failure exit path.
   Lease destruction also expires it, but never performs a native release.

This lease is trusted host attestation. It cannot authenticate arbitrary native
XR calls, infer a wait result or prevent a dishonest adapter from releasing an
image early. All operations are render-thread only; they are not a cross-thread
synchronization mechanism.

Retire all destination wrappers before native swapchain image destruction, and
retire the native session/swapchains before the final EGL context/resource owner.
Wrapper destruction invalidates outstanding leases and deletes FBO/local depth
only. There is no per-eye `glFinish`, CPU readback, resolve flip or swap in this
path. Owned preparation/tests/shutdown retain their existing explicit synchronous
drains. A native driver failure after encoding does not promise pixel rollback.

## Color and scope

Imported `rgba8_srgb` destinations require linear shader RGB; GLES performs the
sRGB attachment conversion once. Existing `rgba8_unorm` retains authored output
encoding. Pipeline destination formats must match exactly. This first sRGB format
is for imported EGL images: owned-target/capture operations reject it, and Metal
pipeline creation returns an unsupported diagnostic rather than mapping it to
UNORM. Imported output is a native attachment, not a new sampled texture view.

This is a reusable renderer/adapter foundation. The optional Android XR host in
`examples/xr/android/Host.cmake` now reuses it for the lab and application clients,
retaining acquire/timeout/flush/release, anchor-change and lifecycle ownership.
That extraction is compile/link checked; its physical Quest regression remains a
separate acceptance gate. Multiview/MSAA and runtime composition depth are not
implemented. A physics-free
saved scene consumer proves a presentation boundary; it is not a restriction
against future client prediction, controllers or reconciliation.

## Validation

The CPU lease regression covers timeout retention, stale/foreign generations,
move ownership, expiry and destination lifetime. The GLES executable exercises
real EGL indexed24/32/64-byte draws, a16KiB block, four texture slots, depth,
top-origin row packing and imported-image ownership/format/context/feedback/sRGB
checks. Run it only where an EGL ES runtime is available; an Android cross-build
alone is not native GPU or Quest hardware proof. Linux software Mesa is suitable
for an explicit offscreen execution gate, with its renderer/version reported.
