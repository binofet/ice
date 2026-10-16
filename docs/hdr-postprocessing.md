# GPU HDR targets and ordered postprocessing

`ice::render` supports linear RGBA16F render targets on Metal and OpenGL4.1.
Scene rendering, threshold/downsample, ping-pong blur and final tone mapping stay
on the GPU. The application owns the graph, sizes, shader packages and frame
boundary; the engine supplies resources, explicit bindings, pass ordering and
checked ownership. This is an SDR presentation path with HDR intermediates, not
an HDR monitor/output-transfer-function implementation.

## Targets, formats and views

```cpp
using namespace ice::render;
auto scene = gpu.create_render_target({width, height, color_format::rgba16_float});
auto bloom = gpu.create_render_target({width / 2, height / 2});
// Check every allocation before publishing this new resource graph.
pipeline_options scene_options;
scene_options.target_format = color_format::rgba16_float;
scene_options.sampled_bindings = 0; // No sampled inputs, explicitly declared.
scene_options.depth_test = scene_options.depth_write = true;
```

Dimensions are immutable and in `[1,16384]`, bounded further by native texture/
framebuffer capacity and allocation success. GL queries its limits; Metal checks
its GPU-family capacity ([Apple feature tables](https://developer.apple.com/metal/capabilities/)).
CPU RGBA8 upload/capture keeps its separate 4096 bound; a 6144×2560 display can use
native-size GPU targets without a full-size CPU capture. Each target owns linear color plus
Depth32Float. `rgba16_float` retains negative and greater-than-one finite shader
radiance; no engine tone map, gamma conversion or clamping is inserted. The
shader author remains responsible for finite output and half-float range. The
existing normalized clear color and depth-clear-one contract is unchanged.
`rgba8_unorm` is also available. `pipeline_options.target_format` must match the
render destination; native presentation and RGBA8 captures stay `rgba8_unorm`.
Metal's matching UNORM attachment is BGRA8 internally, preserving logical RGBA
channels and the existing surface format.

`target.color()` and `target.depth()` return borrowed `const texture&` views. They
remain valid only while that target owner exists, and become initialized after
its first successful `render_into` submission. Sampling a fresh uninitialized
view is rejected. Do not retain the reference across destruction or graph swap.
Targets and their native storage retain device lifetime independently of the
C++ device owner. Submitted commands retain storage even if its owner is later
released; this does not make a destroyed C++ view safe to dereference.

Resize by allocating a complete candidate graph, then swapping ownership at the
application's frame boundary after successful preparation. A failed allocation
leaves the previous graph intact. Window zero-size/suspension skips rendering;
zero-sized targets are rejected. A renderer replacement recreates targets and
pipelines from retained descriptions/source on its new logical device; old
views cannot migrate even when the physical GPU is the same.

## Explicit slots and sampling

`draw_item.sampled_images` supplies up to four borrowed texture pointers, at
slots 0–3. The previous `sampled_image` field still supplies slot 0 when array slot 0
is null. Supplying two different slot 0 pointers is rejected; the same pointer in
both fields is allowed. Uploaded RGBA8 images and target views use the same draw
boundary. Foreign-device images are rejected, including unused supplied slots.

New pipelines set `pipeline_options.sampled_bindings` to the exact bit mask of
shader-used slots (`0..15`). Preparation rejects a mismatch with shader
reflection. Its default `0xff` preserves the previous auto-reflected zero-or-one
slot 0 API; it does not infer a multi-texture material contract. Metal uses
`[[texture(0)]]` through `[[texture(3)]]`; GLSL multi-slot samplers are named
`ice_texture0` through `ice_texture3`, and the backend assigns their texture
units. A sole slot 0 sampler can retain its old arbitrary name. Uniform bytes
remain at read-only buffer 0 / one std140 block, bounded to 16384 bytes per draw.

`depth_sampled_bindings` is a subset of the explicit used-slot mask. Those slots
require target depth views; other used slots require color views. Depth sampling
returns raw zero-to-one floats without a comparison sampler. GLSL uses
`sampler2D`; Metal uses `depth2d<float>`. Both shader stages may sample the views.
The color sampling contract is bilinear/clamp-to-edge and the depth contract is
nearest/clamp-to-edge. GL texture state enforces these settings; authored Metal
constexpr samplers must use the corresponding filter/address settings, as with
the existing image path. Texture arrays, comparison samplers and mipmaps are not
part of this profile.

All uploaded and GPU-produced views use top-origin UVs: `(0,0)` is the top-left.
Metal stores the native target directly. GL renders into native attachments and
performs a GPU-only vertical color/depth framebuffer blit into canonical sampled
storage. This adds one resolve pass, a second color/depth allocation and bandwidth
on GL. It avoids shader rewriting and inconsistent coordinates; an internal
optimization can replace that resolve later without changing authored shaders.
Scissor is disabled for the resolve, and read/draw framebuffer bindings return
to zero afterward. Each later pass sets its own viewport and render state.

## Pass ordering and validation

```cpp
draw_item scene_draw{scene_pipeline, mesh, uniforms};
frame_batch scene_pass{std::span(&scene_draw, 1)};
auto submitted = gpu.render_into(*scene, scene_pass);
// Check submitted status before consuming this frame's result.
draw_item composite{composite_pipeline, nullptr, composite_uniforms};
composite.sampled_images[0] = &scene->color();
composite.sampled_images[1] = &bloom->color();
auto presented = surface.draw_batch(frame_batch{std::span(&composite, 1)});
```

Each pass validates all draws before encoding: format, logical device, uniforms,
required slots, vertex layout, initialized views, color/depth kind and feedback.
A target cannot sample its own color OR depth in that pass, including an alias of
the same native storage. Use distinct ping-pong targets for blur. Checked errors
preserve the target's prior contents; empty passes clear it. Native driver or
shader faults do not promise rollback or process isolation. The whole graph is
not one atomic transaction: an invalid later pass does not undo earlier valid
passes. The application prepares/validates graph candidates before publication.

Calls run synchronously on the owning render/UI thread for validation and
encoding, while execution stays asynchronous. Metal uses one command queue and
at most 64 outstanding target command buffers; `busy` means no new work was
submitted. GL chains shared-context GPU fences/server waits, flushing producers
before another context consumes their output. There is no per-pass `glFinish`,
CPU readback or GPU-completion wait in this live path. Drivers can still stall API
calls; these interfaces do not guarantee a hard real-time deadline.

`device::wait_idle()` explicitly drains preceding work for preparation, tests,
handoff or shutdown. It is synchronous and can pause all device work, including
native presentation. It returns available native failure diagnostics. Keep it
out of the runtime bloom chain. Existing surface drains remain available.

`live_pipeline::prepare` accepts an appended four-slot array and retains old call
compatibility. A candidate uses the requested format and actual bindings: RGBA8
keeps its existing capture probe; HDR creates a detached matching 63×31 target,
submits its probe and drains the device before staging. Failure preserves the
active revision, staged valid candidate and undo owner. This preparation is
intentionally synchronous today; candidate fences/job scheduling are future
work. Activation and undo remain explicit caller frame-boundary operations.

## Verification and limits

`ice_hdr_tests tests/data/hdr` requires a native GPU and runs outside CPU CTest.
The fixture renders asymmetric linear radiance up to 8 and raw depths .25/.75,
then runs threshold/downsample and horizontal/vertical ping-pong blur before a
two-input tone map. Captures prove values above 1 survived the intermediate
format, four independent slots work, depth/orientation match, and failures
preserve prior target contents. It also covers fresh-view rejection, foreign
resources, format/binding/feedback errors, transactional HDR reload, resizing,
two previews, a native 6144×2560 target, retained device lifetime, producer storage
retired immediately after consumer submission and four actual renderer replacements.

This slice adds no compute, multisampling, MRT, mip chains, persistent attachment
loads, native HDR displays, temporal history policy or automatic render graph.
The application chooses effect shaders, bloom scale/threshold, tone mapping,
exposure and ownership policy. Android/Quest's separate GLES/OpenXR probe is not
the desktop OpenGL4.1 renderer. Metal retains native mobile compatibility; new
backends must implement this same explicit contract when added.

Local validation on Apple M2 Max: strict C++20 and C++23 builds each passed all 14
CPU CTests with unrelated physics/Lua/companion modules disabled; native HDR,
indexed mesh and live-pipeline suites passed both standards. The final HDR
composite differed by 0/255 between Metal and OpenGL. The native HDR suite also
passed ASan/UBSan with leak detection disabled on macOS. Existing native Metal/GL
and eight-handoff regressions also passed. API documentation and argument naming
are audited, and pinned formatting/static analysis cover the new code. An iOS
arm64 simulator consumer links the shared Metal HDR API; Android arm64 compiles
the portable declarations. These are cross-build proofs, not mobile HDR hardware
or Quest renderer runtime validation.
