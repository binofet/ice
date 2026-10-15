#include "ice/render/live_pipeline.h"
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <utility>

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::cerr << "Live pipeline check " << __LINE__ << ": " << #condition << '\n';         \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

namespace {
class provider final : public ice::io::virtual_scheme_handler {
public:
	std::string                   text;
	unsigned                      reads = 0;
	ice::io::resource_read_result read(const ice::io::resource_uri &,
	                                   std::vector<unsigned char> &bytes, std::string &) override {
		++reads;
		bytes.assign(text.begin(), text.end());
		return ice::io::resource_read_result::success;
	}
};
std::string source(ice::render::backend_kind kind, int variant) {
	if (kind == ice::render::backend_kind::metal) {
		const std::string body =
		    variant == 0   ? "return values.color;"
		    : variant == 1 ? "return float4(1-values.color.rgb,1);"
		    : variant == 2
		        ? "return values.color+values.extra;"
		        : "constexpr sampler s(coord::normalized); return image.sample(s,float2(.5));";
		return "#include <metal_stdlib>\nusing namespace metal;\n"
		       "struct output {float4 position [[position]];};\n"
		       "struct parameters {float4 color;" +
		       std::string(variant == 2 ? "float4 extra;" : "") +
		       "};\n"
		       "vertex output reload_vertex(uint i [[vertex_id]]) {const float2 "
		       "p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)}; return {float4(p[i],0,1)};}\n"
		       "fragment float4 reload_fragment(constant parameters &values [[buffer(0)]]" +
		       std::string(variant == 3 ? ",texture2d<float> image [[texture(0)]]" : "") + ") {" +
		       body + "}\n";
	}
	const std::string body = variant == 0   ? "color=values;"
	                         : variant == 1 ? "color=vec4(1-values.rgb,1);"
	                         : variant == 2 ? "color=values+extra;"
	                                        : "color=texture(image,vec2(.5));";
	return "#ifdef ICE_VERTEX_STAGE\nvoid reload_vertex() {const vec2 "
	       "p[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3)); "
	       "gl_Position=vec4(p[gl_VertexID],0,1);}\n#endif\n"
	       "#ifdef ICE_FRAGMENT_STAGE\nlayout(location=0) out vec4 color;\n"
	       "layout(std140) uniform parameters {vec4 values;" +
	       std::string(variant == 2 ? "vec4 extra;" : "") + "};\n" +
	       std::string(variant == 3 ? "uniform sampler2D image;\n" : "") +
	       "void reload_fragment() {" + body + "}\n#endif\n";
}
bool matches(const ice::render::image &image, const std::array<float, 4> &color) {
	for (std::size_t index = 0; index < image.rgba.size(); ++index)
		if (std::abs(int(image.rgba[index]) - int(std::lround(color[index % 4] * 255))) > 1)
			return false;
	return image.width == 63 && image.height == 31;
}
} // namespace
int main() {
	using namespace ice::render;
	std::vector<backend_kind> available;
#ifdef ICE_TEST_METAL
	available.push_back(backend_kind::metal);
#endif
#ifdef ICE_TEST_OPENGL
	available.push_back(backend_kind::opengl);
#endif
	CHECK(!available.empty());
	for (auto kind : available) {
		ice::io::resource_resolver resolver;
		auto                       provider = std::make_shared<::provider>();
		resolver.register_handler("memory", provider);
		provider->text = source(kind, 0);
		auto original  = shader_snapshot::read(resolver, "memory://reload/source");
		CHECK(original && provider->reads == 1);
		provider->text   = source(kind, 1);
		auto replacement = shader_snapshot::read(resolver, "memory://reload/source");
		CHECK(replacement && provider->reads == 2 && replacement->sha256() != original->sha256());
		auto gpu = device::create(kind), other = device::create(kind);
		CHECK(gpu && other && (*gpu)->identity() && (*gpu)->identity() != (*other)->identity());
		std::array<float, 4> colors{.2f, .6f, .3f, 1}, inverted{.8f, .4f, .7f, 1};
		auto                 uniforms = std::as_bytes(std::span(colors));
		live_pipeline        live;
		CHECK(!live.current() && live.revision() == 0 && !live.can_undo());
		CHECK(!live.activate(**gpu, 0) && !live.undo(**gpu, 0));
		CHECK(live.prepare(**gpu, *original, "reload_vertex", "reload_fragment", 0, uniforms));
		CHECK(provider->reads == 2 && !live.current() && live.has_prepared());
		CHECK(!live.activate(**other, 0) && live.has_prepared());
		CHECK(live.activate(**gpu, 0) == 1 && live.current() && !live.has_prepared());
		CHECK(live.device_identity() == (*gpu)->identity());
		auto check_active = [&](const auto &expected) {
			auto captured = (*gpu)->render_offscreen(*live.current(), 63, 31, uniforms);
			return captured && matches(*captured, expected);
		};
		CHECK(check_active(colors));
		CHECK(live.source()->sha256() == original->sha256());
		provider->text = "this is invalid shader syntax";
		auto invalid   = shader_snapshot::read(resolver, "memory://reload/invalid");
		CHECK(invalid &&
		      !live.prepare(**gpu, *invalid, "reload_vertex", "reload_fragment", 1, uniforms));
		CHECK(!live.prepare(**gpu, *replacement, "missing", "reload_fragment", 1, uniforms));
		CHECK(live.revision() == 1 && check_active(colors));
		provider->text      = source(kind, 2);
		auto larger_binding = shader_snapshot::read(resolver, "memory://reload/larger");
		CHECK(larger_binding && !live.prepare(**gpu, *larger_binding, "reload_vertex",
		                                      "reload_fragment", 1, uniforms));
		provider->text = source(kind, 3);
		auto sampled   = shader_snapshot::read(resolver, "memory://reload/sampled");
		CHECK(sampled &&
		      !live.prepare(**gpu, *sampled, "reload_vertex", "reload_fragment", 1, uniforms));
		image pixels{1, 1, {10, 20, 30, 255}};
		auto  foreign_image = (*other)->create_texture(pixels);
		CHECK(foreign_image && !live.prepare(**gpu, *sampled, "reload_vertex", "reload_fragment", 1,
		                                     uniforms, foreign_image->get()));
		CHECK(check_active(colors) && live.revision() == 1);
		CHECK(
		    !live.prepare(**other, *replacement, "reload_vertex", "reload_fragment", 1, uniforms));
		CHECK(!live.prepare(**gpu, *replacement, "reload_vertex", "reload_fragment", 0, uniforms));
		CHECK(live.prepare(**gpu, *replacement, "reload_vertex", "reload_fragment", 1, uniforms));
		// A rejected edit does not replace an already prepared valid transaction.
		CHECK(!live.prepare(**gpu, *invalid, "reload_vertex", "reload_fragment", 1, uniforms));
		CHECK(live.has_prepared());
		CHECK(!live.activate(**gpu, 0) && !live.activate(**other, 1));
		CHECK(check_active(colors) && live.has_prepared());
		CHECK(live.activate(**gpu, 1) == 2 && live.can_undo() && check_active(inverted));
		CHECK(!live.undo(**gpu, 1) && !live.undo(**other, 2) && check_active(inverted));
		CHECK(live.prepare(**gpu, *replacement, "reload_vertex", "reload_fragment", 2, uniforms));
		CHECK(live.undo(**gpu, 2) == 3 && !live.has_prepared() && !live.can_undo());
		CHECK(!live.activate(**gpu, 2) && !live.undo(**gpu, 3) && check_active(colors));
		// This owner explicitly promises an empty, reusable moved-from state. Exercise
		// that stronger contract rather than treating moved-from access as unspecified.
		// NOLINTBEGIN(bugprone-use-after-move)
		live_pipeline moved(std::move(live));
		CHECK(!live.current() && live.revision() == 0 && moved.revision() == 3);
		CHECK(!live.activate(**gpu, 0));
		CHECK(live.prepare(**other, *original, "reload_vertex", "reload_fragment", 0, uniforms));
		CHECK(live.activate(**other, 0) == 1);
		live = std::move(moved);
		CHECK(live.revision() == 3 && check_active(colors) && !moved.current());
		// NOLINTEND(bugprone-use-after-move)
		CHECK(live.prepare(**gpu, *replacement, "reload_vertex", "reload_fragment", 3, uniforms));
		live.discard_prepared();
		CHECK(!live.has_prepared() && !live.activate(**gpu, 3) && check_active(colors));
		CHECK((*gpu)->take_error().empty());
		std::cout << "Live pipeline passed on " << (*gpu)->name()
		          << ": immutable bytes, preparation failures, bindings, stale/device rejection, "
		             "activation/undo and move lifetime\n";
	}
}
