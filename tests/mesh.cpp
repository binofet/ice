#include "ice/render/device.h"
#include "ice/render/live_pipeline.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <thread>

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::fprintf(stderr, "Mesh check failed %d: %s\n", __LINE__, #condition);              \
			return false;                                                                          \
		}                                                                                          \
	} while (0)

namespace {
using namespace ice::render;
struct parameters {
	std::array<float, 16> transform{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
	std::array<float, 4>  tint{1, 1, 1, 1}, light{0, 0, 1, 0};
};
auto      bytes(const parameters &input) { return std::as_bytes(std::span(&input, 1)); }
mesh_data square() {
	return {{{{0, 0, 0}, {1, 0, 0}},
	         {{-.8f, -.8f, 0}, {0, 0, 1}},
	         {{.8f, -.8f, 0}, {0, 0, 1}},
	         {{.8f, .8f, 0}, {0, 0, 1}},
	         {{-.8f, .8f, 0}, {0, 0, 1}}},
	        {1, 2, 3, 1, 3, 4}};
}
bool center(const image &input, std::array<int, 4> color) {
	auto offset = (std::size_t(input.height / 2) * input.width + input.width / 2) * 4;
	for (unsigned component = 0; component < 4; ++component)
		if (std::abs(int(input.rgba[offset + component]) - color[component]) > 2)
			return false;
	return true;
}
bool exercise(backend_kind backend, const std::filesystem::path &root, image &parity) {
	auto gpu = device::create(backend);
	CHECK(gpu);
	std::printf("Indexed mesh backend: %s\n", (*gpu)->name().c_str());
	ice::io::resource_uri uri;
	std::string           diagnostic;
	CHECK(ice::io::resource_uri::from_path(
	    (root / (backend == backend_kind::metal ? "probe.metal" : "probe.glsl")).string(), uri,
	    diagnostic));
	ice::io::resource_resolver resolver;
	auto                       source = shader_snapshot::read(resolver, uri.string());
	CHECK(source);
	pipeline_options scene_options{vertex_layout::position_normal, true, true, false};
	auto             scene =
	    (*gpu)->prepare_pipeline(*source, "mesh_vertex_main", "mesh_fragment", scene_options);
	CHECK(scene);
	auto overlay = (*gpu)->prepare_pipeline(*source, "overlay_vertex", "mesh_fragment",
	                                        {vertex_layout::procedural, false, false, true});
	CHECK(overlay);
	CHECK(!(*gpu)->prepare_pipeline(*source, "mesh_vertex_main", "mesh_fragment"));
	CHECK(!(*gpu)->prepare_pipeline(*source, "overlay_vertex", "mesh_fragment", scene_options));
	CHECK(!(*gpu)->prepare_pipeline(*source, "overlay_vertex", "mesh_fragment",
	                                {vertex_layout::procedural, false, true, false}));
	auto description = square();
	auto geometry    = (*gpu)->create_mesh(description);
	CHECK(geometry);
	description.vertices[1].position = {100, 100, 100};
	description.indices.clear(); // Upload is immutable, with no borrowed CPU aliases.
	CHECK(!(*gpu)->create_mesh(description));
	description            = square();
	description.indices[2] = 100;
	CHECK(!(*gpu)->create_mesh(description));
	description                       = square();
	description.vertices[2].normal[1] = std::numeric_limits<float>::quiet_NaN();
	CHECK(!(*gpu)->create_mesh(description));
	parameters near, far, ui;
	near.transform[14] = .25f;
	near.tint          = {0, 0, 1, 1};
	far.transform[14]  = .75f;
	far.tint           = {1, 0, 0, 1};
	ui.tint            = {0, .5f, 0, .5f};
	std::array items{draw_item{scene->get(), geometry->get(), bytes(near), nullptr},
	                 draw_item{scene->get(), geometry->get(), bytes(far), nullptr}};
	auto       clear_only = (*gpu)->render_offscreen(frame_batch{{}, {.1f, .2f, .3f, 1}}, 63, 31);
	CHECK(clear_only && center(*clear_only, {26, 51, 77, 255}));
	auto occluded = (*gpu)->render_offscreen(frame_batch{items}, 63, 31);
	CHECK(occluded && center(*occluded, {0, 0, 255, 255}));
	parity = *occluded;
	std::ranges::reverse(items);
	auto reversed = (*gpu)->render_offscreen(frame_batch{items}, 63, 31);
	CHECK(reversed && center(*reversed, {0, 0, 255, 255}));
	std::array mixed{items[0], items[1], draw_item{overlay->get(), nullptr, bytes(ui), nullptr}};
	auto       blended = (*gpu)->render_offscreen(frame_batch{mixed}, 63, 31);
	CHECK(blended && center(*blended, {0, 128, 128, 255}));
	auto text_pipeline = (*gpu)->prepare_pipeline(*source, "overlay_vertex", "overlay_texture",
	                                              {vertex_layout::procedural, false, false, true});
	CHECK(text_pipeline);
	image text_pixels{2, 2, {0, 128, 0, 128, 0, 128, 0, 128, 0, 128, 0, 128, 0, 128, 0, 128}};
	auto  text = (*gpu)->create_texture(text_pixels);
	CHECK(text);
	parameters text_uniforms = ui;
	text_uniforms.light      = {63, 31, 1, 0};
	std::array with_text{
	    items[0], items[1],
	    draw_item{text_pipeline->get(), nullptr, bytes(text_uniforms), text->get()}};
	auto terminal = (*gpu)->render_offscreen(frame_batch{with_text}, 63, 31);
	CHECK(terminal && center(*terminal, {0, 128, 127, 255}));
	parity     = *terminal;
	auto again = (*gpu)->render_offscreen(frame_batch{items}, 63, 31);
	CHECK(again &&
	      center(*again, {0, 0, 255, 255})); // Overlay depth mask must not poison next clear.
	auto untested = (*gpu)->prepare_pipeline(*source, "mesh_vertex_main", "mesh_fragment",
	                                         {vertex_layout::position_normal, false, false, false});
	CHECK(untested);
	std::array painter{draw_item{untested->get(), geometry->get(), bytes(near), nullptr},
	                   draw_item{untested->get(), geometry->get(), bytes(far), nullptr}};
	auto       painted = (*gpu)->render_offscreen(frame_batch{painter}, 63, 31);
	CHECK(painted && center(*painted, {255, 0, 0, 255}));
	auto no_write = (*gpu)->prepare_pipeline(*source, "mesh_vertex_main", "mesh_fragment",
	                                         {vertex_layout::position_normal, true, false, false});
	CHECK(no_write);
	std::array unwritten{draw_item{no_write->get(), geometry->get(), bytes(near), nullptr},
	                     draw_item{scene->get(), geometry->get(), bytes(far), nullptr}};
	auto       unwritten_depth = (*gpu)->render_offscreen(frame_batch{unwritten}, 63, 31);
	CHECK(unwritten_depth && center(*unwritten_depth, {255, 0, 0, 255}));
	parameters clipped    = near;
	clipped.transform[14] = -.25f;
	draw_item behind{scene->get(), geometry->get(), bytes(clipped), nullptr};
	auto      removed = (*gpu)->render_offscreen(frame_batch{std::span(&behind, 1)}, 63, 31);
	CHECK(removed && center(*removed, {0, 0, 0, 255}));
	auto normals = square();
	for (auto &vertex : normals.vertices)
		vertex.normal = {1, 0, 0};
	auto normal_mesh = (*gpu)->create_mesh(normals);
	CHECK(normal_mesh);
	parameters white;
	white.transform[14] = .5f;
	draw_item normal_draw{scene->get(), normal_mesh->get(), bytes(white), nullptr};
	auto normal_image = (*gpu)->render_offscreen(frame_batch{std::span(&normal_draw, 1)}, 63, 31);
	CHECK(normal_image && center(*normal_image, {64, 64, 64, 255}));
	auto foreign = device::create(backend);
	CHECK(foreign);
	auto foreign_mesh = (*foreign)->create_mesh(square());
	CHECK(foreign_mesh);
	auto invalid        = items;
	invalid[1].geometry = foreign_mesh->get();
	CHECK(!(*gpu)->render_offscreen(frame_batch{invalid}, 63, 31));
	invalid             = items;
	invalid[1].uniforms = {};
	CHECK(!(*gpu)->render_offscreen(frame_batch{invalid}, 63, 31));
	invalid             = items;
	invalid[1].pipeline = nullptr;
	CHECK(!(*gpu)->render_offscreen(frame_batch{invalid}, 63, 31));
	auto missing_text             = with_text;
	missing_text[2].sampled_image = nullptr;
	CHECK(!(*gpu)->render_offscreen(frame_batch{missing_text}, 63, 31));
	auto foreign_text = (*foreign)->create_texture(text_pixels);
	CHECK(foreign_text);
	missing_text[2].sampled_image = foreign_text->get();
	CHECK(!(*gpu)->render_offscreen(frame_batch{missing_text}, 63, 31));
	std::vector<draw_item> too_many(4097, items[0]);
	CHECK(!(*gpu)->render_offscreen(frame_batch{too_many}, 63, 31));
	std::vector<std::byte> block(4096);
	too_many.assign(1025, draw_item{scene->get(), geometry->get(), block, nullptr});
	CHECK(!(*gpu)->render_offscreen(frame_batch{too_many}, 63, 31));
	CHECK(!(*gpu)->render_offscreen(
	    frame_batch{items, {0, 0, 0, std::numeric_limits<float>::quiet_NaN()}}, 63, 31));
	live_pipeline live;
	CHECK(live.prepare(**gpu, *source, "mesh_vertex_main", "mesh_fragment", 0, bytes(near), nullptr,
	                   scene_options, geometry->get()));
	CHECK(live.activate(**gpu, 0));
	CHECK(!live.prepare(**gpu, *source, "mesh_vertex_main", "mesh_fragment", 1, bytes(near),
	                    nullptr, scene_options, foreign_mesh->get()));
	CHECK(live.revision() == 1 && live.current());
	draw_item retained{live.current(), geometry->get(), bytes(near), nullptr};
	CHECK((*gpu)->render_offscreen(frame_batch{std::span(&retained, 1)}, 63, 31));
	CHECK(live.prepare(**gpu, *source, "mesh_vertex_main", "mesh_fragment", 1, bytes(near), nullptr,
	                   {vertex_layout::position_normal, false, false, false}, geometry->get()));
	CHECK(live.activate(**gpu, 1));
	std::array live_items{draw_item{live.current(), geometry->get(), bytes(near), nullptr},
	                      draw_item{live.current(), geometry->get(), bytes(far), nullptr}};
	auto       live_painter = (*gpu)->render_offscreen(frame_batch{live_items}, 63, 31);
	CHECK(live_painter && center(*live_painter, {255, 0, 0, 255}));
	CHECK(live.undo(**gpu, 2));
	live_items[0].pipeline = live_items[1].pipeline = live.current();
	auto live_depth = (*gpu)->render_offscreen(frame_batch{live_items}, 63, 31);
	CHECK(live_depth && center(*live_depth, {0, 0, 255, 255}));
	// Two native drawables have independent VAOs/depth attachments while sharing immutable meshes.
	auto first_window =
	    ice::platform::desktop_window::create({"ICE indexed preview A", 160, 120, true});
	auto second_window =
	    ice::platform::desktop_window::create({"ICE indexed preview B", 160, 120, true});
	CHECK(first_window && second_window);
	auto first_surface  = (*gpu)->attach(**first_window);
	auto second_surface = (*gpu)->attach(**second_window);
	CHECK(first_surface && second_surface);
	std::array<unsigned, 2> presentations{};
	for (unsigned frame = 0; frame < 5; ++frame) {
		ice::platform::desktop_window::pump_events();
		auto first  = (*first_surface)->draw_batch(frame_batch{with_text});
		auto second = (*second_surface)->draw_batch(frame_batch{items});
		CHECK(first && second);
		presentations[0] += *first == frame_status::submitted;
		presentations[1] += *second == frame_status::submitted;
		std::this_thread::sleep_for(std::chrono::milliseconds(8));
	}
	CHECK(presentations[0] > 0 && presentations[1] > 0);
	invalid             = items;
	invalid[1].geometry = foreign_mesh->get();
	CHECK(!(*first_surface)->draw_batch(frame_batch{invalid}));
	CHECK((*first_surface)->draw_batch(frame_batch{with_text}));
	CHECK((*first_surface)->resize(0, 0));
	CHECK((*first_surface)->draw_batch(frame_batch{with_text}) == frame_status::unavailable);
	CHECK((*first_surface)->resize(96, 64));
	CHECK((*first_surface)->draw_batch(frame_batch{with_text}));
	CHECK((*first_surface)->wait_idle() && (*second_surface)->wait_idle());
	CHECK((*gpu)->take_error().empty());
	(*gpu).reset(); // Native resources/surfaces must retain the device independently.
	CHECK((*first_surface)->draw_batch(frame_batch{with_text}));
	CHECK((*first_surface)->wait_idle());
	return true;
}
bool replacement(const std::vector<backend_kind> &backends, const std::filesystem::path &root) {
	CHECK(!backends.empty());
	struct resources {
		std::unique_ptr<device>   gpu;
		std::unique_ptr<mesh>     geometry;
		std::unique_ptr<pipeline> shader;
	};
	const auto description = square();
	parameters values;
	values.tint          = {0, 0, 1, 1};
	values.transform[14] = .25f;
	auto prepare         = [&](backend_kind backend) -> ice::expected<resources, diagnostic> {
        auto gpu = device::create(backend);
        if (!gpu)
            return ice::unexpected(gpu.error());
        auto geometry = (*gpu)->create_mesh(description);
        if (!geometry)
            return ice::unexpected(geometry.error());
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
        auto shader = (*gpu)->prepare_pipeline(*snapshot, "mesh_vertex_main", "mesh_fragment",
		                                               {vertex_layout::position_normal, true, true, false});
        if (!shader)
            return ice::unexpected(shader.error());
        return resources{std::move(*gpu), std::move(*geometry), std::move(*shader)};
	};
	auto window =
	    ice::platform::desktop_window::create({"ICE indexed renderer handoff", 160, 120, true});
	CHECK(window);
	const auto identity = (*window)->id();
	auto       active   = prepare(backends[0]);
	CHECK(active);
	auto surface = active->gpu->attach(**window);
	CHECK(surface);
	unsigned                           simulation_steps = 0;
	std::vector<std::unique_ptr<mesh>> retired;
	for (unsigned iteration = 0; iteration < 4; ++iteration) {
		auto candidate = prepare(backends[(iteration + 1) % backends.size()]);
		CHECK(candidate);
		draw_item item{candidate->shader.get(), candidate->geometry.get(), bytes(values), nullptr};
		auto probe = candidate->gpu->render_offscreen(frame_batch{std::span(&item, 1)}, 63, 31);
		CHECK(probe && center(*probe, {0, 0, 255, 255}));
		draw_item previous{active->shader.get(), active->geometry.get(), bytes(values), nullptr};
		CHECK((*surface)->draw_batch(frame_batch{std::span(&previous, 1)}));
		CHECK((*surface)->wait_idle());
		surface->reset();
		auto attached = candidate->gpu->attach(**window);
		if (!attached) {
			surface = active->gpu->attach(**window);
			std::fprintf(stderr, "Indexed handoff attach failed: %s\n",
			             attached.error().message.c_str());
			return false;
		}
		surface = std::move(attached);
		retired.push_back(std::move(active->geometry));
		active = std::move(candidate);
		item   = {active->shader.get(), active->geometry.get(), bytes(values), nullptr};
		draw_item foreign{active->shader.get(), retired.back().get(), bytes(values), nullptr};
		CHECK(!(*surface)->draw_batch(frame_batch{std::span(&foreign, 1)}));
		unsigned presentations = 0;
		for (unsigned frame = 0; frame < 3; ++frame) {
			ice::platform::desktop_window::pump_events();
			auto submitted = (*surface)->draw_batch(frame_batch{std::span(&item, 1)});
			CHECK(submitted);
			presentations += *submitted == frame_status::submitted;
			++simulation_steps;
			std::this_thread::sleep_for(std::chrono::milliseconds(8));
		}
		CHECK(presentations > 0 && (*window)->id() == identity);
		CHECK(simulation_steps == (iteration + 1) * 3 && active->gpu->take_error().empty());
	}
	CHECK((*surface)->wait_idle());
	std::printf("Indexed renderer replacements:4; retained window%u and CPU simulation%u\n",
	            unsigned(identity), simulation_steps);
	return true;
}
} // namespace
int main(int argc, char **argv) {
	auto               root = std::filesystem::absolute(argc > 1 ? argv[1] : "tests/data/mesh");
	std::vector<image> results;
	std::vector<backend_kind> backends;
#ifdef ICE_TEST_METAL
	backends.push_back(backend_kind::metal);
	results.emplace_back();
	if (!exercise(ice::render::backend_kind::metal, root, results.back()))
		return 1;
#endif
#ifdef ICE_TEST_OPENGL
	backends.push_back(backend_kind::opengl);
	results.emplace_back();
	if (!exercise(ice::render::backend_kind::opengl, root, results.back()))
		return 1;
#endif
	if (results.size() == 2) {
		std::size_t different = 0;
		for (std::size_t index = 0; index < results[0].rgba.size(); ++index)
			if (std::abs(int(results[0].rgba[index]) - int(results[1].rgba[index])) > 2)
				++different;
		if (different) {
			std::fprintf(stderr, "Mesh parity mismatch: %zu channels\n", different);
			return 1;
		}
	}
	if (!replacement(backends, root))
		return 1;
	std::puts("Indexed meshes passed: immutable indexed data, normals, depth/order, mixed "
	          "premultiplied overlay, foreign/bad batches and live preparation");
}
