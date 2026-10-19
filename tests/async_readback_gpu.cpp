#include "ice/render/device.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>

// Manual, offscreen acceptance fixture. No window, presentation, app, input or provider.
// Invoke one explicit backend per process; unavailable devices are failures, never skips.
namespace {
using namespace ice::render;
using clock_type     = std::chrono::steady_clock;
constexpr int  width = 63, height = 31;
constexpr auto deadline_seconds = std::chrono::seconds(10);
void           check(bool condition, const std::string &message) {
    if (!condition)
        throw std::runtime_error(message);
}
template <class T> T unwrap(ice::expected<T, diagnostic> result, const std::string &stage) {
	if (!result)
		throw std::runtime_error(stage + ": " + result.error().message);
	return std::move(*result);
}
struct parameters {
	std::array<float, 16> transform{.45f, 0, 0, 0, 0, .45f, 0, 0, 0, 0, 1, 0, -.5f, .5f, .25f, 1};
	std::array<float, 4>  tint{1, 0, 0, 1}, light{0, 0, 1, 0};
};
struct observation {
	unsigned polls = 0, pending = 0;
};
readback_status poll(readback_ticket &ticket, observation &observed) {
	const auto status = unwrap(ticket.poll(), "poll");
	++observed.polls;
	observed.pending += status == readback_status::pending;
	return status;
}
void await(readback_ticket &ticket, readback_status terminal, observation &observed) {
	const auto deadline = clock_type::now() + deadline_seconds;
	for (;;) {
		const auto status = poll(ticket, observed);
		if (status == terminal)
			return;
		check(status == readback_status::pending, "Unexpected ticket terminal state");
		check(clock_type::now() < deadline, "Ticket did not retire within fixture deadline");
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}
void record(const char *stage, const observation &observed) {
	std::cout << "stage=" << stage << " polls=" << observed.polls
	          << " pending_observed=" << observed.pending << '\n';
}
void busy(ice::expected<readback_submission, diagnostic> result, const char *stage) {
	auto rejected = unwrap(std::move(result), stage);
	check(rejected.status == frame_status::busy && !rejected.ticket,
	      std::string(stage) + ": expected busy with empty ticket");
}
void diagnostic_rejection(ice::expected<readback_submission, diagnostic> result,
                          const char                                    *stage) {
	check(!result, std::string(stage) + ": invalid request was accepted");
	check(!result.error().message.empty(), std::string(stage) + ": missing diagnostic");
	std::cout << "rejected=" << stage << " diagnostic=" << result.error().message << '\n';
}
readback_ticket submit(device &gpu, const frame_batch &frame, readback_limits limits) {
	auto submitted = unwrap(gpu.render_offscreen_async(frame, width, height, limits), "submit");
	check(submitted.status == frame_status::submitted && bool(submitted.ticket),
	      "Expected submitted with a ticket");
	return std::move(submitted.ticket);
}
readback_ticket readmit(device &gpu, const frame_batch &frame, readback_limits limits,
                        const char *stage) {
	const auto deadline   = clock_type::now() + deadline_seconds;
	unsigned   busy_polls = 0;
	for (;;) {
		auto next = unwrap(gpu.render_offscreen_async(frame, width, height, limits), stage);
		if (next.status == frame_status::submitted) {
			check(bool(next.ticket), "Submitted replacement has no ticket");
			std::cout << "stage=" << stage << " busy_before_readmission=" << busy_polls << '\n';
			return std::move(next.ticket);
		}
		check(next.status == frame_status::busy && !next.ticket, "Invalid admission result");
		++busy_polls;
		check(clock_type::now() < deadline, "Admission did not return after retirement");
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}
std::array<std::uint8_t, 4> rgba(const readback_pixels &pixels, int x, int y) {
	const auto  row   = pixels.origin == readback_origin::top_left ? y : pixels.height - 1 - y;
	const auto  index = std::size_t(row) * pixels.row_stride + std::size_t(x) * 4;
	const auto &b     = pixels.bytes;
	if (pixels.layout == readback_layout::bgra8)
		return {b[index + 2], b[index + 1], b[index], b[index + 3]};
	return {b[index], b[index + 1], b[index + 2], b[index + 3]};
}
void verify(const readback_pixels &pixels, backend_kind backend) {
	check(pixels.width == width && pixels.height == height, "Wrong pixel extent");
	const bool metal = backend == backend_kind::metal;
	check(pixels.row_stride == (metal ? 256u : 252u), "Wrong native row stride");
	check(pixels.bytes.size() == pixels.row_stride * height, "Wrong padded byte length");
	check(pixels.layout == (metal ? readback_layout::bgra8 : readback_layout::rgba8),
	      "Wrong backend channel layout");
	check(pixels.origin == (metal ? readback_origin::top_left : readback_origin::bottom_left),
	      "Wrong backend row origin");
	unsigned checked = 0;
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			// Analytic clip-space oracle, excluding raster-edge boundary rules.
			const float cx = 2 * (float(x) + .5f) / width - 1;
			const float cy = 1 - 2 * (float(y) + .5f) / height;
			const float a  = std::max(std::abs(cx + .5f), std::abs(cy - .5f));
			const float b  = std::max(std::abs(cx - .5f), std::abs(cy + .5f));
			if (std::abs(a - .36f) < .04f || std::abs(b - .36f) < .04f)
				continue;
			const std::array<int, 4> expected = a < .36f   ? std::array<int, 4>{255, 0, 0, 255}
			                                    : b < .36f ? std::array<int, 4>{0, 128, 255, 255}
			                                               : std::array<int, 4>{32, 64, 96, 255};
			const auto               actual   = rgba(pixels, x, y);
			for (unsigned channel = 0; channel < 4; ++channel)
				check(std::abs(int(actual[channel]) - expected[channel]) <= 2,
				      "Pixel oracle mismatch at " + std::to_string(x) + "," + std::to_string(y) +
				          " channel " + std::to_string(channel));
			++checked;
		}
	}
	check(checked > 1000, "Insufficient interior pixels checked");
}
void write_pixels(const readback_pixels &pixels, const std::filesystem::path &output) {
	std::ofstream raw(output / "native.bin", std::ios::binary);
	raw.write(reinterpret_cast<const char *>(pixels.bytes.data()),
	          static_cast<std::streamsize>(pixels.bytes.size()));
	raw.close();
	check(bool(raw), "Cannot write native bytes");
	std::ofstream view(output / "top-rgb.ppm", std::ios::binary);
	view << "P6\n" << width << ' ' << height << "\n255\n";
	for (int y = 0; y < height; ++y)
		for (int x = 0; x < width; ++x) {
			const auto pixel = rgba(pixels, x, y);
			view.write(reinterpret_cast<const char *>(pixel.data()), 3);
		}
	view.close();
	check(bool(view), "Cannot write normalized preview");
	std::ofstream info(output / "packing.txt");
	info << "width=" << pixels.width << "\nheight=" << pixels.height
	     << "\nrow_stride=" << pixels.row_stride << "\nbytes=" << pixels.bytes.size()
	     << "\nlayout=" << (pixels.layout == readback_layout::bgra8 ? "bgra8" : "rgba8")
	     << "\norigin=" << (pixels.origin == readback_origin::top_left ? "top_left" : "bottom_left")
	     << "\npadding=unspecified; preserved in native.bin, excluded from pixel oracle\n";
	info.close();
	check(bool(info), "Cannot write packing record");
}
void ready_take(readback_ticket &ticket, backend_kind backend, const char *stage) {
	observation observed;
	await(ticket, readback_status::ready, observed);
	auto pixels = unwrap(ticket.take(), "take replacement");
	verify(pixels, backend);
	check(unwrap(ticket.poll(), "consumed poll") == readback_status::consumed && !ticket.take(),
	      "Replacement take was not single-use");
	record(stage, observed);
}
void exercise(backend_kind backend, const std::filesystem::path &root,
              const std::filesystem::path &output) {
	auto gpu = unwrap(device::create(backend), "device unavailable");
	std::cout << "backend=" << (backend == backend_kind::metal ? "metal" : "gl")
	          << " device=" << gpu->name() << " identity=" << gpu->identity() << std::endl;
	ice::io::resource_uri uri;
	std::string           error;
	if (!ice::io::resource_uri::from_path(
	        (root / (backend == backend_kind::metal ? "probe.metal" : "probe.glsl")).string(), uri,
	        error))
		throw std::runtime_error(error);
	ice::io::resource_resolver resolver;
	auto source = unwrap(shader_snapshot::read(resolver, uri.string()), "shader snapshot");
	std::cout << "shader_sha256=" << source.sha256() << '\n';
	auto       shader = unwrap(gpu->prepare_pipeline(source, "mesh_vertex_main", "mesh_fragment",
	                                                 {vertex_layout::position_normal, true, true, false}),
	                           "prepare existing mesh shader");
	mesh_data  square{{{{-.8f, -.8f, 0}, {0, 0, 1}},
	                   {{.8f, -.8f, 0}, {0, 0, 1}},
	                   {{.8f, .8f, 0}, {0, 0, 1}},
	                   {{-.8f, .8f, 0}, {0, 0, 1}}},
	                  {0, 1, 2, 0, 2, 3}};
	auto       geometry = unwrap(gpu->create_mesh(square), "create existing mesh shape");
	parameters upper, lower;
	lower.transform[12] = .5f;
	lower.transform[13] = -.5f;
	lower.tint          = {0, .5f, 1, 1};
	std::array draws{draw_item{shader.get(), geometry.get(), std::as_bytes(std::span(&upper, 1))},
	                 draw_item{shader.get(), geometry.get(), std::as_bytes(std::span(&lower, 1))}};
	const frame_batch frame{draws, {.125f, .25f, .375f, 1}};
	// The current native logical-peak formulas, not a host shipping policy or memory measurement.
	const std::size_t     cost = backend == backend_kind::metal
	                                 ? std::size_t(width) * height * 8 + 256u * height * 2
	                                 : std::size_t(width) * height * 16;
	const readback_limits limits{1, cost};
	readback_ticket       empty;
	check(!empty.poll() && !empty.take(), "Empty ticket did not diagnose");
	diagnostic_rejection(gpu->render_offscreen_async(frame, 0, height, limits), "zero-width");
	diagnostic_rejection(gpu->render_offscreen_async(frame, 4097, height, limits), "oversize");
	std::array invalid_draw{draw_item{}};
	diagnostic_rejection(gpu->render_offscreen_async({invalid_draw}, width, height, limits),
	                     "null-pipeline");
	// On a host missing actual async capabilities this valid call fails the campaign with its
	// real diagnostic. No injected loader, fake unavailable success, or synchronous fallback.
	auto        first = submit(*gpu, frame, limits);
	observation observed;
	const auto  initial = poll(first, observed);
	check(initial == readback_status::pending || initial == readback_status::ready,
	      "Unexpected initial status");
	busy(gpu->render_offscreen_async(frame, width, height, {1, cost * 2}), "ticket-pressure");
	busy(gpu->render_offscreen_async(frame, width, height, {2, cost * 2 - 1}), "byte-pressure");
	busy(gpu->render_offscreen_async(frame, width, height, {0, cost}), "disabled-ticket-bound");
	busy(gpu->render_offscreen_async(frame, width, height, {1, 0}), "disabled-byte-bound");
	await(first, readback_status::ready, observed);
	auto pixels = unwrap(first.take(), "take first");
	verify(pixels, backend);
	check(unwrap(first.poll(), "consumed poll") == readback_status::consumed && !first.take(),
	      "Take was not single-use");
	first = {};
	record("ready-take", observed);
	std::promise<void> writer_ready;
	auto               ready = writer_ready.get_future();
	std::jthread       writer(
        [owned = std::move(pixels), backend, &writer_ready, output](std::stop_token stop) {
            try {
                verify(owned, backend);
                write_pixels(owned, output);
                writer_ready.set_value();
            } catch (...) {
                writer_ready.set_exception(std::current_exception());
            }
            while (!stop.stop_requested())
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        });
	check(ready.wait_for(deadline_seconds) == std::future_status::ready, "Writer did not respond");
	ready.get();
	busy(gpu->render_offscreen_async(frame, width, height, limits), "writer-owned-pressure");
	writer.request_stop();
	writer.join(); // Only CPU writer; no GPU wait. Lambda destruction releases pixel admission.
	std::cout << "stage=writer-owned-pressure passed\n";
	auto cancelled = readmit(*gpu, frame, limits, "writer-release");
	cancelled.cancel();
	observation cancellation;
	await(cancelled, readback_status::cancelled, cancellation);
	check(!cancelled.take(), "Cancelled take unexpectedly yielded pixels");
	busy(gpu->render_offscreen_async(frame, width, height, limits), "cancelled-owner-pressure");
	cancelled = {};
	record("cancel", cancellation);
	{
		auto next = readmit(*gpu, frame, limits, "cancel-release");
		ready_take(next, backend, "post-cancel-pixels");
	}
	auto        dropped = readmit(*gpu, frame, limits, "pre-drop");
	observation drop;
	const auto  before_drop = poll(dropped, drop);
	check(before_drop == readback_status::pending || before_drop == readback_status::ready,
	      "Unexpected pre-drop status");
	dropped = {}; // No take: backend must retain any still-submitted native transfer.
	record("drop", drop);
	{
		auto next = readmit(*gpu, frame, limits, "drop-release");
		ready_take(next, backend, "post-drop-pixels");
	}
	const auto native_error = gpu->take_error();
	check(native_error.empty(), "Backend reported a native error: " + native_error);
	std::cout << "PASS native async acceptance; submitted=5; pending observations are reported, "
	             "not required or synthesized; no timing/performance claim\n";
}
} // namespace
int main(int argc, char **argv) {
	try {
		std::cout << std::unitbuf;
		check(
		    argc == 4,
		    "Usage: ice_async_readback_gpu_tests metal|gl existing-mesh-shader-dir new-output-dir");
		backend_kind      backend;
		const std::string selected = argv[1];
		if (selected == "metal") {
#ifdef ICE_TEST_METAL
			backend = backend_kind::metal;
#else
			throw std::runtime_error("Requested Metal backend is not compiled");
#endif
		} else if (selected == "gl") {
#ifdef ICE_TEST_OPENGL
			backend = backend_kind::opengl;
#else
			throw std::runtime_error("Requested desktop GL backend is not compiled");
#endif
		} else
			throw std::runtime_error("Explicit metal or gl backend required; GLES not covered");
		check(std::filesystem::create_directory(argv[3]), "Output directory must be new");
		exercise(backend, argv[2], argv[3]);
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "FAIL native async acceptance: " << error.what() << '\n';
		return 1;
	}
}
