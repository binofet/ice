#include "ice/render/metal/device.h"
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>
bool borrowed_layer_ownership_test();
#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::cerr << "Metal check " << __LINE__ << ": " << #condition << '\n';                 \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)
int main(int argc, char **argv) {
	using namespace ice::render::metal;
	using ice::platform::desktop_window;
	using ice::render::frame_status;
	if (argc != 2)
		return 2;
	CHECK(borrowed_layer_ownership_test());
	ice::io::resource_resolver resolver;
	std::string                diagnostics;
	ice::io::resource_uri      directory;
	CHECK(ice::io::resource_uri::from_path(std::string(argv[1]) + "/", directory, diagnostics));
	CHECK(resolver.mount("shaders://tests/", directory.string(), diagnostics));
	auto gpu = device::create();
	if (!gpu) {
		std::cerr << gpu.error().message << '\n';
		return 1;
	}
	std::cout << "GPU: " << (*gpu)->name() << '\n';
	auto prepared = (*gpu)->prepare_pipeline(resolver, "shaders://tests/probe.metal",
	                                         "probe_vertex", "probe_fragment");
	if (!prepared) {
		std::cerr << prepared.error().message << '\n';
		return 1;
	}
	auto invalid = (*gpu)->prepare_pipeline(resolver, "shaders://tests/invalid.metal", "v", "f");
	CHECK(!invalid &&
	      invalid.error().message.find("shaders://tests/invalid.metal") != std::string::npos);
	CHECK(!(*gpu)->prepare_pipeline(resolver, "shaders://tests/missing.metal", "v", "f"));
	CHECK(!(*gpu)->prepare_pipeline(resolver, "shaders://tests/probe.metal", "missing",
	                                "probe_fragment"));
	CHECK(!(*gpu)->prepare_pipeline(resolver, "shaders://tests/probe.metal", "probe_vertex",
	                                "unsupported_buffer"));
	CHECK(!(*gpu)->prepare_pipeline(resolver, "shaders://tests/probe.metal", "probe_vertex",
	                                "unsupported_texture"));
	CHECK(!(*gpu)->prepare_pipeline(resolver, "shaders://tests/probe.metal", "probe_vertex",
	                                "unsupported_depth"));
	CHECK(!(*gpu)->prepare_pipeline(resolver, "shaders://tests/probe.metal", "probe_vertex",
	                                "unsupported_texture_array"));
	std::array<float, 4> values{.25f, .5f, .75f, 1};
	auto                 uniforms = std::as_bytes(std::span(values));
	CHECK(!(*gpu)->render_offscreen(**prepared, 32, 16));
	CHECK(!(*gpu)->render_offscreen(**prepared, 0, 16, uniforms));
	std::vector<std::byte> oversized(ice::render::max_uniform_bytes + 1);
	CHECK(!(*gpu)->render_offscreen(**prepared, 32, 16, oversized));
	auto other = device::create();
	CHECK(other && !(*other)->render_offscreen(**prepared, 32, 16, uniforms));
	CHECK(!(*gpu)->attach_layer(nullptr, 32, 16));
	auto capture = (*gpu)->render_offscreen(**prepared, 63, 31, uniforms);
	if (!capture) {
		std::cerr << capture.error().message << '\n';
		return 1;
	}
	CHECK(capture->rgba.size() == 63 * 31 * 4);
	for (std::size_t pixel : {std::size_t(0), std::size_t(31 * 63 / 2), std::size_t(63 * 31 - 1)})
		for (std::size_t channel = 0; channel < 4; ++channel)
			CHECK(std::abs(int(capture->rgba[pixel * 4 + channel]) -
			               int(std::lround(values[channel] * 255))) <= 1);
	auto coordinates = (*gpu)->prepare_pipeline(resolver, "shaders://tests/probe.metal",
	                                            "probe_vertex", "probe_coordinates");
	CHECK(coordinates);
	values        = {63, 31, 0, 0};
	auto oriented = (*gpu)->render_offscreen(**coordinates, 63, 31, uniforms);
	CHECK(oriented && oriented->rgba[0] < 5 && oriented->rgba[1] < 5);
	std::size_t last = oriented->rgba.size() - 4;
	CHECK(oriented->rgba[last] > 245 && oriented->rgba[last + 1] > 245);
	CHECK(std::abs(int(oriented->rgba[last + 2]) - 64) <= 1);
	// An asymmetric image catches both RGBA swizzles and top/bottom convention errors.
	ice::render::image pixels{
	    2, 2, {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255}};
	auto sampled       = (*gpu)->create_texture(pixels);
	auto foreign_image = (*other)->create_texture(pixels);
	CHECK(sampled && foreign_image);
	CHECK(!(*gpu)->create_texture({2, 2, {}}));
	CHECK(!(*gpu)->create_texture({4097, 1, {}}));
	auto textured = (*gpu)->prepare_pipeline(resolver, "shaders://tests/probe.metal",
	                                         "probe_vertex", "probe_texture");
	CHECK(textured);
	values = {2, 2, 0, 0};
	CHECK(!(*gpu)->render_offscreen(**textured, 2, 2, uniforms));
	CHECK(!(*gpu)->render_offscreen(**textured, 2, 2, uniforms, foreign_image->get()));
	auto uploaded = (*gpu)->render_offscreen(**textured, 2, 2, uniforms, sampled->get());
	CHECK(uploaded && uploaded->rgba == pixels.rgba);
	pixels.rgba[0] = 64;
	CHECK((*sampled)->update(pixels));
	CHECK(!(*sampled)->update({1, 1, {0, 0, 0, 0}}));
	auto updated = (*gpu)->render_offscreen(**textured, 2, 2, uniforms, sampled->get());
	CHECK(updated && updated->rgba == pixels.rgba);
	values      = {.25f, .5f, .75f, 1};
	auto first  = desktop_window::create({"ICE Metal one", 640, 400, true});
	auto second = desktop_window::create({"ICE Metal two", 480, 320, true});
	CHECK(first && second);
	auto first_surface  = (*gpu)->attach(**first);
	auto second_surface = (*gpu)->attach(**second);
	CHECK(first_surface && second_surface);
	unsigned submitted = 0;
	for (int frame = 0; frame < 20; ++frame) {
		desktop_window::pump_events();
		auto a = (*first_surface)->draw(**prepared, uniforms);
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
	CHECK((*first_surface)->resize(0, 0));
	auto suspended = (*first_surface)->draw(**prepared, uniforms);
	CHECK(suspended && *suspended == frame_status::unavailable);
	CHECK(!(*first_surface)->resize(-1, 40));
	first_surface->reset();
	first->reset();
	CHECK((*second_surface)->draw(**prepared, uniforms));
	// Readback is a deliberate queue drain here; the real presentation path stays asynchronous.
	CHECK((*gpu)->render_offscreen(**prepared, 16, 16, uniforms));
	CHECK((*gpu)->take_error().empty());
	gpu->reset();
	CHECK((*second_surface)->draw(**prepared, uniforms));
	second_surface->reset();
	second->reset();
	std::cout << "Metal passed: URI diagnostics, reflected binding validation, RGBA/orientation "
	             "readback, independent previews, resize/suspend and retained resource lifetime\n";
}
