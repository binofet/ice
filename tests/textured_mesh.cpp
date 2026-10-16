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
			std::fprintf(stderr, "Textured mesh check %d: %s\n", __LINE__, #condition);            \
			return false;                                                                          \
		}                                                                                          \
	} while (0)

namespace {
using namespace ice::render;
constexpr int width = 64, height = 48;

constexpr std::string_view gl_source = R"(
#ifdef ICE_VERTEX_STAGE
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 uv;
out vec2 surface_uv;
out vec3 surface_normal;
void uv_vertex() {
    gl_Position=vec4(position.xy,2.0*position.z-1.0,1);
    surface_uv=uv.xy;
    surface_normal=normal;
}
void plain_vertex() {
    gl_Position=vec4(position.xy,2.0*position.z-1.0,1);
    surface_uv=vec2(0);
    surface_normal=normal;
}
void screen_vertex() {
    const vec2 corners[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));
    vec2 p=corners[gl_VertexID];
    gl_Position=vec4(p,0,1);
    surface_uv=vec2(p.x*.5+.5,.5-p.y*.5);
    surface_normal=vec3(0,0,1);
}
#endif
#ifdef ICE_FRAGMENT_STAGE
in vec2 surface_uv;
in vec3 surface_normal;
uniform sampler2D ice_texture0;
layout(location=0) out vec4 color;
void sample_fragment() {
    vec4 sampled=texture(ice_texture0,surface_uv);
    color=vec4(sampled.rgb*(.25+.75*surface_normal.z),sampled.a);
}
void solid_fragment() { color=vec4(surface_normal,1); }
#endif
)";

constexpr std::string_view metal_source = R"(
#include <metal_stdlib>
using namespace metal;
struct uv_input {
    float3 position [[attribute(0)]];
    float3 normal [[attribute(1)]];
    float2 uv [[attribute(2)]];
};
struct plain_input { float3 position [[attribute(0)]]; float3 normal [[attribute(1)]]; };
struct output { float4 position [[position]]; float2 uv; float3 normal; };
vertex output uv_vertex(uv_input input [[stage_in]]) {
    return {float4(input.position,1),input.uv.xy,input.normal};
}
vertex output plain_vertex(plain_input input [[stage_in]]) {
    return {float4(input.position,1),float2(0),input.normal};
}
vertex output screen_vertex(uint index [[vertex_id]]) {
    const float2 corners[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};
    float2 p=corners[index];
    return {float4(p,0,1),float2(p.x*.5+.5,.5-p.y*.5),float3(0,0,1)};
}
fragment float4 sample_fragment(output input [[stage_in]],texture2d<float> image [[texture(0)]]) {
    constexpr sampler linear_clamp(coord::normalized,address::clamp_to_edge,filter::linear);
    float4 sampled=image.sample(linear_clamp,input.uv);
    return float4(sampled.rgb*(.25+.75*input.normal.z),sampled.a);
}
fragment float4 solid_fragment(output input [[stage_in]]) { return float4(input.normal,1); }
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

textured_mesh_data square() {
	// Nonsequential indices avoid accidentally testing an implicit vertex-order triangle list.
	return {{{{9, 9, 9}, {0, 0, 0}, {0, 0}},
	         {{-1, -1, .25f}, {0, 0, 1}, {0, 1}},
	         {{1, -1, .25f}, {0, 0, 1}, {1, 1}},
	         {{1, 1, .25f}, {0, 0, 1}, {1, 0}},
	         {{-1, 1, .25f}, {0, 0, 1}, {0, 0}}},
	        {1, 2, 3, 1, 3, 4}};
}
mesh_data plain_square() {
	mesh_data result;
	for (const auto &vertex : square().vertices)
		result.vertices.push_back({vertex.position, vertex.normal});
	result.indices = square().indices;
	return result;
}
image checker() {
	image result{4, 4, {}};
	for (unsigned y = 0; y < 4; ++y)
		for (unsigned x = 0; x < 4; ++x) {
			// Alternating checker colors also vary by row/column, detecting orientation flips.
			if ((x + y) % 2)
				result.rgba.insert(result.rgba.end(), {std::uint8_t(30 + x * 20),
				                                       std::uint8_t(180 - y * 20), 220, 255});
			else
				result.rgba.insert(result.rgba.end(), {std::uint8_t(230 - x * 20),
				                                       std::uint8_t(20 + y * 25), 40, 255});
		}
	return result;
}
std::array<int, 4> sample(const image &texture, float u, float v, float brightness = 1) {
	const float        x = u * texture.width - .5f, y = v * texture.height - .5f;
	const int          left = int(std::floor(x)), top = int(std::floor(y));
	const float        fx = x - float(left), fy = y - float(top);
	std::array<int, 4> result{};
	for (unsigned channel = 0; channel < 4; ++channel) {
		float value = 0;
		for (int row = 0; row < 2; ++row)
			for (int column = 0; column < 2; ++column) {
				const auto offset =
				    (std::size_t(std::clamp(top + row, 0, texture.height - 1)) * texture.width +
				     std::clamp(left + column, 0, texture.width - 1)) *
				        4 +
				    channel;
				value += texture.rgba[offset] * (column ? fx : 1 - fx) * (row ? fy : 1 - fy);
			}
		result[channel] = int(std::lround(value * (channel < 3 ? brightness : 1)));
	}
	return result;
}
bool pixel(const image &output, int x, int y, std::array<int, 4> expected) {
	const auto offset = (std::size_t(y) * output.width + x) * 4;
	for (unsigned channel = 0; channel < 4; ++channel)
		if (std::abs(int(output.rgba[offset + channel]) - expected[channel]) > 2)
			return false;
	return true;
}
bool same(const image &a, const image &b) {
	if (a.width != b.width || a.height != b.height || a.rgba.size() != b.rgba.size())
		return false;
	for (std::size_t at = 0; at < a.rgba.size(); ++at)
		if (std::abs(int(a.rgba[at]) - int(b.rgba[at])) > 2)
			return false;
	return true;
}

bool exercise(backend_kind backend, image &parity) {
	auto gpu = device::create(backend);
	if (!gpu)
		std::fprintf(stderr, "Textured mesh backend unavailable: %s\n",
		             gpu.error().message.c_str());
	CHECK(gpu);
	std::printf("Textured mesh backend: %s\n", (*gpu)->name().c_str());
	ice::io::resource_resolver resolver;
	auto                       provider = std::make_shared<source_handler>();
	provider->source                    = backend == backend_kind::metal ? metal_source : gl_source;
	resolver.register_handler("memory", provider);
	auto source = shader_snapshot::read(resolver, "memory://indexed-uv/probe");
	CHECK(source);
	pipeline_options options;
	options.vertices         = vertex_layout::position_normal_uv;
	options.sampled_bindings = 1;
	auto pipeline = (*gpu)->prepare_pipeline(*source, "uv_vertex", "sample_fragment", options);
	CHECK(pipeline);
	options.vertices = vertex_layout::position_normal;
	CHECK(!(*gpu)->prepare_pipeline(*source, "uv_vertex", "sample_fragment", options));
	options.sampled_bindings = 0;
	auto plain = (*gpu)->prepare_pipeline(*source, "plain_vertex", "solid_fragment", options);
	CHECK(plain);
	options.vertices = vertex_layout::position_normal_uv;
	auto unused_uv   = (*gpu)->prepare_pipeline(*source, "plain_vertex", "solid_fragment", options);
	CHECK(unused_uv);
	options.vertices         = vertex_layout::procedural;
	options.sampled_bindings = 1;
	auto screen = (*gpu)->prepare_pipeline(*source, "screen_vertex", "sample_fragment", options);
	CHECK(screen);
	options.vertices = static_cast<vertex_layout>(99);
	CHECK(!(*gpu)->prepare_pipeline(*source, "uv_vertex", "sample_fragment", options));
	// A float3 at location2 must fail reflected layout validation, even though shader code is
	// valid.
	const std::string from =
	    backend == backend_kind::metal ? "float2 uv [[attribute(2)]]" : "in vec2 uv;";
	const std::string to =
	    backend == backend_kind::metal ? "float3 uv [[attribute(2)]]" : "in vec3 uv;";
	provider->source.replace(provider->source.find(from), from.size(), to);
	auto bad_source = shader_snapshot::read(resolver, "memory://indexed-uv/bad-type");
	CHECK(bad_source);
	options.vertices = vertex_layout::position_normal_uv;
	auto bad_pipeline =
	    (*gpu)->prepare_pipeline(*bad_source, "uv_vertex", "sample_fragment", options);
	CHECK(!bad_pipeline && bad_pipeline.error().message.find("vertex inputs") != std::string::npos);
	auto description  = square();
	auto geometry     = (*gpu)->create_mesh(description);
	auto old_geometry = (*gpu)->create_mesh(plain_square());
	CHECK(geometry && old_geometry);
	for (float invalid :
	     {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
		auto bad              = description;
		bad.vertices[2].uv[1] = invalid;
		CHECK(!(*gpu)->create_mesh(bad));
	}
	auto bad       = description;
	bad.indices[0] = 99;
	CHECK(!(*gpu)->create_mesh(bad));
	bad                         = description;
	bad.vertices[1].position[0] = std::numeric_limits<float>::quiet_NaN();
	CHECK(!(*gpu)->create_mesh(bad));
	bad                       = description;
	bad.vertices[1].normal[2] = std::numeric_limits<float>::infinity();
	CHECK(!(*gpu)->create_mesh(bad));
	bad.indices.clear();
	CHECK(!(*gpu)->create_mesh(bad));
	const auto pixels  = checker();
	auto       texture = (*gpu)->create_texture(pixels);
	CHECK(texture);
	// Mutating/releasing the CPU arrays does not alter the already uploaded 32-byte stream.
	description.vertices.assign(description.vertices.size(), {});
	description.indices.clear();
	std::array items{draw_item{pipeline->get(), geometry->get(), {}, texture->get()}};
	auto       output = (*gpu)->render_offscreen(frame_batch{items}, width, height);
	CHECK(output);
	for (auto point : {std::array{5, 6}, std::array{20, 9}, std::array{40, 30}, std::array{57, 40}})
		CHECK(pixel(*output, point[0], point[1],
		            sample(pixels, (point[0] + .5f) / width, (point[1] + .5f) / height)));
	parity = *output;
	std::array old_items{draw_item{plain->get(), old_geometry->get(), {}, nullptr}};
	auto       old_output = (*gpu)->render_offscreen(frame_batch{old_items}, width, height);
	CHECK(old_output && pixel(*old_output, width / 2, height / 2, {0, 0, 255, 255}));
	old_items[0]       = {unused_uv->get(), geometry->get(), {}, nullptr};
	auto unused_output = (*gpu)->render_offscreen(frame_batch{old_items}, width, height);
	CHECK(unused_output && same(*old_output, *unused_output));
	// Mirrored authored coordinates must mirror texture content, independently of screen position.
	auto mirrored = square();
	for (auto &vertex : mirrored.vertices)
		vertex.uv[0] = 1 - vertex.uv[0];
	auto mirror_geometry = (*gpu)->create_mesh(mirrored);
	CHECK(mirror_geometry);
	items[0].geometry = mirror_geometry->get();
	auto mirror       = (*gpu)->render_offscreen(frame_batch{items}, width, height);
	CHECK(mirror && !same(*output, *mirror));
	CHECK(pixel(*mirror, 5, 6, sample(pixels, 1 - 5.5f / width, 6.5f / height)));
	for (auto &vertex : mirrored.vertices) {
		vertex.uv     = {-2, 3};
		vertex.normal = {0, 0, 0};
	}
	auto clamped_geometry = (*gpu)->create_mesh(mirrored);
	CHECK(clamped_geometry);
	items[0].geometry = clamped_geometry->get();
	auto clamped      = (*gpu)->render_offscreen(frame_batch{items}, width, height);
	CHECK(clamped && pixel(*clamped, width / 2, height / 2, sample(pixels, -2, 3, .25f)));
	items[0].geometry = geometry->get();
	auto target       = (*gpu)->create_render_target({width, height, color_format::rgba8_unorm});
	CHECK(target && (*gpu)->render_into(**target, frame_batch{items}));
	std::array readback{draw_item{screen->get(), nullptr, {}, &(*target)->color()}};
	auto       before = (*gpu)->render_offscreen(frame_batch{readback}, width, height);
	CHECK(before && same(*before, *output));
	// A valid first draw followed by any bad layout must preserve the initialized target.
	std::array invalid{items[0], items[0]};
	for (auto mismatch : {draw_item{pipeline->get(), old_geometry->get(), {}, texture->get()},
	                      draw_item{plain->get(), geometry->get(), {}, nullptr},
	                      draw_item{unused_uv->get(), old_geometry->get(), {}, nullptr},
	                      draw_item{screen->get(), geometry->get(), {}, texture->get()},
	                      draw_item{pipeline->get(), nullptr, {}, texture->get()}}) {
		invalid[1] = mismatch;
		CHECK(!(*gpu)->render_into(**target, frame_batch{invalid, {1, 0, 1, 1}}));
		auto after = (*gpu)->render_offscreen(frame_batch{readback}, width, height);
		CHECK(after && same(*before, *after));
	}
	auto foreign = device::create(backend);
	CHECK(foreign);
	auto foreign_geometry = (*foreign)->create_mesh(square());
	CHECK(foreign_geometry);
	invalid[1] = {pipeline->get(), foreign_geometry->get(), {}, texture->get()};
	CHECK(!(*gpu)->render_into(**target, frame_batch{invalid, {1, 0, 1, 1}}));
	CHECK((*gpu)->take_error().empty());
	CHECK((*gpu)->wait_idle());
	// Native owners remain valid to retire after the logical device wrapper has gone away.
	gpu->reset();
	foreign->reset();
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
	if (captures.empty()) {
		std::fputs("Textured mesh test requires an enabled native renderer\n", stderr);
		return 1;
	}
	if (captures.size() == 2 && !same(captures[0], captures[1])) {
		std::fputs("Metal/OpenGL indexed UV capture parity failed\n", stderr);
		return 1;
	}
	std::puts("Textured meshes passed: 32-byte UV checker parity, mirrored/clamped coordinates, "
	          "24-byte compatibility, immutable uploads and pre-clear layout rejection");
}
