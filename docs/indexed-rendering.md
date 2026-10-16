# Indexed drawing and ordered frame batches

The indexed rendering profile supports immutable position/normal,
position/normal/UV and position/normal/UV/skin triangle lists on Metal and OpenGL
4.1 core. It uses the same logical device ownership,
shader URI snapshots, uniform block and sampled image contracts as procedural
drawing. It does not create a camera, simulation, material model or application.

The CPU camera API in `ice/render/camera.h` belongs to `ice::assets` and needs no
renderer or window. A `camera_pose` contains world position and a camera-to-world
quaternion: local +X is right, +Y is up and -Z is forward. `look_at_pose` constructs
that pose; `make_camera_view` validates it and builds perspective or orthographic
matrices for an explicit drawable size and clip-depth convention. Perspective FOV
uses radians; orthographic extent is the full vertical span in world units.

The returned `camera_view` carries eye, basis vectors and an inverse view-projection.
`camera_ray` unprojects normalized top-left coordinates in [0,1]. Its origin lies
on the near plane, its direction has unit length, and its finite length reaches
the far plane for either projection. Use that segment for bounded picking casts.
Invalid poses, degenerate look-at axes, singular/unrepresentable matrices and clip
ranges that lose a finite far plane return diagnostics. These factories do not
select an active camera, own input or alter authored scene state.

## Authored resources

`mesh_data` owns CPU vertices and uint32 indices. A vertex contains two
`std::array<float,3>` fields, position followed by normal, with a fixed 24-byte
stride. `device::create_mesh` validates counts, finite components and every index
before uploading independent immutable GPU storage. The CPU description may be
changed or released after creation. Retain a description when it must be uploaded
to a replacement logical device.

`textured_mesh_data` is an additive description with `mesh_vertex_uv` vertices:
float3 position, float3 normal and float2 UV, in that order. Its fixed stride is
32 bytes, with offsets 0, 12 and 24. The original `mesh_vertex`/`mesh_data` contract
remains 24 bytes. All `device::create_mesh` overloads upload independent immutable
storage, and each GPU mesh retains its exact declared layout. A shader that ignores
UV still needs a `position_normal_uv` pipeline when consuming a 32-byte mesh;
matching the position/normal prefix does not permit an implicit stride conversion.

`skinned_mesh_data` adds `mesh_vertex_skin`: position3, normal3, UV2, joints4 and
weights4, with a fixed 64-byte stride and offsets 0, 12, 24, 32 and 48. Joints are
finite integral floats in `[0,16777215]`, the portable consecutive exact-integer
profile. Weights are finite values in `[0,1]` whose sum differs from one by at most
0.0001. Upload validates all four lanes, including zero-weight lanes, and preserves
the authored values without normalization. This stream has an exact layout identity
independent of the 24/32-byte prefixes.

Palette layout/count, mesh-to-palette remapping, animation and skinning math belong
to the caller's shader. The engine imposes no asset joint-count limit. Before drawing,
the caller must check every joint lane against that draw's actual palette: a
zero-weight lane can still cause an out-of-bounds shader access. A caller may pack
an asset's referenced joints into a smaller palette without changing its source
skeleton. Normal transformation, including nonuniform scale, remains authored
shader math; upload never substitutes a rigid normal transform.

UV components must be finite. They may be outside `[0,1]`; upload does not clamp or
rewrite authored coordinates. The existing texture contract samples uploaded row
zero at UV y=0, using linear filtering and clamp-to-edge without mipmaps. There is
no material system, glTF texture transform or implicit UV flip in this addition.
An authored shader chooses which coordinates, textures and color-space operations
it uses.

The bounds are 1–1048576 vertices and 3–3145728 indices, with a multiple of three
indices. Degenerate triangles and non-unit/zero normals are not geometry repair
errors; the shader author chooses how to use their values. Indices do not have to
be sequential. GPU meshes retain their native device lifetime independently of
the C++ device wrapper.

`pipeline_options` selects procedural, `position_normal`, `position_normal_uv` or
`position_normal_uv_skin` vertex input, depth testing, depth writing and
premultiplied alpha blending. Defaults preserve the
existing procedural path. Indexed shader attributes are float3 position at
location/attribute zero and float3 normal at one. Position must be active; unused
normal input may be optimized out. `position_normal_uv` additionally permits
float2 UV at location/attribute two, which may also be optimized out. The skin layout
additionally permits float4 joint indices at three and float4 weights at four.
Other active attributes, arrays and mismatched types are rejected. Metal's fixed
vertex descriptor fetches buffer one, reserving buffer zero for the caller's
uniform bytes. GL uses per-context VAOs with shared immutable
vertex/index buffers, restoring the appropriate 24/32/64-byte stride and disabling
unused attributes when a batch switches layouts. Mesh/pipeline layouts must match
exactly in portable and backend-specific entry points, before target clears or GPU
encoding.

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
depth testing is rejected. There is no face culling in this profile. Counterclockwise
triangles are front-facing on both backends, including their shader built-ins; a
mirrored transform reverses that classification. Material shaders may choose their
own discard or two-sided shading policy. Metal and
offscreen GL captures use 32-bit float depth; GL native window presentation
requests a 24-bit depth buffer. Do not assume identical precision at coincident
surfaces or rasterization boundaries. Readback pixels remain top-row-first RGBA8.

Blending is premultiplied `ONE, ONE_MINUS_SRC_ALPHA` for color and alpha, matching
native text canvases. Opaque meshes normally use depth test/write and no blending.
A procedural terminal overlay normally disables depth test/write and enables
blending. Transparent spatial geometry can test depth without writing it; the
caller remains responsible for draw sorting. No automatic sorting or OIT is
implied.

The bounds are 131072 items, `max_uniform_bytes` (16384) uniform bytes per item and
64 MiB of aggregate
uniform bytes per frame. The existing RGBA8 sampled image at binding zero
is available to either shader stage. GPU HDR targets and explicit four-slot postprocess
bindings extend that baseline; see [HDR postprocessing](hdr-postprocessing.md). Image updates replace storage and preserve
the image used by preceding submissions. Beyond these three indexed layouts, the profile has no additional vertex
formats, color/tangent attributes, mutable vertex streams, instancing, mesh shaders or
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

`ice_textured_mesh_tests` is likewise GPU-only. It retains embedded shader bytes
through a URI provider and uploads an asymmetric checker image. Tests compare
actual interpolated UV sampling against CPU bilinear expectations, mirror authored
UVs independently of screen position, exercise out-of-range clamping and the normal
offset, switch between old/new layouts, and compare Metal/OpenGL captures. Invalid
UVs, reflected attribute types, exact layout mismatches and foreign resources are
rejected; a valid first draw followed by an invalid layout preserves an initialized
render target. CPU arrays may be released after upload, and native resource owners
can retire safely after their logical-device wrapper.

`ice_skinned_mesh_tests` compares two-joint GPU deformation with CPU-baked position,
inverse-transpose normal and UV references, including nonuniform joint scale. Both
backends render through a full 16 KiB block used by vertex and fragment stages, and
exercise 4096/4112/16384-byte boundaries plus rejected 16385-byte submissions and
16400-byte reflected blocks. Invalid joint/weight data and exact layout mismatches
are rejected; an invalid second draw preserves a previously initialized target.
Submitted large uniforms remain independent of caller storage, and resources may
outlive the logical-device wrapper.

Metal uses `setBytes` through 4096 bytes and owned shared `MTLBuffer` storage above
that boundary, bound at buffer zero in both stages. It allocates every large draw
buffer before beginning the render pass, so a checked allocation failure cannot
partially clear or draw that target. OpenGL verifies the native uniform-block
limit at device creation and requires at least the portable 16 KiB profile. The
aggregate frame budget remains 64 MiB.

The whole-batch rejection guarantee applies to checked input and ownership
failures. Native compiler/driver errors, hangs and shader faults are not an
application rollback or crash sandbox.
