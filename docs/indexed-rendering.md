# Indexed drawing and ordered frame batches

The first indexed rendering profile supports immutable position/normal triangle
lists on Metal and OpenGL 4.1 core. It uses the same logical device ownership,
shader URI snapshots, uniform block and sampled image contracts as procedural
drawing. It does not create a camera, simulation, material model or application.

## Authored resources

`mesh_data` owns CPU vertices and uint32 indices. A vertex contains two
`std::array<float,3>` fields, position followed by normal, with a fixed 24-byte
stride. `device::create_mesh` validates counts, finite components and every index
before uploading independent immutable GPU storage. The CPU description may be
changed or released after creation. Retain a description when it must be uploaded
to a replacement logical device.

The bounds are 1–1048576 vertices and 3–3145728 indices, with a multiple of three
indices. Degenerate triangles and non-unit/zero normals are not geometry repair
errors; the shader author chooses how to use their values. Indices do not have to
be sequential. GPU meshes retain their native device lifetime independently of
the C++ device wrapper.

`pipeline_options` selects procedural or `position_normal` vertex input, depth
testing, depth writing and premultiplied alpha blending. Defaults preserve the
existing procedural path. Indexed shader attributes are float3 position at
location/attribute zero and float3 normal at one. Position must be active; unused
normal input may be optimized out. Other active attributes and mismatched types
are rejected. Metal's fixed vertex descriptor fetches buffer one, reserving buffer
zero for the caller's uniform bytes. GL uses per-context VAOs with shared immutable
vertex/index buffers.

Camera/model/normal transforms, lighting and tint remain authored uniform/shader
contracts. There is no automatic normal transformation or native memory-layout
translation. Shared scene camera matrices use zero-to-one clip depth. Metal
consumes it directly; the authored GLSL entry explicitly adapts its output with
`gl_Position.z = 2.0 * gl_Position.z - gl_Position.w`. The backend does not rewrite
arbitrary shader source. The matching test shader packages demonstrate this.

## One clear, ordered draws

Each borrowed `draw_item` points to a pipeline, an optional mesh, uniform bytes and
an optional sampled image. `frame_batch` supplies an ordered span of items and a
normalized RGBA clear color. `presentation_surface::draw_batch` and
`device::render_offscreen(batch,width,height)` validate the entire batch before
touching a render target. Foreign pipelines/meshes/images, missing required
bindings, wrong input layouts and oversized uniform data reject the whole batch.
Empty batches only clear. Resources and byte spans remain valid throughout the
synchronous call; native APIs retain submitted resources and copy uniform bytes.

A batch clears color and depth once, then executes items in their authored order.
Indexed input requires a mesh; procedural input rejects one. Existing `draw` and
procedural offscreen overloads remain one-item conveniences. Pipeline preparation
does not detach an active surface, and mesh/shader uploads do not replace existing
resources on failure.

Depth uses less-than comparison and a clear value of one. Writing depth without
depth testing is rejected. There is no face culling in this profile. Metal and
offscreen GL captures use 32-bit float depth; GL native window presentation
requests a 24-bit depth buffer. Do not assume identical precision at coincident
surfaces or rasterization boundaries. Readback pixels remain top-row-first RGBA8.

Blending is premultiplied `ONE, ONE_MINUS_SRC_ALPHA` for color and alpha, matching
native text canvases. Opaque meshes normally use depth test/write and no blending.
A procedural terminal overlay normally disables depth test/write and enables
blending. Transparent spatial geometry can test depth without writing it; the
caller remains responsible for draw sorting. No automatic sorting or OIT is
implied.

The bounds are 131072 items, 4096 uniform bytes per item and 64 MiB of aggregate
uniform bytes per frame. The existing RGBA8 sampled image at binding zero
is available to either shader stage. GPU HDR targets and explicit four-slot postprocess
bindings extend that baseline; see [HDR postprocessing](hdr-postprocessing.md). Image updates replace storage and preserve
the image used by preceding submissions. The profile has no additional vertex
formats, UV/color attributes, mutable vertex streams, instancing, mesh shaders or
compute dispatch. These can be added through explicit capability/API extensions.

## Live replacement and tests

`live_pipeline::prepare` accepts optional pipeline options and a probe mesh after
its existing binding arguments. Indexed preparation compiles exact retained bytes
and renders a detached 63×31 probe using a same-device mesh. Stale revisions,
foreign probes and invalid candidates preserve active/staged/history owners.
Activation/undo retain the pipeline's vertex/depth/blend state. A logical-device
replacement creates a new holder and uploads retained CPU descriptions; resources
cannot migrate between devices implicitly.

`ice_mesh_tests tests/data/mesh` runs explicitly with a native GPU, outside CPU
CTest. It covers nonsequential indexing, upload independence, normal input,
occlusion and order, depth-write policy, clipping, mixed sampled terminal overlay,
repeated depth clears, bounds/foreign-resource rejection, live options undo,
independent previews, resize/suspend and retained device lifetime. Four actual
renderer replacements preserve a window and CPU simulation state. The same suite
compares Metal/GL mixed-scene readback pixels with a small UNORM tolerance.

The whole-batch rejection guarantee applies to checked input and ownership
failures. Native compiler/driver errors, hangs and shader faults are not an
application rollback or crash sandbox.
