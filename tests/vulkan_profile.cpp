#include "profile.h"
#include "submission.h"
#include <cstdio>
#include <stdexcept>
#include <string>
using namespace ice::render;
namespace vk = ice::render::vulkan::detail;
#define CHECK(x)                                                                                   \
	do {                                                                                           \
		if (!(x))                                                                                  \
			throw std::runtime_error("line " + std::to_string(__LINE__) + ": " #x);                \
	} while (false)
// Instruction fixtures exercise the actual profile reader only, not SPIR-V validity or GPU work.
std::vector<std::uint32_t> stage(std::uint32_t model, bool push = true) {
	std::vector<std::uint32_t> w{0x07230203,      0x00010300, 0, 20,         0, (2u << 16) | 17, 1,
	                             (5u << 16) | 15, model,      1, 0x6e69616d, 0};
	if (push) {
		const std::vector<std::uint32_t> types{(3u << 16) | 22,
		                                       2,
		                                       32,
		                                       (4u << 16) | 23,
		                                       3,
		                                       2,
		                                       4,
		                                       (4u << 16) | 24,
		                                       4,
		                                       3,
		                                       4,
		                                       (4u << 16) | 30,
		                                       5,
		                                       4,
		                                       3,
		                                       (4u << 16) | 32,
		                                       6,
		                                       9,
		                                       5,
		                                       (4u << 16) | 59,
		                                       6,
		                                       7,
		                                       9,
		                                       (3u << 16) | 71,
		                                       5,
		                                       2,
		                                       (5u << 16) | 72,
		                                       5,
		                                       0,
		                                       35,
		                                       0,
		                                       (5u << 16) | 72,
		                                       5,
		                                       1,
		                                       35,
		                                       64,
		                                       (5u << 16) | 72,
		                                       5,
		                                       0,
		                                       7,
		                                       16,
		                                       (4u << 16) | 72,
		                                       5,
		                                       0,
		                                       5};
		w.insert(w.end(), types.begin(), types.end());
	}
	return w;
}
std::string packet(const std::vector<std::uint32_t> &v, const std::vector<std::uint32_t> &f) {
	std::string text = "ice-vulkan-spirv-v1\n";
	for (bool vertex : {true, false}) {
		const auto &words = vertex ? v : f;
		text += std::string(vertex ? "vertex " : "fragment ") + std::to_string(words.size()) + "\n";
		for (auto word : words) {
			char hex[10];
			std::snprintf(hex, sizeof(hex), "%08x ", unsigned(word));
			text += hex;
		}
		text += '\n';
	}
	return text;
}
void submission_obligations() {
	enum class outcome {
		success,
		suboptimal,
		not_ready,
		timeout,
		out_of_date,
		surface_lost,
		host_oom,
		device_oom,
		device_lost,
		unknown
	};
	struct oracle {
		outcome result;
		bool    queue_wait, acquire_wait;
	};
	const oracle cases[]{{outcome::success, true, true},      {outcome::suboptimal, true, true},
	                     {outcome::not_ready, true, false},   {outcome::timeout, true, false},
	                     {outcome::out_of_date, true, false}, {outcome::surface_lost, true, false},
	                     {outcome::host_oom, false, false},   {outcome::device_oom, false, false},
	                     {outcome::device_lost, true, false}, {outcome::unknown, true, false}};
	for (const auto &test : cases) {
		bool     pending = false;
		unsigned calls   = 0;
		auto     submit  = [&] {
            CHECK(pending);
            ++calls;
            return test.result;
		};
		CHECK(vk::queue_with_obligation(pending, submit, outcome::host_oom, outcome::device_oom) ==
		      test.result);
		CHECK(calls == 1 && pending == test.queue_wait);
		// An OOM fence is still reset/unsignaled. An attempted drain would never finish;
		// the actual shared adapter must make that fence absent from the wait set.
		if (test.result == outcome::host_oom || test.result == outcome::device_oom) {
			const bool reset_fence_signaled = false;
			const bool drain_can_finish     = !pending || reset_fence_signaled;
			CHECK(drain_can_finish);
		}
		pending = false;
		calls   = 0;
		CHECK(vk::acquire_with_obligation(pending, submit, outcome::success, outcome::suboptimal) ==
		      test.result);
		CHECK(calls == 1 && pending == test.acquire_wait);
	}
	for (bool queue : {true, false}) {
		bool pending = false, threw = false;
		auto uncertain = [&]() -> outcome {
			CHECK(pending);
			throw std::runtime_error("no typed result");
		};
		try {
			if (queue)
				vk::queue_with_obligation(pending, uncertain, outcome::host_oom,
				                          outcome::device_oom);
			else
				vk::acquire_with_obligation(pending, uncertain, outcome::success,
				                            outcome::suboptimal);
		} catch (const std::runtime_error &) {
			threw = true;
		}
		CHECK(threw && pending);
	}
}
int main() try {
	submission_obligations();
	auto v = stage(0), f = stage(4);
	auto p = vk::parse(packet(v, f), "main", "main");
	CHECK(p && p->pushes && p->vertex == v && p->fragment == f);
	CHECK(vk::parse(packet(stage(0, false), stage(4, false)), "main", "main") &&
	      !vk::parse(packet(stage(0, false), stage(4, false)), "main", "main")->pushes);
	CHECK(!vk::parse(packet(v, f), "other", "main"));
	CHECK(!vk::parse(packet(f, v), "main", "main"));
	CHECK(!vk::parse(packet(v, f) + " extra", "main", "main"));
	CHECK(!vk::parse("ice-vulkan-spirv-v1 vertex 32769", "main", "main"));
	CHECK(!vk::parse("ice-vulkan-spirv-v1 vertex", "main", "main"));
	for (auto index : {0u, 1u, 3u, 4u, 5u}) {
		auto bad   = v;
		bad[index] = 0xffffffff;
		CHECK(!vk::parse(packet(bad, f), "main", "main"));
	}
	auto reserved_version = v;
	reserved_version[1]   = 0x00010101;
	CHECK(!vk::parse(packet(reserved_version, f), "main", "main"));
	for (std::uint32_t storage : {0u, 2u, 4u, 5u, 8u, 10u, 11u, 12u}) {
		auto bad = v;
		bad.insert(bad.end(), {(4u << 16) | 59, 6, 8, storage});
		CHECK(!vk::parse(packet(bad, f), "main", "main"));
	}
	for (std::uint32_t offset : {0u, 60u, 68u, 0xffffffffu}) {
		auto bad = v;
		for (std::size_t at = 5; at < bad.size(); at += bad[at] >> 16)
			if ((bad[at] & 65535) == 72 && bad[at + 2] == 1 && bad[at + 3] == 35)
				bad[at + 4] = offset;
		CHECK(!vk::parse(packet(bad, f), "main", "main"));
	}
	auto row   = v;
	row.back() = 4;
	CHECK(!vk::parse(packet(row, f), "main", "main"));
	auto capability = v;
	capability[6]   = 2;
	CHECK(!vk::parse(packet(capability, f), "main", "main"));
	// Both indexed input locations are float3; foreign layouts/locations/types refuse.
	auto input = v;
	input.insert(input.end(), {(4u << 16) | 23, 8, 2, 3, (4u << 16) | 32, 9, 1, 8, (4u << 16) | 59,
	                           9, 10, 1, (4u << 16) | 71, 10, 30, 0});
	CHECK(vk::parse(packet(input, f), "main", "main"));
	CHECK(!vk::parse(packet(input, f), "main", "main", vertex_layout::procedural));
	input.back() = 2;
	CHECK(!vk::parse(packet(input, f), "main", "main"));
	pipeline_options options;
	options.sampled_bindings = 0;
	CHECK(vk::supported(options));
	options.vertices = vertex_layout::position_normal;
	CHECK(vk::supported(options));
	options.vertices = vertex_layout::position_normal_uv;
	CHECK(!vk::supported(options));
	options.vertices         = vertex_layout::position_normal;
	options.sampled_bindings = 1;
	CHECK(!vk::supported(options));
	options.sampled_bindings = 0;
	options.target_format    = color_format::rgba16_float;
	CHECK(!vk::supported(options));
	options.target_format = color_format::rgba8_unorm;
	options.depth_write   = true;
	CHECK(!vk::supported(options));
	options.depth_test = true;
	CHECK(vk::supported(options));
	CHECK(vk::choose_extent({0, 700}, {640, 480}, {64, 64}, {2000, 2000}).width == 0);
	CHECK(vk::choose_extent({500, 700}, {640, 480}, {64, 64}, {2000, 2000}).height == 480);
	const auto size =
	    vk::choose_extent({30, 3000}, {UINT32_MAX, UINT32_MAX}, {64, 64}, {2000, 2000});
	CHECK(size.width == 64 && size.height == 2000);
	std::puts("Vulkan profile: packet/stage/entry/ownership-layout bounds, push layout, refusals "
	          "and swapchain extents PASS");
} catch (const std::exception &e) {
	std::fprintf(stderr, "%s\n", e.what());
	return 1;
}
