#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace ice::render {
/// Supported immutable vertex input contracts. Uniform bytes remain at binding zero.
/// Indexed scene shaders use zero-to-one clip depth. Authored GLSL entries explicitly map
/// gl_Position.z = 2*z-w for OpenGL; Metal consumes the shared convention directly.
enum class vertex_layout {
	procedural,
	position_normal,
	position_normal_uv,
	position_normal_uv_skin
};
/// Portable per-draw uniform byte ceiling, including padding; aggregate batches retain 64 MiB.
/// Shader palette layout/count remains authored application policy within this byte contract.
inline constexpr std::size_t max_uniform_bytes = 16 * 1024;
/// Linear color attachment format. Presentation/capture remain RGBA8; HDR targets retain >1
/// radiance.
enum class color_format { rgba8_unorm, rgba16_float };
/// Immutable-size GPU target description. Prepare a replacement before swapping a resized graph.
struct render_target_description {
	/// Pixel dimensions in [1,16384], also bounded by native capabilities/allocation.
	/// Zero-sized windows skip target allocation; target dimensions must be positive.
	int width = 0, height = 0;
	/// Linear color storage; each target also has a sampleable Depth32Float attachment.
	color_format format = color_format::rgba16_float;
};
/// Explicit fixed-function state retained with the prepared pipeline.
/// Counterclockwise triangles are front-facing on both backends; this profile disables culling.
struct pipeline_options {
	/// Procedural triangles or an explicit indexed position/normal[/UV[/skin]] layout.
	/// Locations zero/one are float3, UV two is float2, joints/weights three/four are float4.
	/// Indexed strides are 24, 32 and 64 bytes respectively.
	/// Metal fetches indexed vertices from buffer one, reserving buffer zero for uniform bytes.
	vertex_layout vertices = vertex_layout::procedural;
	/// Compare incoming depth with the stored value using less-than; depth clears to one.
	bool depth_test = false;
	/// Write passing depth; true requires depth_test. Overlay pipelines normally disable both.
	bool depth_write = false;
	/// Premultiplied-alpha blending: ONE, ONE_MINUS_SRC_ALPHA for color and alpha.
	bool alpha_blend = false;
	/// Required destination color format; incompatible targets reject the whole pass.
	color_format target_format = color_format::rgba8_unorm;
	/// Exact used texture slots as bits 0..3. 0xff retains legacy reflection of optional slot zero;
	/// explicit postprocess descriptors use 0..15, including zero for no sampled resources.
	std::uint8_t sampled_bindings = 0xff;
	/// Subset of sampled_bindings using nearest, raw Depth32Float samples; no compare sampler.
	/// Legacy inferred bindings do not accept depth. Metal uses depth2d<float>; GL uses sampler2D.
	std::uint8_t depth_sampled_bindings = 0;
};
/// Fixed 24-byte interleaved vertex; arrays avoid build-dependent native vector alignment.
struct mesh_vertex {
	/// Object-space position. All three components must be finite.
	std::array<float, 3> position{};
	/// Object-space normal. Finite components are required; normalization belongs to the shader.
	std::array<float, 3> normal{};
};
/// Owned CPU triangle-list description, retained by the caller for logical-device replacement.
struct mesh_data {
	/// One to 1048576 finite position/normal vertices.
	std::vector<mesh_vertex> vertices;
	/// Three to 3145728 uint32 indices, a multiple of three, all less than vertices.size().
	std::vector<std::uint32_t> indices;
};
/// Fixed 32-byte interleaved vertex; UV extends the unchanged 24-byte position/normal prefix.
struct mesh_vertex_uv {
	/// Object-space position. All three components must be finite.
	std::array<float, 3> position{};
	/// Object-space normal. Finite components are required; normalization belongs to the shader.
	std::array<float, 3> normal{};
	/// Authored texture coordinates at location two; finite, with no forced [0,1] restriction.
	/// Uploaded image row zero samples at UV y=0; addressing/filtering remain the texture contract.
	std::array<float, 2> uv{};
};
/// Owned CPU triangle list with explicit position/normal/UV input for device reconstruction.
struct textured_mesh_data {
	/// One to 1048576 finite position/normal/UV vertices, with a fixed 32-byte stride.
	std::vector<mesh_vertex_uv> vertices;
	/// Three to 3145728 uint32 indices, a multiple of three, all less than vertices.size().
	std::vector<std::uint32_t> indices;
};
/// Fixed 64-byte immutable skin stream, extending the unchanged position/normal/UV prefix.
struct mesh_vertex_skin {
	/// Object-space position. All three components must be finite.
	std::array<float, 3> position{};
	/// Object-space normal; normalization and skin normal transforms belong to the shader.
	std::array<float, 3> normal{};
	/// Finite authored texture coordinates; uploaded image row zero samples at UV y=0.
	std::array<float, 2> uv{};
	/// Four finite integral floats in [0,16777215], the consecutive exact-integer profile.
	/// These are shader-local palette indices, not an engine-wide asset joint-count limit.
	/// The caller validates every lane against its shader palette, even zero-weight lanes.
	std::array<float, 4> joints{};
	/// Finite nonnegative influences in [0,1], summing to one within 0.0001.
	/// Upload preserves these weights exactly; it never silently normalizes them.
	std::array<float, 4> weights{1, 0, 0, 0};
};
/// Owned CPU triangle list for an explicit position/normal/UV/joints/weights input layout.
struct skinned_mesh_data {
	/// One to 1048576 validated 64-byte skin vertices. Source arrays are not retained by uploads.
	std::vector<mesh_vertex_skin> vertices;
	/// Three to 3145728 uint32 indices, a multiple of three, all less than vertices.size().
	std::vector<std::uint32_t> indices;
};
/// Recoverable shader, device, surface or submission failure.
struct diagnostic {
	/// URI-qualified compiler output or native failure explanation.
	std::string message;
};
/// Owned RGBA8 pixels, top row first, for uploads or explicit offscreen captures.
struct image {
	/// Pixel dimensions of the owned image.
	int width = 0, height = 0;
	/// Four bytes per pixel, tightly packed in RGBA order.
	std::vector<std::uint8_t> rgba;
};
/// Submission outcome; unavailable/busy leave the previous presented image intact.
enum class frame_status { submitted, unavailable, busy };
} // namespace ice::render
