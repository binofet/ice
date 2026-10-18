#include "ice/profiling/profile.h"
#include "backend.h"
#include <array>
#include <atomic>
#include <charconv>
#include <cstring>
#include <limits>

#ifndef ICE_PROFILE_REVISION
// Only the private recording harness compiles this source outside the verified CMake target.
#define ICE_PROFILE_REVISION "unverified-recording-harness"
#define ICE_PROFILE_SOURCE_STATE "unverified"
#define ICE_PROFILE_CXX_STANDARD "unverified"
#define ICE_PROFILE_CONFIGURATION "unverified"
#endif

namespace ice::profiling {
namespace {
std::atomic<bool>          running{false};
std::atomic<std::uint64_t> program_ids{0};
struct counters {
	std::atomic<std::uint64_t> live{0}, peak{0}, allocations{0}, reallocations{0}, frees{0};
	std::atomic<std::uint64_t> quota_failures{0}, allocator_failures{0}, retained_shrinks{0};
} memory;
bool emitting() noexcept { return running.load(std::memory_order_acquire) && backend::connected(); }
} // namespace
capture_session::capture_session(bool requested) noexcept {
	if (!requested || running.load(std::memory_order_acquire))
		return;
	// Host serializes startup/shutdown and joins instrumented work before shutdown.
	backend::start();
	running.store(true, std::memory_order_release);
	owned_ = true;
	metadata("ICE optional Tracy "
	         "0.14.1;30997d5ca6bb632cc10807a1da8a6d3de0aeeb3c;manual;localhost;no-broadcast;on-"
	         "demand;CPU-zones-only");
	metadata_field("ice.revision", ICE_PROFILE_REVISION);
	metadata_field("ice.source_state", ICE_PROFILE_SOURCE_STATE);
	metadata_field("ice.cxx_standard", ICE_PROFILE_CXX_STANDARD);
	metadata_field("ice.configuration", ICE_PROFILE_CONFIGURATION);
}
capture_session::~capture_session() {
	if (owned_) {
		running.store(false, std::memory_order_release);
		backend::stop();
	}
}
zone::zone(const source_location *source) noexcept {
	if (emitting())
		context_ = backend::begin(source);
}
zone::~zone() {
	if (context_.active && running.load(std::memory_order_acquire))
		backend::end(context_);
}
void zone::text(std::string_view value) const noexcept {
	if (context_.active && !value.empty() && running.load(std::memory_order_acquire) &&
	    value.size() <= max_annotation_bytes)
		backend::text(context_, value);
}
void zone::value(std::uint64_t value) const noexcept {
	if (context_.active && running.load(std::memory_order_acquire))
		backend::value(context_, value);
}
void frame() noexcept {
	if (emitting()) {
		sample_lua_memory();
		backend::frame();
	}
}
void plot(const char *static_name, std::uint64_t value) noexcept {
	// Do not silently relabel an unrepresentable unsigned measurement as an exact signed plot.
	if (value <= std::uint64_t(std::numeric_limits<std::int64_t>::max()) && emitting())
		backend::plot(static_name, std::int64_t(value));
}
void metadata(std::string_view value) noexcept {
	// On-demand app info must survive until a collector connects; backend handles that storage.
	if (running.load(std::memory_order_acquire) && value.size() <= max_annotation_bytes)
		backend::metadata(value);
}
std::uint64_t next_program_id() noexcept {
	return program_ids.fetch_add(1, std::memory_order_relaxed) + 1;
}
void metadata_field(std::string_view name, std::string_view value) noexcept {
	if (!running.load(std::memory_order_acquire) || name.size() >= max_annotation_bytes ||
	    value.size() > max_annotation_bytes - name.size() - 1)
		return;
	std::array<char, max_annotation_bytes> text{};
	if (!name.empty())
		std::memcpy(text.data(), name.data(), name.size());
	text[name.size()] = '=';
	if (!value.empty())
		std::memcpy(text.data() + name.size() + 1, value.data(), value.size());
	metadata({text.data(), name.size() + 1 + value.size()});
}
bool describe_program(std::uint64_t id, std::string_view canonical_uri) noexcept {
	// Defer per-program metadata until actually captured; an unconnected on-demand client
	// must not accumulate a second catalog of every discarded/probed program.
	if (!emitting() || canonical_uri.empty())
		return false;
	std::array<char, max_annotation_bytes> text{};
	constexpr std::string_view             prefix = "Lua program ";
	std::memcpy(text.data(), prefix.data(), prefix.size());
	auto number = std::to_chars(text.data() + prefix.size(), text.data() + text.size(), id);
	if (number.ec != std::errc{} ||
	    canonical_uri.size() >= std::size_t(text.data() + text.size() - number.ptr))
		return false;
	*number.ptr++ = ' ';
	std::memcpy(number.ptr, canonical_uri.data(), canonical_uri.size());
	metadata({text.data(), std::size_t(number.ptr - text.data()) + canonical_uri.size()});
	return true;
}
void account_lua_resize(std::size_t old_bytes, std::size_t new_bytes,
                        resize_outcome outcome) noexcept {
	if (!old_bytes && !new_bytes)
		return;
	if (!new_bytes)
		memory.frees.fetch_add(1, std::memory_order_relaxed);
	else if (!old_bytes)
		memory.allocations.fetch_add(1, std::memory_order_relaxed);
	else
		memory.reallocations.fetch_add(1, std::memory_order_relaxed);
	if (outcome == resize_outcome::quota_denied) {
		memory.quota_failures.fetch_add(1, std::memory_order_relaxed);
		return;
	}
	if (outcome == resize_outcome::allocator_denied) {
		memory.allocator_failures.fetch_add(1, std::memory_order_relaxed);
		return;
	}
	if (outcome == resize_outcome::retained_shrink) {
		memory.allocator_failures.fetch_add(1, std::memory_order_relaxed);
		memory.retained_shrinks.fetch_add(1, std::memory_order_relaxed);
		return;
	}
	if (new_bytes > old_bytes) {
		const auto live = memory.live.fetch_add(new_bytes - old_bytes, std::memory_order_relaxed) +
		                  new_bytes - old_bytes;
		auto peak = memory.peak.load(std::memory_order_relaxed);
		while (peak < live &&
		       !memory.peak.compare_exchange_weak(peak, live, std::memory_order_relaxed)) {
		}
	} else
		memory.live.fetch_sub(old_bytes - new_bytes, std::memory_order_relaxed);
}
lua_memory_counters lua_memory() noexcept {
	return {memory.live.load(),
	        memory.peak.load(),
	        memory.allocations.load(),
	        memory.reallocations.load(),
	        memory.frees.load(),
	        memory.quota_failures.load(),
	        memory.allocator_failures.load(),
	        memory.retained_shrinks.load()};
}
void sample_lua_memory() noexcept {
	if (!emitting())
		return;
	const auto m = lua_memory();
	plot("Lua allocator live bytes", m.live_bytes);
	plot("Lua allocator peak bytes", m.peak_bytes);
	plot("Lua allocation attempts", m.allocations);
	plot("Lua reallocation attempts", m.reallocations);
	plot("Lua free calls", m.frees);
	plot("Lua quota denials", m.quota_failures);
	plot("Lua allocator failures", m.allocator_failures);
	plot("Lua retained shrink failures", m.retained_shrinks);
}
} // namespace ice::profiling
