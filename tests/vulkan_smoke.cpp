#include "ice/render/device.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <thread>

namespace {
using namespace ice::render;
class packet_provider final : public ice::io::virtual_scheme_handler {
public:
	std::vector<unsigned char>    bytes;
	ice::io::resource_read_result read(const ice::io::resource_uri &uri,
	                                   std::vector<unsigned char>  &out,
	                                   std::string                 &error) override {
		if (uri.string() != "smoke://shader/mesh") {
			error = "Unknown smoke shader";
			return ice::io::resource_read_result::error;
		}
		out = bytes;
		return ice::io::resource_read_result::success;
	}
};
template <typename T> T take(ice::expected<T, diagnostic> v) {
	if (!v)
		throw std::runtime_error(v.error().message);
	return std::move(*v);
}
} // namespace
int main(int argc, char **argv) try {
	if (argc != 2)
		throw std::runtime_error("Usage: ice_vulkan_smoke path/to/ice-vulkan-mesh.shader");
	auto          provider = std::make_shared<packet_provider>();
	std::ifstream file(argv[1], std::ios::binary);
	if (!file)
		throw std::runtime_error("Cannot open named smoke shader packet");
	provider->bytes.resize(1024 * 1024 + 1);
	file.read(reinterpret_cast<char *>(provider->bytes.data()),
	          std::streamsize(provider->bytes.size()));
	provider->bytes.resize(std::size_t(file.gcount()));
	if (provider->bytes.size() > 1024 * 1024)
		throw std::runtime_error("Smoke shader exceeds1MiB");
	ice::io::resource_resolver resolver;
	resolver.register_handler("smoke", provider);
	auto window =
	    ice::platform::desktop_window::create({"ICE Vulkan indexed mesh smoke", 900, 600, true});
	if (!window)
		throw std::runtime_error(window.error().message);
	auto gpu = take(device::create(backend_kind::vulkan));
	std::printf("Vulkan smoke device: %s\n", gpu->name().c_str());
	auto             surface = take(gpu->attach(**window));
	pipeline_options options;
	options.vertices   = vertex_layout::position_normal;
	options.depth_test = options.depth_write = true;
	options.sampled_bindings                 = 0;
	auto pipeline =
	    take(gpu->prepare_pipeline(resolver, {"smoke://shader/mesh", "main", "main"}, options));
	mesh_data data;
	for (float z : {-0.3f, 0.3f})
		for (float y : {-0.45f, 0.45f})
			for (float x : {-0.45f, 0.45f})
				data.vertices.push_back({{x, y, z}, {x, y, z}});
	data.indices = {0, 1, 3, 0, 3, 2, 4, 6, 7, 4, 7, 5, 0, 4, 5, 0, 5, 1,
	                2, 3, 7, 2, 7, 6, 0, 2, 6, 0, 6, 4, 1, 5, 7, 1, 7, 3};
	auto mesh    = take(gpu->create_mesh(data));
	// Invalid and duplicate ownership checks use the real common device API.
	if (gpu->attach(**window))
		throw std::runtime_error("Duplicate window attachment accepted");
	if (gpu->create_texture({1, 1, {0, 0, 0, 255}}))
		throw std::runtime_error("Unsupported texture accepted");
	if (surface->resize(-1, 1))
		throw std::runtime_error("Invalid resize accepted");
	if (!surface->resize(0, 0) ||
	    take(surface->draw_batch(frame_batch{})) != frame_status::unavailable)
		throw std::runtime_error("Zero-size suspension failed");
	std::uint64_t submitted = 0, busy = 0, unavailable = 0, resized_frames = 0;
	const auto    initial_extent = (*window)->extent();
	bool          resized        = false;
	const auto    start          = std::chrono::steady_clock::now();
	while (!(*window)->should_close()) {
		ice::platform::desktop_window::pump_events();
		const float seconds =
		    std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
		if (seconds >= 10)
			break;
		if (seconds >= 2 && !resized) {
			if (!(*window)->resize(1100, 700))
				throw std::runtime_error("Native resize refused");
			resized = true;
		}
		const auto extent = (*window)->extent();
		if (!surface->resize(extent.pixel_width, extent.pixel_height))
			throw std::runtime_error("Surface resize refused");
		const float           c = std::cos(seconds), s = std::sin(seconds);
		std::array<float, 20> uniforms{c,       0, -s * .6f, 0, 0,    1, 0,    0,    s, 0,
		                               c * .6f, 0, 0,        0, 0.5f, 1, 0.2f, 0.8f, 1, 1};
		const draw_item       draw{pipeline.get(), mesh.get(), std::as_bytes(std::span(uniforms))};
		const auto            status =
		    take(surface->draw_batch({std::span(&draw, 1), {0.02f, 0.03f, 0.05f, 1}}));
		if (status == frame_status::submitted) {
			++submitted;
			if (resized && (extent.pixel_width != initial_extent.pixel_width ||
			                extent.pixel_height != initial_extent.pixel_height))
				++resized_frames;
		} else if (status == frame_status::busy)
			++busy;
		else
			++unavailable;
		if (auto error = gpu->take_error(); !error.empty())
			throw std::runtime_error(error);
		std::this_thread::sleep_for(std::chrono::milliseconds(8));
	}
	// Submission owners must retain pipeline/mesh storage after these public owners retire.
	mesh.reset();
	pipeline.reset();
	auto idle = surface->wait_idle();
	if (!idle)
		throw std::runtime_error(idle.error().message);
	surface.reset();
	if (auto error = gpu->take_error(); !error.empty())
		throw std::runtime_error(error);
	std::printf("Vulkan smoke: submitted=%llu busy=%llu unavailable=%llu resized_frames=%llu; "
	            "owned closure "
	            "complete\n",
	            static_cast<unsigned long long>(submitted), static_cast<unsigned long long>(busy),
	            static_cast<unsigned long long>(unavailable),
	            static_cast<unsigned long long>(resized_frames));
	if (!submitted || !resized_frames)
		throw std::runtime_error("Smoke ended before mesh submission and resize");
	return 0;
} catch (const std::exception &e) {
	std::fprintf(stderr, "Vulkan smoke failed: %s\n", e.what());
	return 1;
}
