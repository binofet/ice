#include "ice/render/device.h"
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>
#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::cerr << "Render handoff check " << __LINE__ << ": " << #condition << '\n';        \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)
int main(int argc, char **argv) {
	using namespace ice::render;
	if (argc != 2)
		return 2;
	ice::io::resource_resolver resolver;
	ice::io::resource_uri      directory;
	std::string                error;
	CHECK(ice::io::resource_uri::from_path(std::string(argv[1]) + "/", directory, error));
	CHECK(resolver.mount("shaders://tests/", directory.string(), error));
	auto source = [](backend_kind kind) {
		return shader_source{kind == backend_kind::metal ? "shaders://tests/metal/probe.metal"
		                                                 : "shaders://tests/gl/probe.glsl",
		                     "probe_vertex", "probe_fragment"};
	};
	std::vector<backend_kind> available;
#ifdef ICE_TEST_METAL
	available.push_back(backend_kind::metal);
#endif
#ifdef ICE_TEST_OPENGL
	available.push_back(backend_kind::opengl);
#endif
	CHECK(!available.empty());
	auto window = ice::platform::desktop_window::create({"ICE renderer handoff", 640, 400, true});
	CHECK(window);
	const auto identity = (*window)->id();
	auto       active   = device::create(available[0]);
	CHECK(active);
	auto prepared = (*active)->prepare_pipeline(resolver, source(available[0]));
	CHECK(prepared);
	auto surface = (*active)->attach(**window);
	CHECK(surface);
	CHECK(!(*active)->attach(**window));
	// These are authored scene/session values. A device swap must not reset them or the window.
	std::array<float, 4>                   scene{.2f, .7f, .4f, 1};
	auto                                   uniforms         = std::as_bytes(std::span(scene));
	unsigned                               simulation_steps = 0;
	std::vector<std::unique_ptr<pipeline>> retired;
	for (unsigned iteration = 0; iteration < 8; ++iteration) {
		auto candidate = device::create(available[(iteration + 1) % available.size()]);
		CHECK(candidate);
		auto bad =
		    (*candidate)->prepare_pipeline(resolver, {"shaders://tests/missing.shader", "v", "f"});
		CHECK(!bad && (*surface)->draw(**prepared, uniforms));
		auto replacement =
		    (*candidate)->prepare_pipeline(resolver, source((*candidate)->backend()));
		CHECK(replacement);
		CHECK(!(*candidate)->render_offscreen(**prepared, 63, 31, uniforms));
		auto capture = (*candidate)->render_offscreen(**replacement, 63, 31, uniforms);
		CHECK(capture);
		for (std::size_t i = 0; i < capture->rgba.size(); ++i)
			CHECK(std::abs(int(capture->rgba[i]) - int(std::lround(scene[i % 4] * 255))) <= 1);
		// Detach only after the replacement has compiled and rendered. Keep the old owner and
		// pipeline alive until attachment succeeds, making restoration possible on native failure.
		CHECK((*surface)->wait_idle());
		surface->reset();
		auto attached = (*candidate)->attach(**window);
		if (!attached) {
			std::cerr << attached.error().message << '\n';
			surface = (*active)->attach(**window);
			CHECK(surface && (*surface)->draw(**prepared, uniforms));
			return 1;
		}
		surface = std::move(attached);
		retired.push_back(std::move(*prepared));
		prepared = std::move(replacement);
		active   = std::move(candidate);
		CHECK(!(*surface)->draw(*retired.back(), uniforms));
		unsigned presentations = 0;
		for (int frame = 0; frame < 3; ++frame) {
			ice::platform::desktop_window::pump_events();
			auto draw = (*surface)->draw(**prepared, uniforms);
			CHECK(draw);
			presentations += *draw == frame_status::submitted;
			++simulation_steps;
			std::this_thread::sleep_for(std::chrono::milliseconds(8));
		}
		CHECK(presentations > 0);
		CHECK((*window)->id() == identity && simulation_steps == (iteration + 1) * 3);
		CHECK((*active)->take_error().empty());
		if (iteration == 3) {
			CHECK((*window)->resize(720, 480));
			ice::platform::desktop_window::pump_events();
			auto extent = (*window)->extent();
			CHECK((*surface)->resize(extent.pixel_width, extent.pixel_height));
		}
		std::cout << "Handoff " << iteration + 1 << ": " << (*active)->name() << "; window "
		          << identity << "; simulation " << simulation_steps << '\n';
	}
	CHECK((*surface)->wait_idle());
	std::cout << "Prepared failure preservation, cross-device rejection, eight handoffs, retained "
	             "CPU/window state and readback parity passed\n";
}
