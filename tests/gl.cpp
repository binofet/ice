#include "ice/render/gl/device.h"
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
	for (const auto *fragment :
	     {"uniform_fragment", "texture_fragment", "blocks_fragment", "oversized_fragment"})
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
	std::vector<std::byte> oversized(4097);
	CHECK(!(*gpu)->render_offscreen(**prepared, 32, 16, oversized));
	auto other = device::create();
	CHECK(other && !(*other)->render_offscreen(**prepared, 32, 16, uniforms));

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
	CHECK((*second_surface)->draw(**prepared, uniforms));
	CHECK((*second_surface)->wait_idle());
	second_surface->reset();
	second->reset();
	std::cout
	    << "OpenGL passed: URI diagnostics, binding validation, RGBA/orientation readback, "
	       "independent previews, resize/suspend, queue drain and retained resource lifetime\n";
}
