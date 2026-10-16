#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace ice::render {
/// Supported immutable vertex input contracts. Uniform bytes remain at binding zero.
/// Indexed scene shaders use zero-to-one clip depth. Authored GLSL entries explicitly map
/// gl_Position.z = 2*z-w for OpenGL; Metal consumes the shared convention directly.
enum class vertex_layout { procedural, position_normal };
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
struct pipeline_options {
	/// Procedural triangles, or indexed float3 position/normal attributes at locations zero/one.
	/// Metal fetches their fixed 24-byte stride from vertex buffer one, reserving buffer zero.
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
