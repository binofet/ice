#include "ice/profiling/profile.h"
#include "ice/script/lua_program.h"
#include "profiling/allocations.h"
#include <array>
#include <cstdio>
#include <stdexcept>
#ifdef ICE_PROFILING_RECORDING_TEST
#include "profiling/recording.h"
#endif
#if ICE_PROFILING_ENABLED && !defined(ICE_PROFILING_RECORDING_TEST)
#include <tracy/Tracy.hpp>
#endif

#define CHECK(expression)                                                                          \
	do {                                                                                           \
		if (!(expression)) {                                                                       \
			std::fprintf(stderr, "Profiling check%d: %s\n", __LINE__, #expression);                \
			return 1;                                                                              \
		}                                                                                          \
	} while (false)
namespace {
struct source final : ice::io::virtual_scheme_handler {
	std::string                   text;
	ice::io::resource_read_result read(const ice::io::resource_uri &,
	                                   std::vector<unsigned char> &bytes, std::string &) override {
		bytes.assign(text.begin(), text.end());
		return ice::io::resource_read_result::success;
	}
};
} // namespace
int main() {
	using namespace ice;
	profiling::capture_session inactive;
	CHECK(!inactive.active());
	int evaluated = 0;
	{
		ICE_PROFILE_ZONE(test_zone, "Test.DefaultInactive");
		ICE_PROFILE_TEXT(test_zone, (++evaluated, std::string_view("existing")));
	}
	CHECK(evaluated == (profiling::compiled ? 1 : 0));
#ifdef ICE_PROFILING_RECORDING_TEST
	CHECK(profiling::test::stats().starts == 0 && profiling::test::events().empty());
	{
		profiling::capture_session capture(true);
		CHECK(capture.active());
		profiling::capture_session second(true);
		CHECK(!second.active());
		profiling::test::watch_allocations(true);
		{
			ICE_PROFILE_ZONE(annotation, "Test.OwnedAnnotation");
			char text[] = "retained";
			ICE_PROFILE_TEXT(annotation, std::string_view(text, 8));
			text[0] = 'X';
			ICE_PROFILE_VALUE(annotation, UINT64_MAX);
		}
		profiling::test::watch_allocations(false);
		CHECK(profiling::test::allocation_attempts() == 0);
		std::array<char, profiling::max_annotation_bytes + 1> bounded_text{};
		bounded_text.fill('v');
		const auto before_metadata = profiling::test::events().size();
		profiling::test::watch_allocations(true);
		ICE_PROFILE_METADATA_FIELD(
		    "b", std::string_view(bounded_text.data(), profiling::max_annotation_bytes - 2));
		// Exact boundary is copied; oversize metadata and signed-plot overflow are omitted.
		ICE_PROFILE_METADATA_FIELD(
		    "b", std::string_view(bounded_text.data(), profiling::max_annotation_bytes - 1));
		ICE_PROFILE_PLOT("Test.UnrepresentableUnsigned", UINT64_MAX);
		profiling::test::watch_allocations(false);
		CHECK(profiling::test::allocation_attempts() == 0);
		CHECK(profiling::test::events().size() == before_metadata + 1);
		const auto &bounded_event = profiling::test::events().back();
		CHECK(bounded_event.kind == profiling::test::event_kind::metadata &&
		      bounded_event.text_size == profiling::max_annotation_bytes &&
		      bounded_event.text[0] == 'b' && bounded_event.text[1] == '=' &&
		      bounded_event.text[profiling::max_annotation_bytes - 1] == 'v');
		try {
			ICE_PROFILE_ZONE(exception, "Test.Exception");
			throw std::runtime_error("test");
		} catch (const std::runtime_error &) {
		}
		profiling::account_lua_resize(0, 100, profiling::resize_outcome::committed);
		profiling::account_lua_resize(100, 200, profiling::resize_outcome::quota_denied);
		profiling::account_lua_resize(100, 50, profiling::resize_outcome::retained_shrink);
		CHECK(profiling::lua_memory().live_bytes == 100);
		profiling::account_lua_resize(100, 70, profiling::resize_outcome::committed);
		profiling::account_lua_resize(70, 0, profiling::resize_outcome::committed);
		profiling::account_lua_resize(0, 10, profiling::resize_outcome::allocator_denied);
		auto metrics = profiling::lua_memory();
		CHECK(metrics.live_bytes == 0 && metrics.peak_bytes == 100 && metrics.allocations == 2 &&
		      metrics.reallocations == 3 && metrics.frees == 1);
		CHECK(metrics.quota_failures == 1 && metrics.allocator_failures == 2 &&
		      metrics.retained_shrinks == 1);
	}
	CHECK(profiling::test::stats().starts == 1 && profiling::test::stats().stops == 1);
#elif ICE_PROFILING_ENABLED
	// This is the REAL linked client. Only inspect its manual-lifetime flag; never start it.
	CHECK(!tracy::IsProfilerStarted());
#else
	profiling::capture_session requested(true);
	CHECK(!requested.active());
	ICE_PROFILE_ZONE(disabled, (++evaluated, "Disabled"));
	ICE_PROFILE_VALUE(disabled, ++evaluated);
	ICE_PROFILE_PLOT("Disabled", ++evaluated);
	ICE_PROFILE_METADATA((++evaluated, "Disabled"));
	ICE_PROFILE_METADATA_FIELD((++evaluated, "name"), (++evaluated, "value"));
	ICE_PROFILE_LUA_RESIZE(++evaluated, ++evaluated, profiling::resize_outcome::committed);
	ICE_PROFILE_PROGRAM(++evaluated, (++evaluated, "memory://disabled"));
	CHECK(evaluated == 0);
#endif
	io::resource_resolver resolver;
	auto                  memory = std::make_shared<source>();
	resolver.register_handler("memory", memory);
#ifdef ICE_PROFILING_RECORDING_TEST
	{
		profiling::capture_session capture(true);
		profiling::test::set_connected(false);
#endif
		memory->text = "function run(i) return host.echo{command='read',target='object:7'} end";
		{
			auto program = script::lua_program::prepare(resolver, "memory://profile/retained.lua");
			CHECK(program);
#ifdef ICE_PROFILING_RECORDING_TEST
			CHECK(profiling::lua_memory().live_bytes == program->memory_bytes());
			for (const auto &event : profiling::test::events())
				CHECK(std::string_view(event.text, event.text_size)
				          .find("memory://profile/retained.lua") == std::string_view::npos);
			profiling::test::set_connected(true);
#endif
			const script::host_function host{
			    "echo",
			    [](const script::scalar_record &input)
			        -> ice::expected<script::scalar_record, script::diagnostic> { return input; }};
			auto output = program->call("run", {}, std::span(&host, 1));
			CHECK(output && std::get<std::string>(output->at("target")) == "object:7");
			const script::host_function failing{
			    "echo",
			    [](const script::scalar_record &)
			        -> ice::expected<script::scalar_record, script::diagnostic> {
				    throw std::runtime_error("host failure");
			    }};
			CHECK(!program->call("run", {}, std::span(&failing, 1)) && program->faulted());
		}
		memory->text =
		    "function run(i) local t={} while true do t[#t+1]=string.rep('x',4000) end end";
		{
			auto program = script::lua_program::prepare(resolver, "memory://profile/quota.lua",
			                                            {64 * 1024, 100000, 4096});
			CHECK(program);
			auto failed = program->call("run", {});
			CHECK(!failed && failed.error().code == script::error_code::memory_limit);
		}
		ICE_PROFILE_FRAME();
#ifdef ICE_PROFILING_RECORDING_TEST
		CHECK(profiling::lua_memory().live_bytes == 0 &&
		      profiling::lua_memory().quota_failures > 1);
	}
	unsigned               begins = 0, ends = 0, hosts = 0, frames = 0, uri_associations = 0;
	std::array<bool, 1024> open_zones{};
	bool                   owned = false, target = false, uri = false, exact_value = false;
	for (const auto &e : profiling::test::events()) {
		begins += e.kind == profiling::test::event_kind::begin;
		ends += e.kind == profiling::test::event_kind::end;
		if (e.kind == profiling::test::event_kind::begin) {
			CHECK(e.id < open_zones.size() && !open_zones[e.id]);
			open_zones[e.id] = true;
		} else if (e.kind == profiling::test::event_kind::end) {
			CHECK(e.id < open_zones.size() && open_zones[e.id]);
			open_zones[e.id] = false;
		}
		if (e.kind == profiling::test::event_kind::begin &&
		    std::string_view(e.name) == "Lua.HostInvoke")
			++hosts;
		frames += e.kind == profiling::test::event_kind::frame;
		const std::string_view text(e.text, e.text_size);
		owned |= text == "retained";
		target |= text == "object:7";
		uri |= text.find("memory://profile/retained.lua") != std::string_view::npos;
		uri_associations += e.kind == profiling::test::event_kind::metadata &&
		                    text.find("memory://profile/retained.lua") != std::string_view::npos;
		exact_value |= e.kind == profiling::test::event_kind::value && e.value == UINT64_MAX;
	}
	CHECK(begins == ends && hosts == 2 && frames == 1 && owned && target && uri &&
	      uri_associations == 1 && exact_value && profiling::test::stats().dropped == 0);
#elif ICE_PROFILING_ENABLED
	CHECK(!tracy::IsProfilerStarted());
	CHECK(profiling::lua_memory().live_bytes == 0);
#endif
	std::puts("Profiling CPU passed: disabled erasure / inactive real client or recording zones / "
	          "Lua accounting and longjmp-safe host failures");
}
