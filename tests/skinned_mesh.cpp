#include "ice/math/types.h"
#include "ice/render/device.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <string_view>

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::fprintf(stderr, "Skinned mesh check %d: %s\n", __LINE__, #condition);             \
			return false;                                                                          \
		}                                                                                          \
	} while (0)

namespace {
using namespace ice::render;
using ice::math::mat4;
using ice::math::vec3;
using ice::math::vec4;
constexpr int width = 96, height = 80;

constexpr std::string_view gl_source    = R"(
layout(std140) uniform parameters { mat4 palette[200]; vec4 light; vec4 tail[223]; };
#ifdef ICE_VERTEX_STAGE
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 uv;
layout(location=3) in vec4 joints;
layout(location=4) in vec4 weights;
out vec2 surface_uv;
out vec3 surface_normal;
void skin_vertex() {
    ivec4 j=ivec4(joints);
    mat4 skin=palette[j.x]*weights.x+palette[j.y]*weights.y+
              palette[j.z]*weights.z+palette[j.w]*weights.w;
    gl_Position=skin*vec4(position,1);
    gl_Position.z=2.0*gl_Position.z-gl_Position.w;
    mat3 cofactors=mat3(cross(skin[1].xyz,skin[2].xyz),cross(skin[2].xyz,skin[0].xyz),
                        cross(skin[0].xyz,skin[1].xyz));
    surface_normal=normalize(cofactors*normal);
    surface_uv=uv;
}
void plain_vertex() {
    gl_Position=vec4(position,1);
    gl_Position.z=2.0*gl_Position.z-gl_Position.w;
    surface_normal=normal;
    surface_uv=uv;
}
void screen_vertex() {
    const vec2 corners[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));
    vec2 p=corners[gl_VertexID];
    gl_Position=vec4(p,0,1);
    surface_normal=vec3(0,0,1);
    surface_uv=vec2(p.x*.5+.5,.5-p.y*.5);
}
#endif
#ifdef ICE_FRAGMENT_STAGE
in vec2 surface_uv;
in vec3 surface_normal;
uniform sampler2D ice_texture0;
layout(location=0) out vec4 color;
void skin_fragment() {
    float lighting=.2+.8*max(dot(normalize(surface_normal),normalize(light.xyz)),0);
    color=texture(ice_texture0,surface_uv)*vec4(tail[222].rgb*lighting,1);
}
void copy_fragment() { color=texture(ice_texture0,surface_uv); }
#endif
)";
constexpr std::string_view metal_source = R"(
#include <metal_stdlib>
using namespace metal;
struct parameters { float4x4 palette[200]; float4 light; float4 tail[223]; };
struct skin_input {
    float3 position [[attribute(0)]]; float3 normal [[attribute(1)]];
    float2 uv [[attribute(2)]]; float4 joints [[attribute(3)]]; float4 weights [[attribute(4)]];
};
struct plain_input {
    float3 position [[attribute(0)]]; float3 normal [[attribute(1)]]; float2 uv [[attribute(2)]];
};
struct output { float4 position [[position]]; float2 uv; float3 normal; };
vertex output skin_vertex(skin_input input [[stage_in]],constant parameters &p [[buffer(0)]]) {
    uint4 j=uint4(input.joints);
    float4x4 skin=p.palette[j.x]*input.weights.x+p.palette[j.y]*input.weights.y+
                  p.palette[j.z]*input.weights.z+p.palette[j.w]*input.weights.w;
    float3x3 cofactors=float3x3(cross(skin[1].xyz,skin[2].xyz),cross(skin[2].xyz,skin[0].xyz),
                                cross(skin[0].xyz,skin[1].xyz));
    return {skin*float4(input.position,1),input.uv,normalize(cofactors*input.normal)};
}
vertex output plain_vertex(plain_input input [[stage_in]]) {
    return {float4(input.position,1),input.uv,input.normal};
}
vertex output screen_vertex(uint index [[vertex_id]]) {
    const float2 corners[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};
    float2 p=corners[index];
    return {float4(p,0,1),float2(p.x*.5+.5,.5-p.y*.5),float3(0,0,1)};
}
fragment float4 skin_fragment(output input [[stage_in]],constant parameters &p [[buffer(0)]],
                             texture2d<float> image [[texture(0)]]) {
    constexpr sampler linear_clamp(coord::normalized,address::clamp_to_edge,filter::linear);
    float lighting=.2+.8*max(dot(normalize(input.normal),normalize(p.light.xyz)),0.0);
    return image.sample(linear_clamp,input.uv)*float4(p.tail[222].rgb*lighting,1);
}
fragment float4 copy_fragment(output input [[stage_in]],texture2d<float> image [[texture(0)]]) {
    constexpr sampler linear_clamp(coord::normalized,address::clamp_to_edge,filter::linear);
    return image.sample(linear_clamp,input.uv);
}
)";

class source_handler final : public ice::io::virtual_scheme_handler {
public:
	std::string                   source;
	ice::io::resource_read_result read(const ice::io::resource_uri &,
	                                   std::vector<unsigned char> &bytes, std::string &) override {
		bytes.assign(source.begin(), source.end());
		return ice::io::resource_read_result::success;
	}
};
struct alignas(16) parameters {
	std::array<mat4, 200> palette;
	vec4                  light{.3f, .6f, 1, 0};
	std::array<vec4, 223> tail{};
};
static_assert(sizeof(parameters) == max_uniform_bytes);
auto              bytes(const parameters &values) { return std::as_bytes(std::span(&values, 1)); }
skinned_mesh_data square() {
	return {{{{9, 9, 9}, {0, 0, 1}, {0, 0}, {0, 1, 0, 0}, {1, 0, 0, 0}},
	         {{-.67f, -.61f, .3f}, {.2f, .4f, 1}, {0, 1}, {0, 1, 0, 0}, {1, 0, 0, 0}},
	         {{.64f, -.61f, .3f}, {.2f, .4f, 1}, {1, 1}, {0, 1, 0, 0}, {.75f, .25f, 0, 0}},
	         {{.64f, .68f, .3f}, {.2f, .4f, 1}, {1, 0}, {0, 1, 0, 0}, {.25f, .75f, 0, 0}},
	         {{-.67f, .68f, .3f}, {.2f, .4f, 1}, {0, 0}, {0, 1, 0, 0}, {0, 1, 0, 0}}},
	        {1, 2, 3, 1, 3, 4}};
}
textured_mesh_data deform(const skinned_mesh_data &source, const parameters &values) {
	textured_mesh_data result;
	result.indices = source.indices;
	for (const auto &vertex : source.vertices) {
		mat4 skin(0);
		for (unsigned lane = 0; lane < 4; ++lane)
			skin += values.palette[std::size_t(vertex.joints[lane])] * vertex.weights[lane];
		const auto p = skin * vec4(vertex.position[0], vertex.position[1], vertex.position[2], 1);
		const auto n = glm::normalize(glm::transpose(glm::inverse(glm::mat3(skin))) *
		                              vec3(vertex.normal[0], vertex.normal[1], vertex.normal[2]));
		result.vertices.push_back({{p.x, p.y, p.z}, {n.x, n.y, n.z}, vertex.uv});
	}
	return result;
}
image checker() {
	image result{4, 4, {}};
	for (unsigned y = 0; y < 4; ++y)
		for (unsigned x = 0; x < 4; ++x) {
			if ((x + y) % 2)
				result.rgba.insert(result.rgba.end(), {35, std::uint8_t(180 - 20 * y), 220, 255});
			else
				result.rgba.insert(result.rgba.end(), {std::uint8_t(230 - 20 * x), 40, 35, 255});
		}
	return result;
}
bool same(const image &a, const image &b) {
	if (a.width != b.width || a.height != b.height || a.rgba.size() != b.rgba.size())
		return false;
	for (std::size_t index = 0; index < a.rgba.size(); ++index)
		if (std::abs(int(a.rgba[index]) - int(b.rgba[index])) > 2) {
			std::fprintf(stderr, "Skin pixel mismatch byte%zu: %u/%u\n", index,
			             unsigned(a.rgba[index]), unsigned(b.rgba[index]));
			return false;
		}
	return true;
}

std::string block_source(backend_kind backend, unsigned words) {
	if (backend == backend_kind::opengl)
		return "layout(std140) uniform parameters { vec4 words[" + std::to_string(words) +
		       "]; };\n"
		       "#ifdef ICE_VERTEX_STAGE\nvoid block_vertex(){const vec2 p[3]=vec2[3](vec2(-1,-1),"
		       "vec2(3,-1),vec2(-1,3));gl_Position=vec4(p[gl_VertexID],0,1);}\n#endif\n"
		       "#ifdef ICE_FRAGMENT_STAGE\nlayout(location=0)out vec4 color;void block_fragment(){"
		       "color=words[0]+words[" +
		       std::to_string(words - 1) + "]; }\n#endif\n";
	return "#include <metal_stdlib>\nusing namespace metal;struct parameters{float4 words[" +
	       std::to_string(words) +
	       "];};vertex float4 block_vertex(uint i[[vertex_id]]){"
	       "const float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};return float4(p[i],0,1);}"
	       "fragment float4 block_fragment(constant parameters&p[[buffer(0)]]){return p.words[0]+"
	       "p.words[" +
	       std::to_string(words - 1) + "]; }";
}

std::string invalid_influence_source(backend_kind backend, unsigned location) {
	const auto slot = std::to_string(location);
	if (backend == backend_kind::opengl)
		return "#ifdef ICE_VERTEX_STAGE\nlayout(location=0) in vec3 position;"
		       "layout(location=" +
		       slot +
		       ") in vec3 influence;void bad_vertex(){"
		       "gl_Position=vec4(position+influence*.001,1);}\n#endif\n"
		       "#ifdef ICE_FRAGMENT_STAGE\nlayout(location=0)out vec4 color;"
		       "void bad_fragment(){color=vec4(1);}\n#endif\n";
	return "#include <metal_stdlib>\nusing namespace metal;struct input{"
	       "float3 position[[attribute(0)]];float3 influence[[attribute(" +
	       slot +
	       ")]];};vertex float4 bad_vertex(input v[[stage_in]]){"
	       "return float4(v.position+v.influence*.001,1);}"
	       "fragment float4 bad_fragment(){return float4(1);}";
}

std::string_view facing_source(backend_kind backend) {
	if (backend == backend_kind::opengl)
		return R"(
layout(std140) uniform parameters { vec4 mirror; };
#ifdef ICE_VERTEX_STAGE
void facing_vertex() {
 const vec2 p[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));
 gl_Position=vec4(p[gl_VertexID]*vec2(mirror.x,1),0,1);
}
#endif
#ifdef ICE_FRAGMENT_STAGE
layout(location=0)out vec4 color;
void facing_fragment() { color=gl_FrontFacing?vec4(0,1,0,1):vec4(1,0,0,1); }
#endif
)";
	return R"(
#include <metal_stdlib>
using namespace metal;
vertex float4 facing_vertex(uint i[[vertex_id]],constant float4 &mirror[[buffer(0)]]) {
 const float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};
 return float4(p[i]*float2(mirror.x,1),0,1);
}
fragment float4 facing_fragment(bool front[[front_facing]]) {
 return front?float4(0,1,0,1):float4(1,0,0,1);
}
)";
}

bool exercise(backend_kind backend, image &parity) {
	auto gpu = device::create(backend);
	if (!gpu)
		std::fprintf(stderr, "Skin backend unavailable: %s\n", gpu.error().message.c_str());
	CHECK(gpu);
	std::printf("Skinned mesh backend: %s\n", (*gpu)->name().c_str());
	ice::io::resource_resolver resolver;
	auto                       provider = std::make_shared<source_handler>();
	resolver.register_handler("memory", provider);
	provider->source = backend == backend_kind::metal ? metal_source : gl_source;
	auto source      = shader_snapshot::read(resolver, "memory://skin/profile");
	CHECK(source);
	pipeline_options options;
	options.vertices   = vertex_layout::position_normal_uv_skin;
	options.depth_test = options.depth_write = true;
	options.sampled_bindings                 = 1;
	auto pipeline = (*gpu)->prepare_pipeline(*source, "skin_vertex", "skin_fragment", options);
	CHECK(pipeline);
	options.vertices = vertex_layout::position_normal_uv;
	CHECK(!(*gpu)->prepare_pipeline(*source, "skin_vertex", "skin_fragment", options));
	auto reference = (*gpu)->prepare_pipeline(*source, "plain_vertex", "skin_fragment", options);
	CHECK(reference);
	options.vertices   = vertex_layout::procedural;
	options.depth_test = options.depth_write = false;
	auto copy = (*gpu)->prepare_pipeline(*source, "screen_vertex", "copy_fragment", options);
	CHECK(copy);
	auto original = square();
	auto geometry = (*gpu)->create_mesh(original);
	CHECK(geometry);
	for (float joint : {-1.f, .25f, 16777216.f, std::numeric_limits<float>::quiet_NaN(),
	                    std::numeric_limits<float>::infinity()}) {
		auto bad                  = original;
		bad.vertices[2].joints[3] = joint; // Even the zero-weight lane is validated.
		CHECK(!(*gpu)->create_mesh(bad));
	}
	for (std::array<float, 4> weights :
	     {std::array{0.f, 0.f, 0.f, 0.f}, std::array{1.f, .1f, 0.f, 0.f},
	      std::array{1.f, -.1f, .1f, 0.f},
	      std::array{std::numeric_limits<float>::quiet_NaN(), 0.f, 0.f, 0.f}}) {
		auto bad                = original;
		bad.vertices[1].weights = weights;
		CHECK(!(*gpu)->create_mesh(bad));
	}
	auto representable                  = original;
	representable.vertices[1].joints[3] = 16777215;
	CHECK(
	    (*gpu)->create_mesh(representable)); // Upload only; this draw's palette does not cover it.
	parameters values;
	values.palette.fill(mat4(1));
	values.palette[0] = glm::translate(mat4(1), vec3(-.12f, .05f, 0)) *
	                    glm::rotate(mat4(1), glm::radians(10.f), vec3(0, 1, 0));
	values.palette[1] = glm::translate(mat4(1), vec3(.1f, -.08f, 0)) *
	                    glm::rotate(mat4(1), glm::radians(-17.f), vec3(0, 0, 1)) *
	                    glm::rotate(mat4(1), glm::radians(-15.f), vec3(0, 1, 0)) *
	                    glm::scale(mat4(1), vec3(.85f, 1.1f, .9f));
	values.tail.back() = {.7f, .8f, .9f, 1};
	auto cpu           = (*gpu)->create_mesh(deform(original, values));
	auto texture       = (*gpu)->create_texture(checker());
	CHECK(cpu && texture);
	// Multiple distinct large allocations must be bound to the correct draw in order.
	parameters alternate = values;
	alternate.palette[0][3].x += .21f;
	alternate.palette[1][3].x += .21f;
	alternate.tail.back() = {.9f, .4f, .6f, 1};
	auto alternate_cpu    = (*gpu)->create_mesh(deform(original, alternate));
	CHECK(alternate_cpu);
	std::array multiple{
	    draw_item{pipeline->get(), geometry->get(), bytes(values), texture->get()},
	    draw_item{pipeline->get(), geometry->get(), bytes(alternate), texture->get()}};
	std::array multiple_cpu{
	    draw_item{reference->get(), cpu->get(), bytes(values), texture->get()},
	    draw_item{reference->get(), alternate_cpu->get(), bytes(alternate), texture->get()}};
	auto multi_output    = (*gpu)->render_offscreen(frame_batch{multiple}, width, height);
	auto multi_reference = (*gpu)->render_offscreen(frame_batch{multiple_cpu}, width, height);
	CHECK(multi_output && multi_reference && same(*multi_output, *multi_reference));
	original.vertices.clear();
	original.indices.clear();
	std::array items{draw_item{pipeline->get(), geometry->get(), bytes(values), texture->get()}};
	auto       output = (*gpu)->render_offscreen(frame_batch{items}, width, height);
	CHECK(output);
	std::array cpu_items{draw_item{reference->get(), cpu->get(), bytes(values), texture->get()}};
	auto       cpu_output = (*gpu)->render_offscreen(frame_batch{cpu_items}, width, height);
	CHECK(cpu_output && same(*output, *cpu_output));
	parity      = *output;
	auto target = (*gpu)->create_render_target({width, height, color_format::rgba8_unorm});
	CHECK(target && (*gpu)->render_into(**target, frame_batch{items}));
	// Submission copied both-stage large uniforms; caller storage can change immediately.
	const parameters saved = values;
	values.palette.fill(mat4(0));
	values.tail.fill(vec4(0));
	// GL may retain a declared but unused block in reflection. Its bytes are irrelevant
	// to this copy shader, which samples only the already-submitted target storage.
	std::array readback{draw_item{copy->get(), nullptr, bytes(values), &(*target)->color()}};
	auto       retained = (*gpu)->render_offscreen(frame_batch{readback}, width, height);
	if (!retained)
		std::fprintf(stderr, "Skin readback: %s\n", retained.error().message.c_str());
	CHECK(retained && same(*output, *retained));
	values = saved;
	std::array invalid{items[0],
	                   draw_item{reference->get(), geometry->get(), bytes(values), texture->get()}};
	CHECK(!(*gpu)->render_into(**target, frame_batch{invalid, {1, 0, 1, 1}}));
	std::vector<std::byte> oversized(max_uniform_bytes + 1);
	invalid[1]          = items[0];
	invalid[1].uniforms = oversized;
	CHECK(!(*gpu)->render_into(**target, frame_batch{invalid, {1, 0, 1, 1}}));
	invalid[1].uniforms = bytes(values).first(4096);
	CHECK(!(*gpu)->render_into(**target, frame_batch{invalid, {1, 0, 1, 1}}));
	auto preserved = (*gpu)->render_offscreen(frame_batch{readback}, width, height);
	CHECK(preserved && same(*output, *preserved));
	for (unsigned location : {3u, 4u}) {
		provider->source = invalid_influence_source(backend, location);
		auto bad = shader_snapshot::read(resolver, "memory://skin/bad-" + std::to_string(location));
		CHECK(bad);
		pipeline_options bad_options;
		bad_options.vertices         = vertex_layout::position_normal_uv_skin;
		bad_options.sampled_bindings = 0;
		CHECK(!(*gpu)->prepare_pipeline(*bad, "bad_vertex", "bad_fragment", bad_options));
	}
	// The exact setBytes/buffer boundary, complete16KiB boundary, and reflected oversize policy.
	for (unsigned words : {256u, 257u, 1024u, 1025u}) {
		provider->source = block_source(backend, words);
		auto block =
		    shader_snapshot::read(resolver, "memory://skin/block-" + std::to_string(words));
		CHECK(block);
		pipeline_options block_options;
		block_options.sampled_bindings = 0;
		auto program =
		    (*gpu)->prepare_pipeline(*block, "block_vertex", "block_fragment", block_options);
		if (words > 1024) {
			CHECK(!program);
			continue;
		}
		CHECK(program);
		std::vector<vec4> data(words, vec4(0));
		data.front().x = .125f;
		data.back()    = {.25f, .5f, .75f, 1};
		auto capture   = (*gpu)->render_offscreen(**program, 8, 8, std::as_bytes(std::span(data)));
		CHECK(capture);
		for (unsigned channel = 0; channel < 4; ++channel) {
			const std::array expected{96, 128, 191, 255};
			CHECK(std::abs(int(capture->rgba[channel]) - expected[channel]) <= 1);
		}
	}
	// Shader front-facing built-ins must agree even though this profile disables culling.
	provider->source     = facing_source(backend);
	auto facing_snapshot = shader_snapshot::read(resolver, "memory://skin/front-facing");
	CHECK(facing_snapshot);
	pipeline_options facing_options;
	facing_options.sampled_bindings = 0;
	auto facing = (*gpu)->prepare_pipeline(*facing_snapshot, "facing_vertex", "facing_fragment",
	                                       facing_options);
	CHECK(facing);
	for (float mirror : {1.f, -1.f}) {
		const vec4 value(mirror, 0, 0, 0);
		auto       capture =
		    (*gpu)->render_offscreen(**facing, 8, 8, std::as_bytes(std::span(&value, 1)));
		CHECK(capture);
		const std::array<std::uint8_t, 4> expected =
		    mirror > 0 ? std::array<std::uint8_t, 4>{0, 255, 0, 255}
		               : std::array<std::uint8_t, 4>{255, 0, 0, 255};
		for (std::size_t at = 0; at < capture->rgba.size(); at += 4)
			CHECK(std::equal(expected.begin(), expected.end(), capture->rgba.begin() + at));
	}
	CHECK((*gpu)->take_error().empty() && (*gpu)->wait_idle());
	gpu->reset();
	return true;
}
} // namespace

int main() {
	std::vector<image> captures;
#ifdef ICE_TEST_METAL
	captures.emplace_back();
	if (!exercise(backend_kind::metal, captures.back()))
		return 1;
#endif
#ifdef ICE_TEST_OPENGL
	captures.emplace_back();
	if (!exercise(backend_kind::opengl, captures.back()))
		return 1;
#endif
	if (captures.empty() || (captures.size() == 2 && !same(captures[0], captures[1])))
		return 1;
	std::puts("GPU skinning passed: two-joint CPU-reference position/normal/UV deformation, "
	          "16KiB both-stage uniforms, 4KiB buffer boundary, immutable inputs, layout rejection "
	          "and CCW/mirrored shader front-facing parity");
}
