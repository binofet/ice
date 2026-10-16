#include "ice/render/gl/device.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::cerr << "OpenGL check " << __LINE__ << ": " << #condition << '\n';                \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

namespace {
class malformed_sources final : public ice::io::virtual_scheme_handler {
public:
	ice::io::resource_read_result read(const ice::io::resource_uri &uri,
	                                   std::vector<unsigned char>  &bytes,
	                                   std::string                 &diagnostics) override {
		(void)diagnostics;
		if (uri.path == "/oversized")
			bytes.assign(1024 * 1024 + 1, 'x');
		else if (uri.path == "/nul")
			bytes = {'x', 0, 'y'};
		else if (uri.path == "/utf8")
			bytes = {0xc0, 0xaf};
		else
			bytes.clear();
		return ice::io::resource_read_result::success;
	}
};
class sampled_sources final : public ice::io::virtual_scheme_handler {
public:
	ice::io::resource_read_result read(const ice::io::resource_uri &uri,
	                                   std::vector<unsigned char>  &bytes,
	                                   std::string                 &diagnostics) override {
		(void)diagnostics;
		std::string fragment;
		if (uri.path == "/array")
			fragment = "uniform sampler2D images[2]; void sampled_fragment() { color = "
			           "texture(images[0],vec2(.5))+texture(images[1],vec2(.5)); }";
		else if (uri.path == "/array-one")
			fragment = "uniform sampler2D images[1]; void sampled_fragment() { color = "
			           "texture(images[0],vec2(.5)); }";
		else if (uri.path == "/cube")
			fragment = "uniform samplerCube image; void sampled_fragment() { color = "
			           "texture(image,vec3(1)); }";
		else if (uri.path == "/shadow")
			fragment = "uniform sampler2DShadow image; void sampled_fragment() { color = "
			           "vec4(texture(image,vec3(.5))); }";
		else if (uri.path == "/two")
			fragment = "uniform sampler2D first_image, second_image; void sampled_fragment() { "
			           "color = texture(first_image,vec2(.5))+texture(second_image,vec2(.5)); }";
		else {
			const std::string coordinates = uri.path == "/linear" ? "vec2(.5)"
			                                : uri.path == "/clamp"
			                                    ? "vec2(-2,2)"
			                                    : "vec2(gl_FragCoord.x/2,1-gl_FragCoord.y/2)";
			fragment = "uniform sampler2D image; void sampled_fragment() { color = "
			           "texture(image," +
			           coordinates + "); }";
		}
		const std::string source =
		    "#ifdef ICE_VERTEX_STAGE\nvoid sampled_vertex() { const vec2 p[3] = "
		    "vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3)); "
		    "gl_Position = vec4(p[gl_VertexID],0,1); }\n#endif\n"
		    "#ifdef ICE_FRAGMENT_STAGE\nlayout(location=0) out vec4 color;\n" +
		    fragment + "\n#endif\n";
		bytes.assign(source.begin(), source.end());
		return ice::io::resource_read_result::success;
	}
};
bool matches(const ice::render::image &actual, const ice::render::image &expected) {
	if (actual.width != expected.width || actual.height != expected.height ||
	    actual.rgba.size() != expected.rgba.size())
		return false;
	for (std::size_t index = 0; index < actual.rgba.size(); ++index)
		if (std::abs(int(actual.rgba[index]) - int(expected.rgba[index])) > 1)
			return false;
	return true;
}
} // namespace

int main(int argc, char **argv) {
	using namespace ice::render::gl;
	using ice::platform::desktop_window;
	using ice::render::frame_status;
	if (argc != 2)
		return 2;
	ice::io::resource_resolver resolver;
	std::string                diagnostics;
	ice::io::resource_uri      directory;
	CHECK(ice::io::resource_uri::from_path(std::string(argv[1]) + "/", directory, diagnostics));
	CHECK(resolver.mount("shaders://tests/", directory.string(), diagnostics));
	resolver.register_handler("malformed", std::make_shared<malformed_sources>());
	resolver.register_handler("sampled", std::make_shared<sampled_sources>());
	auto gpu = device::create();
	if (!gpu) {
		std::cerr << gpu.error().message << '\n';
		return 1;
	}
	std::cout << "GPU: " << (*gpu)->name() << '\n';
	auto prepared = (*gpu)->prepare_pipeline(resolver, "shaders://tests/probe.glsl", "probe_vertex",
	                                         "probe_fragment");
	if (!prepared) {
		std::cerr << prepared.error().message << '\n';
		return 1;
	}
	auto invalid = (*gpu)->prepare_pipeline(resolver, "shaders://tests/invalid.glsl", "v", "f");
	CHECK(!invalid &&
	      invalid.error().message.find("shaders://tests/invalid.glsl") != std::string::npos);
	CHECK(!(*gpu)->prepare_pipeline(resolver, "shaders://tests/missing.glsl", "v", "f"));
	CHECK(!(*gpu)->prepare_pipeline(resolver, "shaders://tests/probe.glsl", "missing",
	                                "probe_fragment"));
	CHECK(!(*gpu)->prepare_pipeline(resolver, "shaders://tests/probe.glsl", "probe_vertex",
	                                "invalid name"));
	CHECK(!(*gpu)->prepare_pipeline(resolver, "shaders://tests/unsupported.glsl",
	                                "attribute_vertex", "plain_fragment"));
	for (const auto *fragment : {"uniform_fragment", "blocks_fragment", "oversized_fragment"})
		CHECK(!(*gpu)->prepare_pipeline(resolver, "shaders://tests/unsupported.glsl",
		                                "probe_vertex", fragment));

	for (const auto *uri : {"malformed:///empty", "malformed:///oversized", "malformed:///nul",
	                        "malformed:///utf8"}) {
		auto rejected = (*gpu)->prepare_pipeline(resolver, uri, "v", "f");
		CHECK(!rejected && rejected.error().message.find(uri) != std::string::npos);
	}

	std::array<float, 4> values{.25f, .5f, .75f, 1};
	auto                 uniforms = std::as_bytes(std::span(values));
	CHECK(!(*gpu)->render_offscreen(**prepared, 32, 16));
	CHECK(!(*gpu)->render_offscreen(**prepared, 0, 16, uniforms));
	CHECK(!(*gpu)->render_offscreen(**prepared, 4097, 16, uniforms));
	std::vector<std::byte> oversized(ice::render::max_uniform_bytes + 1);
	CHECK(!(*gpu)->render_offscreen(**prepared, 32, 16, oversized));
	auto other = device::create();
	CHECK(other && !(*other)->render_offscreen(**prepared, 32, 16, uniforms));
	auto sampled_pipeline = (*gpu)->prepare_pipeline(resolver, "sampled:///oriented",
	                                                 "sampled_vertex", "sampled_fragment");
	CHECK(sampled_pipeline);
	for (const auto *path : {"array", "array-one", "cube", "shadow", "two"})
		CHECK(!(*gpu)->prepare_pipeline(resolver, std::string("sampled:///") + path,
		                                "sampled_vertex", "sampled_fragment"));
	ice::render::image pixels{
	    2, 2, {255, 0, 0, 255, 0, 255, 0, 128, 0, 0, 255, 64, 255, 255, 255, 0}};
	CHECK(!(*gpu)->create_texture({0, 2, {}}));
	CHECK(!(*gpu)->create_texture({-1, 2, {}}));
	CHECK(!(*gpu)->create_texture({4097, 1, {}}));
	CHECK(!(*gpu)->create_texture({2, 4097, {}}));
	auto malformed_pixels = pixels;
	malformed_pixels.rgba.pop_back();
	CHECK(!(*gpu)->create_texture(malformed_pixels));
	malformed_pixels.rgba.insert(malformed_pixels.rgba.end(), 2, 0);
	CHECK(!(*gpu)->create_texture(malformed_pixels));
	CHECK((*gpu)->create_texture({1, 1, {255, 0, 0, 255}}));
	CHECK((*gpu)->create_texture({4096, 1, std::vector<std::uint8_t>(4096 * 4)}));
	auto sampled         = (*gpu)->create_texture(pixels);
	auto foreign_texture = (*other)->create_texture(pixels);
	CHECK(sampled && foreign_texture);
	CHECK(!(*gpu)->render_offscreen(**sampled_pipeline, 2, 2));
	CHECK(!(*gpu)->render_offscreen(**sampled_pipeline, 2, 2, {}, foreign_texture->get()));
	CHECK(!(*gpu)->render_offscreen(**prepared, 2, 2, uniforms, foreign_texture->get()));
	auto uploaded = (*gpu)->render_offscreen(**sampled_pipeline, 2, 2, {}, sampled->get());
	CHECK(uploaded && matches(*uploaded, pixels));
	auto linear = (*gpu)->prepare_pipeline(resolver, "sampled:///linear", "sampled_vertex",
	                                       "sampled_fragment");
	auto clamp  = (*gpu)->prepare_pipeline(resolver, "sampled:///clamp", "sampled_vertex",
	                                       "sampled_fragment");
	CHECK(linear && clamp);
	auto filtered = (*gpu)->render_offscreen(**linear, 1, 1, {}, sampled->get());
	CHECK(filtered && matches(*filtered, {1, 1, {128, 128, 128, 112}}));
	auto clamped = (*gpu)->render_offscreen(**clamp, 1, 1, {}, sampled->get());
	CHECK(clamped && matches(*clamped, {1, 1, {0, 0, 255, 64}}));
	CHECK(!(*sampled)->update({1, 4, pixels.rgba}));
	CHECK(!(*sampled)->update(malformed_pixels));
	uploaded = (*gpu)->render_offscreen(**sampled_pipeline, 2, 2, {}, sampled->get());
	CHECK(uploaded && matches(*uploaded, pixels));
	std::reverse(pixels.rgba.begin(), pixels.rgba.end());
	CHECK((*sampled)->update(pixels));
	uploaded = (*gpu)->render_offscreen(**sampled_pipeline, 2, 2, {}, sampled->get());
	CHECK(uploaded && matches(*uploaded, pixels));

	// Prior valid programs remain usable after all failed preparation attempts. Width 63
	// exposes implementations that confuse GPU row alignment with the packed image contract.
	auto capture = (*gpu)->render_offscreen(**prepared, 63, 31, uniforms);
	if (!capture) {
		std::cerr << capture.error().message << '\n';
		return 1;
	}
	CHECK(capture->width == 63 && capture->height == 31 && capture->rgba.size() == 63 * 31 * 4);
	for (std::size_t pixel : {std::size_t(0), std::size_t(31 * 63 / 2), std::size_t(63 * 31 - 1)})
		for (std::size_t channel = 0; channel < 4; ++channel)
			CHECK(std::abs(int(capture->rgba[pixel * 4 + channel]) -
			               int(std::lround(values[channel] * 255))) <= 1);
	auto coordinates = (*gpu)->prepare_pipeline(resolver, "shaders://tests/probe.glsl",
	                                            "probe_vertex", "probe_coordinates");
	CHECK(coordinates);
	values        = {63, 31, 0, 0};
	auto oriented = (*gpu)->render_offscreen(**coordinates, 63, 31, uniforms);
	CHECK(oriented && oriented->rgba[0] < 5 && oriented->rgba[1] < 5);
	std::size_t last = oriented->rgba.size() - 4;
	CHECK(oriented->rgba[last] > 245 && oriented->rgba[last + 1] > 245);
	CHECK(std::abs(int(oriented->rgba[last + 2]) - 64) <= 1);
	values = {.25f, .5f, .75f, 1};

	auto first  = desktop_window::create({"ICE OpenGL one", 640, 400, true});
	auto second = desktop_window::create({"ICE OpenGL two", 480, 320, true});
	CHECK(first && second);
	auto first_surface  = (*gpu)->attach(**first);
	auto second_surface = (*gpu)->attach(**second);
	CHECK(first_surface && second_surface);
	CHECK(!(*first_surface)->draw(**sampled_pipeline));
	CHECK(!(*first_surface)->draw(**sampled_pipeline, {}, foreign_texture->get()));
	CHECK((*first_surface)->draw(**sampled_pipeline, {}, sampled->get()));
	CHECK((*second_surface)->draw(**sampled_pipeline, {}, sampled->get()));
	std::reverse(pixels.rgba.begin(), pixels.rgba.end());
	CHECK((*sampled)->update(pixels));
	CHECK((*first_surface)->draw(**sampled_pipeline, {}, sampled->get()));
	CHECK((*second_surface)->draw(**sampled_pipeline, {}, sampled->get()));
	CHECK(!(*first_surface)->draw(**prepared));
	CHECK(!(*first_surface)->draw(**prepared, oversized));
	auto foreign = (*other)->prepare_pipeline(resolver, "shaders://tests/probe.glsl",
	                                          "probe_vertex", "probe_fragment");
	CHECK(foreign && !(*first_surface)->draw(**foreign, uniforms));
	unsigned submitted = 0;
	for (int frame = 0; frame < 20; ++frame) {
		desktop_window::pump_events();
		// Alternate bytes and context order so one preview cannot accidentally rely on
		// uniform storage or VAO state belonging to the other presentation context.
		values = {.25f, .5f, .75f, 1};
		auto a = (*first_surface)->draw(**prepared, uniforms);
		values = {.75f, .25f, .5f, 1};
		auto b = (*second_surface)->draw(**prepared, uniforms);
		CHECK(a && b);
		submitted += *a == frame_status::submitted;
		submitted += *b == frame_status::submitted;
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	CHECK(submitted >= 2 && (*first)->resize(720, 480));
	desktop_window::pump_events();
	auto extent = (*first)->extent();
	CHECK((*first_surface)->resize(extent.pixel_width, extent.pixel_height));
	CHECK((*first_surface)->draw(**prepared, uniforms));
	CHECK((*first_surface)->wait_idle());
	CHECK((*first_surface)->resize(0, 0));
	auto suspended = (*first_surface)->draw(**prepared, uniforms);
	CHECK(suspended && *suspended == frame_status::unavailable);
	CHECK(!(*first_surface)->resize(-1, 40));
	CHECK(!(*first_surface)->resize(40, 16385));
	CHECK((*first_surface)->resize(extent.pixel_width, extent.pixel_height));
	CHECK((*first_surface)->draw(**prepared, uniforms));
	(*first)->request_close();
	CHECK((*first)->should_close() && !(*second)->should_close());
	first_surface->reset();
	first->reset();
	CHECK((*second_surface)->draw(**prepared, uniforms));
	CHECK((*second_surface)->wait_idle());
	CHECK((*gpu)->render_offscreen(**prepared, 16, 16, uniforms));
	CHECK((*gpu)->take_error().empty());
	gpu->reset();
	CHECK((*sampled)->update(pixels));
	CHECK((*second_surface)->draw(**sampled_pipeline, {}, sampled->get()));
	sampled->reset();
	CHECK((*second_surface)->wait_idle());
	CHECK((*second_surface)->draw(**prepared, uniforms));
	CHECK((*second_surface)->wait_idle());
	second_surface->reset();
	second->reset();
	std::cout
	    << "OpenGL passed: URI diagnostics, binding validation, RGBA/orientation readback, "
	       "top-origin texture upload/update, sampling bounds/filtering, independent previews, "
	       "resize/suspend, queue drain and retained resource lifetime\n";
}
