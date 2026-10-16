#include "ice/render/device.h"
#include "ice/render/live_pipeline.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <thread>
using namespace ice::render;
#define CHECK(expression)                                                                          \
	do {                                                                                           \
		if (!(expression)) {                                                                       \
			std::fprintf(stderr, "HDR failed %d: %s\n", __LINE__, #expression);                    \
			return false;                                                                          \
		}                                                                                          \
	} while (0)
namespace {
std::span<const std::byte> bytes(const std::array<float, 4> &values) {
	return std::as_bytes(std::span(values));
}
bool pixel(const image &capture, int x, int y, std::array<int, 4> expected, int tolerance = 2) {
	for (unsigned channel = 0; channel < 4; ++channel)
		if (std::abs(int(capture.rgba[(std::size_t(y) * capture.width + x) * 4 + channel]) -
		             expected[channel]) > tolerance) {
			std::fprintf(stderr, "Pixel %d,%d channel%u actual%u expected%d\n", x, y, channel,
			             capture.rgba[(std::size_t(y) * capture.width + x) * 4 + channel],
			             expected[channel]);
			return false;
		}
	return true;
}
bool exercise(backend_kind kind, const std::filesystem::path &root, image &comparison) {
	auto gpu = device::create(kind), foreign = device::create(kind);
	CHECK(gpu && foreign);
	std::printf("HDR device: %s\n", (*gpu)->name().c_str());
	ice::io::resource_uri uri;
	std::string           error;
	CHECK(ice::io::resource_uri::from_path(
	    (root / (kind == backend_kind::metal ? "probe.metal" : "probe.glsl")).string(), uri,
	    error));
	ice::io::resource_resolver resolver;
	auto                       snapshot = shader_snapshot::read(resolver, uri.string());
	CHECK(snapshot);
	auto prepare = [&](std::string entry, color_format format, std::uint8_t slots,
	                   std::uint8_t depth = 0, bool write = false) {
		pipeline_options options;
		options.depth_test             = write;
		options.depth_write            = write;
		options.target_format          = format;
		options.sampled_bindings       = slots;
		options.depth_sampled_bindings = depth;
		return (*gpu)->prepare_pipeline(*snapshot, "fullscreen", entry, options);
	};
	auto scene     = prepare("scene_hdr", color_format::rgba16_float, 0, 0, true);
	auto threshold = prepare("threshold", color_format::rgba16_float, 1);
	auto blur      = prepare("blur", color_format::rgba16_float, 1);
	auto tone      = prepare("tone", color_format::rgba8_unorm, 1);
	auto composite = prepare("composite", color_format::rgba8_unorm, 3);
	auto depth     = prepare("depth_probe", color_format::rgba8_unorm, 4, 4);
	auto depth_hdr = prepare("depth_probe", color_format::rgba16_float, 4, 4);
	auto all       = prepare("bindings", color_format::rgba8_unorm, 15);
	if (!scene || !threshold || !blur || !tone || !composite || !depth || !depth_hdr || !all) {
		for (auto *result :
		     {&scene, &threshold, &blur, &tone, &composite, &depth, &depth_hdr, &all})
			if (!*result)
				std::fprintf(stderr, "%s\n", result->error().message.c_str());
		return false;
	}
	CHECK(!prepare("composite", color_format::rgba8_unorm, 1));
	CHECK(!prepare("tone", color_format::rgba8_unorm, 0));
	CHECK(!prepare("tone", color_format::rgba8_unorm, 0xff, 1));
	CHECK(!prepare("tone", static_cast<color_format>(99), 1));
	CHECK(!(*gpu)->create_render_target({0, 31}));
	CHECK(!(*gpu)->create_render_target({63, 16385}));
	CHECK(!(*gpu)->create_render_target({63, 31, static_cast<color_format>(99)}));
	auto target         = (*gpu)->create_render_target({63, 31});
	auto bloom          = (*gpu)->create_render_target({31, 15});
	auto ping           = (*gpu)->create_render_target({31, 15});
	auto ldr            = (*gpu)->create_render_target({63, 31, color_format::rgba8_unorm});
	auto foreign_target = (*foreign)->create_render_target({63, 31});
	CHECK(target && bloom && ping && ldr && foreign_target);
	CHECK((*target)->description().width == 63 && (*target)->description().height == 31 &&
	      (*target)->description().format == color_format::rgba16_float);
	const std::array<float, 4> zero{};
	draw_item                  uninitialized{tone->get(), nullptr, bytes(zero)};
	uninitialized.sampled_images[0] = &(*target)->color();
	CHECK(!(*gpu)->render_offscreen(frame_batch{std::span(&uninitialized, 1)}, 7, 5));
	draw_item scene_item{scene->get(), nullptr, bytes(zero)};
	auto      first_pass = (*gpu)->render_into(**target, frame_batch{std::span(&scene_item, 1)});
	if (!first_pass)
		std::fprintf(stderr, "First HDR pass: %s\n", first_pass.error().message.c_str());
	CHECK(first_pass == frame_status::submitted);
	draw_item tone_item{tone->get(), nullptr, bytes(zero)};
	tone_item.sampled_images[0] = &(*target)->color();
	auto original = (*gpu)->render_offscreen(frame_batch{std::span(&tone_item, 1)}, 63, 31);
	CHECK(original && pixel(*original, 31, 3, {204, 85, 28, 255}) &&
	      pixel(*original, 31, 27, {51, 170, 227, 255}));
	// A clamped UNORM intermediate would tone-map red4 to128, not204.
	draw_item depth_item{depth->get(), nullptr, bytes(zero)};
	depth_item.sampled_images[2] = &(*target)->depth();
	auto depth_pixels = (*gpu)->render_offscreen(frame_batch{std::span(&depth_item, 1)}, 63, 31);
	CHECK(depth_pixels && pixel(*depth_pixels, 31, 3, {64, 64, 64, 255}) &&
	      pixel(*depth_pixels, 31, 27, {191, 191, 191, 255}));
	draw_item threshold_item{threshold->get(), nullptr, bytes(zero)};
	threshold_item.sampled_images[0] = &(*target)->color();
	CHECK((*gpu)->render_into(**bloom, frame_batch{std::span(&threshold_item, 1)}) ==
	      frame_status::submitted);
	std::array<float, 4> offset{1.f / 31, 0, 0, 0};
	draw_item            blur_item{blur->get(), nullptr, bytes(offset)};
	blur_item.sampled_images[0] = &(*bloom)->color();
	CHECK((*gpu)->render_into(**ping, frame_batch{std::span(&blur_item, 1)}) ==
	      frame_status::submitted);
	offset                      = {0, 1.f / 15, 0, 0};
	blur_item.uniforms          = bytes(offset);
	blur_item.sampled_images[0] = &(*ping)->color();
	CHECK((*gpu)->render_into(**bloom, frame_batch{std::span(&blur_item, 1)}) ==
	      frame_status::submitted);
	draw_item composite_item{composite->get(), nullptr, bytes(zero)};
	composite_item.sampled_images[0] = &(*target)->color();
	composite_item.sampled_images[1] = &(*bloom)->color();
	auto result = (*gpu)->render_offscreen(frame_batch{std::span(&composite_item, 1)}, 63, 31);
	CHECK(result && pixel(*result, 31, 3, {216, 85, 28, 255}) &&
	      pixel(*result, 31, 27, {51, 182, 235, 255}));
	CHECK(result->rgba != original->rgba);
	comparison = *result;
	// Four distinct resources prove fixed slots cannot silently alias unit zero.
	std::array<std::unique_ptr<texture>, 4>          images;
	const std::array<std::array<std::uint8_t, 4>, 4> values{
	    {{64, 0, 0, 255}, {0, 128, 0, 255}, {0, 0, 192, 255}, {255, 255, 255, 255}}};
	draw_item all_item{all->get(), nullptr, bytes(zero)};
	for (unsigned slot = 0; slot < 4; ++slot) {
		auto image = (*gpu)->create_texture({1, 1, {values[slot].begin(), values[slot].end()}});
		CHECK(image);
		images[slot]                  = std::move(*image);
		all_item.sampled_images[slot] = images[slot].get();
	}
	auto slots = (*gpu)->render_offscreen(frame_batch{std::span(&all_item, 1)}, 7, 5);
	CHECK(slots && pixel(*slots, 3, 2, {64, 128, 192, 255}));
	for (unsigned slot = 0; slot < 4; ++slot) {
		auto missing                 = all_item;
		missing.sampled_images[slot] = nullptr;
		CHECK(!(*gpu)->render_offscreen(frame_batch{std::span(&missing, 1)}, 7, 5));
	}
	auto invalid              = threshold_item;
	invalid.sampled_images[0] = &(*target)->color();
	CHECK(!(*gpu)->render_into(**target, frame_batch{std::span(&invalid, 1)}));
	invalid                   = {depth_hdr->get(), nullptr, bytes(zero)};
	invalid.sampled_images[2] = &(*target)->depth();
	CHECK(!(*gpu)->render_into(**target, frame_batch{std::span(&invalid, 1)}));
	invalid                   = depth_item;
	invalid.sampled_images[2] = &(*target)->color();
	CHECK(!(*gpu)->render_offscreen(frame_batch{std::span(&invalid, 1)}, 7, 5));
	invalid                   = tone_item;
	invalid.sampled_images[0] = &(*target)->depth();
	CHECK(!(*gpu)->render_offscreen(frame_batch{std::span(&invalid, 1)}, 7, 5));
	invalid               = tone_item;
	invalid.sampled_image = images[0].get();
	CHECK(!(*gpu)->render_offscreen(frame_batch{std::span(&invalid, 1)}, 7, 5));
	invalid                   = tone_item;
	invalid.sampled_images[0] = &(*foreign_target)->color();
	CHECK(!(*gpu)->render_offscreen(frame_batch{std::span(&invalid, 1)}, 7, 5));
	CHECK(!(*gpu)->render_into(**foreign_target, frame_batch{std::span(&scene_item, 1)}));
	CHECK(!(*gpu)->render_into(**ldr, frame_batch{std::span(&scene_item, 1)}));
	CHECK(!(*gpu)->render_into(**target, frame_batch{std::span(&tone_item, 1)}));
	CHECK(!(*gpu)->render_offscreen(frame_batch{std::span(&scene_item, 1)}, 7, 5));
	auto foreign_shader = (*foreign)->prepare_pipeline(
	    *snapshot, "fullscreen", "scene_hdr",
	    {vertex_layout::procedural, true, true, false, color_format::rgba16_float, 0, 0});
	CHECK(foreign_shader);
	auto overwriting              = threshold_item;
	overwriting.sampled_images[0] = &(*bloom)->color();
	std::array<draw_item, 2> bad_batch{overwriting,
	                                   draw_item{foreign_shader->get(), nullptr, bytes(zero)}};
	CHECK(!(*gpu)->render_into(**target, frame_batch{bad_batch, {1, 1, 1, 1}}));
	bad_batch[1] = {threshold->get(), nullptr, bytes(zero)}; // Missing its required sampled input.
	CHECK(!(*gpu)->render_into(**target, frame_batch{bad_batch, {1, 1, 1, 1}}));
	auto after = (*gpu)->render_offscreen(frame_batch{std::span(&tone_item, 1)}, 63, 31);
	CHECK(after && after->rgba == original->rgba);
	// Submitted consumer commands retain a producer's native storage after its C++ owner retires.
	{
		auto ephemeral = (*gpu)->create_render_target({47, 29});
		CHECK(ephemeral);
		CHECK((*gpu)->render_into(**ephemeral, frame_batch{std::span(&scene_item, 1)}) ==
		      frame_status::submitted);
		auto consuming              = threshold_item;
		consuming.sampled_images[0] = &(*ephemeral)->color();
		CHECK((*gpu)->render_into(**ping, frame_batch{std::span(&consuming, 1)}) ==
		      frame_status::submitted);
	}
	auto retained              = tone_item;
	retained.sampled_images[0] = &(*ping)->color();
	auto retained_pixels = (*gpu)->render_offscreen(frame_batch{std::span(&retained, 1)}, 47, 29);
	CHECK(retained_pixels && pixel(*retained_pixels, 23, 2, {191, 0, 0, 255}) &&
	      pixel(*retained_pixels, 23, 26, {0, 128, 223, 255}));
	// Transactional HDR preparation probes the matching format and all supplied slots.
	live_pipeline    live;
	pipeline_options hdr_options{vertex_layout::procedural,  true, true, false,
	                             color_format::rgba16_float, 0,    0};
	CHECK(live.prepare(**gpu, *snapshot, "fullscreen", "scene_hdr", 0, bytes(zero), nullptr,
	                   hdr_options));
	CHECK(live.activate(**gpu, 0) == 1);
	auto                           active = live.current();
	pipeline_options               sample_options{vertex_layout::procedural,  false, false, false,
                                    color_format::rgba16_float, 1,     0};
	std::array<const texture *, 4> views{&(*target)->color(), nullptr, nullptr, nullptr};
	CHECK(live.prepare(**gpu, *snapshot, "fullscreen", "threshold", 1, bytes(zero), nullptr,
	                   sample_options, nullptr, views));
	CHECK(!live.prepare(**gpu, *snapshot, "missing", "threshold", 1, bytes(zero), nullptr,
	                    sample_options, nullptr, views));
	sample_options.sampled_bindings = 3;
	CHECK(!live.prepare(**gpu, *snapshot, "fullscreen", "composite", 1, bytes(zero), nullptr,
	                    sample_options, nullptr, views));
	CHECK(live.revision() == 1 && live.current() == active && live.has_prepared());
	CHECK(!live.prepare(**foreign, *snapshot, "fullscreen", "scene_hdr", 1, bytes(zero), nullptr,
	                    hdr_options));
	CHECK(live.activate(**gpu, 1) == 2 && live.can_undo());
	draw_item live_item{live.current(), nullptr, bytes(zero), nullptr, views};
	CHECK((*gpu)->render_into(**ping, frame_batch{std::span(&live_item, 1)}) ==
	      frame_status::submitted);
	CHECK(live.undo(**gpu, 2) == 3 && live.current() == active);
	live_item = {live.current(), nullptr, bytes(zero)};
	CHECK((*gpu)->render_into(**target, frame_batch{std::span(&live_item, 1)}) ==
	      frame_status::submitted);
	CHECK((*gpu)->wait_idle());
	// The user's ultrawide display exceeds CPU capture limits; only the final small probe reads
	// back.
	{
		auto ultrawide = (*gpu)->create_render_target({6144, 2560});
		CHECK(ultrawide);
		CHECK((*gpu)->render_into(**ultrawide, frame_batch{std::span(&scene_item, 1)}) ==
		      frame_status::submitted);
		auto view              = tone_item;
		view.sampled_images[0] = &(*ultrawide)->color();
		auto capture           = (*gpu)->render_offscreen(frame_batch{std::span(&view, 1)}, 63, 31);
		CHECK(capture && pixel(*capture, 31, 3, {204, 85, 28, 255}) &&
		      pixel(*capture, 31, 27, {51, 170, 227, 255}));
		std::puts("HDR ultrawide6144x2560 retained linear radiance and sampled orientation");
	}
	// Graph resize prepares all replacement owners before publication; old views stay usable.
	for (int size : {13, 31, 47}) {
		auto resized = (*gpu)->create_render_target({size, size - 4});
		CHECK(resized);
		CHECK((*gpu)->render_into(**resized, frame_batch{std::span(&scene_item, 1)}) ==
		      frame_status::submitted);
		auto resized_item              = tone_item;
		resized_item.sampled_images[0] = &(*resized)->color();
		auto capture =
		    (*gpu)->render_offscreen(frame_batch{std::span(&resized_item, 1)}, size, size - 4);
		CHECK(capture && pixel(*capture, size / 2, 1, {204, 85, 28, 255}));
	}
	auto window = ice::platform::desktop_window::create({"ICE HDR pass proof", 160, 120, true});
	auto second_window =
	    ice::platform::desktop_window::create({"ICE HDR second preview", 120, 96, true});
	CHECK(window && second_window);
	auto surface = (*gpu)->attach(**window), second_surface = (*gpu)->attach(**second_window);
	CHECK(surface && second_surface);
	unsigned submitted = 0;
	for (unsigned frame = 0; frame < 8; ++frame) {
		ice::platform::desktop_window::pump_events();
		CHECK((*gpu)->render_into(**target, frame_batch{std::span(&scene_item, 1)}) ==
		      frame_status::submitted);
		auto first  = (*surface)->draw_batch(frame_batch{std::span(&composite_item, 1)});
		auto second = (*second_surface)->draw_batch(frame_batch{std::span(&tone_item, 1)});
		CHECK(first && second);
		submitted += *first == frame_status::submitted;
		std::this_thread::sleep_for(std::chrono::milliseconds(8));
	}
	CHECK(submitted > 0 && (*surface)->wait_idle() && (*second_surface)->wait_idle());
	CHECK(!(*surface)->draw_batch(frame_batch{std::span(&scene_item, 1)}));
	CHECK((*surface)->resize(0, 0) && (*surface)->draw_batch(frame_batch{
	                                      std::span(&tone_item, 1)}) == frame_status::unavailable);
	CHECK((*surface)->resize(96, 64));
	CHECK((*gpu)->take_error().empty());
	(*gpu).reset();
	CHECK((*surface)->draw_batch(frame_batch{std::span(&tone_item, 1)}) && (*surface)->wait_idle());
	return true;
}
bool replacement(const std::vector<backend_kind> &backends, const std::filesystem::path &root) {
	CHECK(!backends.empty());
	struct resources {
		std::unique_ptr<device>        gpu;
		std::unique_ptr<render_target> target;
		std::unique_ptr<pipeline>      scene, tone;
	};
	auto prepare = [&](backend_kind backend) -> ice::expected<resources, diagnostic> {
		auto gpu = device::create(backend);
		if (!gpu)
			return ice::unexpected(gpu.error());
		ice::io::resource_uri uri;
		std::string           error;
		if (!ice::io::resource_uri::from_path(
		        (root / (backend == backend_kind::metal ? "probe.metal" : "probe.glsl")).string(),
		        uri, error))
			return ice::unexpected(diagnostic{error});
		ice::io::resource_resolver resolver;
		auto                       snapshot = shader_snapshot::read(resolver, uri.string());
		if (!snapshot)
			return ice::unexpected(snapshot.error());
		auto target = (*gpu)->create_render_target({47, 29});
		if (!target)
			return ice::unexpected(target.error());
		auto scene = (*gpu)->prepare_pipeline(
		    *snapshot, "fullscreen", "scene_hdr",
		    {vertex_layout::procedural, true, true, false, color_format::rgba16_float, 0, 0});
		if (!scene)
			return ice::unexpected(scene.error());
		auto tone = (*gpu)->prepare_pipeline(
		    *snapshot, "fullscreen", "tone",
		    {vertex_layout::procedural, false, false, false, color_format::rgba8_unorm, 1, 0});
		if (!tone)
			return ice::unexpected(tone.error());
		return resources{std::move(*gpu), std::move(*target), std::move(*scene), std::move(*tone)};
	};
	auto window =
	    ice::platform::desktop_window::create({"ICE HDR renderer handoff", 160, 120, true});
	CHECK(window);
	const auto identity = (*window)->id();
	auto       active   = prepare(backends.front());
	CHECK(active);
	auto surface = active->gpu->attach(**window);
	CHECK(surface);
	const std::array<float, 4>                  zero{};
	unsigned                                    simulation_steps = 0;
	std::vector<std::unique_ptr<render_target>> retired;
	for (unsigned iteration = 0; iteration < 4; ++iteration) {
		auto candidate = prepare(backends[(iteration + 1) % backends.size()]);
		CHECK(candidate);
		draw_item scene{candidate->scene.get(), nullptr, bytes(zero)};
		CHECK(candidate->gpu->render_into(*candidate->target, frame_batch{std::span(&scene, 1)}) ==
		      frame_status::submitted);
		draw_item tone{candidate->tone.get(), nullptr, bytes(zero)};
		tone.sampled_images[0] = &candidate->target->color();
		auto probe = candidate->gpu->render_offscreen(frame_batch{std::span(&tone, 1)}, 47, 29);
		CHECK(probe && pixel(*probe, 23, 2, {204, 85, 28, 255}));
		CHECK((*surface)->wait_idle());
		surface->reset();
		auto attached = candidate->gpu->attach(**window);
		if (!attached) {
			surface = active->gpu->attach(**window);
			std::fprintf(stderr, "HDR replacement attach: %s\n", attached.error().message.c_str());
			return false;
		}
		surface = std::move(attached);
		retired.push_back(std::move(active->target));
		active                    = std::move(candidate);
		auto foreign              = tone;
		foreign.sampled_images[0] = &retired.back()->color();
		CHECK(!(*surface)->draw_batch(frame_batch{std::span(&foreign, 1)}));
		unsigned submitted = 0;
		for (unsigned frame = 0; frame < 3; ++frame) {
			ice::platform::desktop_window::pump_events();
			auto draw = (*surface)->draw_batch(frame_batch{std::span(&tone, 1)});
			CHECK(draw);
			submitted += *draw == frame_status::submitted;
			++simulation_steps;
			std::this_thread::sleep_for(std::chrono::milliseconds(8));
		}
		CHECK(submitted && (*window)->id() == identity && simulation_steps == (iteration + 1) * 3);
		CHECK(active->gpu->wait_idle() && active->gpu->take_error().empty());
	}
	CHECK((*surface)->wait_idle());
	std::printf("HDR renderer replacements4; retained window%u and CPU simulation%u\n",
	            unsigned(identity), simulation_steps);
	return true;
}
} // namespace
int main(int argc, char **argv) {
	auto               root = std::filesystem::absolute(argc > 1 ? argv[1] : "tests/data/hdr");
	std::vector<image> results;
	std::vector<backend_kind> backends;
#ifdef ICE_TEST_METAL
	backends.push_back(backend_kind::metal);
	results.emplace_back();
	if (!exercise(backend_kind::metal, root, results.back()))
		return 1;
#endif
#ifdef ICE_TEST_OPENGL
	backends.push_back(backend_kind::opengl);
	results.emplace_back();
	if (!exercise(backend_kind::opengl, root, results.back()))
		return 1;
#endif
	if (results.size() == 2) {
		if (results[0].rgba.size() != results[1].rgba.size())
			return 1;
		unsigned maximum = 0;
		for (std::size_t index = 0; index < results[0].rgba.size(); ++index)
			maximum = std::max(maximum, unsigned(std::abs(int(results[0].rgba[index]) -
			                                              int(results[1].rgba[index]))));
		if (maximum > 3) {
			std::fprintf(stderr, "HDR parity maximum channel difference%u\n", maximum);
			for (int y = 0; y < results[0].height; ++y) {
				auto at = (std::size_t(y) * results[0].width + results[0].width / 2) * 4;
				if (!std::equal(results[0].rgba.begin() + at, results[0].rgba.begin() + at + 4,
				                results[1].rgba.begin() + at))
					std::fprintf(stderr, "row%d Metal%u,%u,%u GL%u,%u,%u\n", y, results[0].rgba[at],
					             results[0].rgba[at + 1], results[0].rgba[at + 2],
					             results[1].rgba[at], results[1].rgba[at + 1],
					             results[1].rgba[at + 2]);
			}
			return 1;
		}
		std::printf("HDR Metal/OpenGL final parity maximum channel difference%u/255\n", maximum);
	}
	if (!replacement(backends, root))
		return 1;
	std::puts("HDR passed: retained radiance, four slots, depth/orientation, GPU bloom passes, "
	          "validation, resize, native previews");
	return 0;
}
