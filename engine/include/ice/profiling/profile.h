#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

#ifndef ICE_PROFILING_ENABLED
#define ICE_PROFILING_ENABLED 0
#endif
#if ICE_PROFILING_ENABLED && !defined(ICE_PROFILING_RECORDING_TEST)
#include <tracy/TracyC.h>
#endif

namespace ice::profiling {
/// Whether this translation unit includes profiling instrumentation.
inline constexpr bool compiled = ICE_PROFILING_ENABLED != 0;
/// Immutable upstream client revision used by the enabled build.
inline constexpr std::string_view tracy_revision = "30997d5ca6bb632cc10807a1da8a6d3de0aeeb3c";
/// Larger annotations are omitted whole rather than truncated.
inline constexpr std::size_t max_annotation_bytes = 4096;
/// Accounting for the custom Lua allocator, including its retained allocation headers.
/// Counters are atomic telemetry; a concurrent snapshot is not an atomic multi-field transaction.
struct lua_memory_counters {
	/// Currently retained bytes, including allocator headers.
	std::uint64_t live_bytes = 0;
	/// Largest observed retained-byte total.
	std::uint64_t peak_bytes = 0;
	/// Allocation attempts, including denied attempts.
	std::uint64_t allocations = 0;
	/// Reallocation attempts, including denied attempts.
	std::uint64_t reallocations = 0;
	/// Frees of an existing allocation.
	std::uint64_t frees = 0;
	/// Attempts rejected by the Lua memory quota or size limit.
	std::uint64_t quota_failures = 0;
	/// Native allocator failures, including retained shrinks.
	std::uint64_t allocator_failures = 0;
	/// Failed shrinks that retain the original allocation capacity.
	std::uint64_t retained_shrinks = 0;
};
/// Outcome of one custom-allocator operation; failed operations preserve live bytes.
enum class resize_outcome { committed, quota_denied, allocator_denied, retained_shrink };

#if ICE_PROFILING_ENABLED
#ifdef ICE_PROFILING_RECORDING_TEST
/// Private recording-harness substitute for the pinned backend's static source location.
struct source_location {
	/// Static zone label.
	const char *name;
	/// Static function label.
	const char *function;
	/// Static source file label.
	const char *file;
	/// Source line number.
	std::uint32_t line;
	/// Optional backend color.
	std::uint32_t color;
};
/// Private recording-harness substitute for an active backend zone token.
struct zone_context {
	/// Recording event identifier.
	std::uint32_t id = 0;
	/// Whether the recording zone emitted a begin event.
	int active = 0;
};
#else
/// Pinned backend source-location ABI; labels must have static lifetime.
using source_location = ___tracy_source_location_data;
/// Pinned backend zone token, including its on-demand connection identity.
using zone_context = TracyCZoneCtx;
#endif
/// One optional process capture. Start/stop only on a quiescent host boundary, before/after
/// instrumented resources and workers. No implicit initialization; no arbitrary runtime toggle.
class capture_session {
public:
	/// Start the backend only for an explicit request without an existing owner.
	explicit capture_session(bool requested = false) noexcept;
	/// Stop an owned backend after all instrumented work has ended.
	~capture_session();
	capture_session(const capture_session &)            = delete;
	capture_session &operator=(const capture_session &) = delete;
	/// Whether this object owns a started backend; no collector connection is implied.
	bool active() const noexcept { return owned_; }

private:
	bool owned_ = false;
};
/// C++ scope only; never place across a Lua C longjmp frame. Labels/source have static lifetime.
class zone {
public:
	/// Begin a zone only while an explicitly started session has a collector connection.
	explicit zone(const source_location *source) noexcept;
	/// End an active zone on normal return or C++ exception unwinding.
	~zone();
	zone(const zone &)            = delete;
	zone &operator=(const zone &) = delete;
	/// Copy an existing nonempty bounded annotation into this captured zone.
	void text(std::string_view value) const noexcept;
	/// Attach an existing unsigned identifier without string formatting.
	void value(std::uint64_t value) const noexcept;

private:
	zone_context context_{};
};
/// Sample Lua counters and mark a captured CPU frame boundary.
void frame() noexcept;
/// A stable/static plot name; values are measurements, never inferred resident GPU storage.
void plot(const char *static_name, std::uint64_t value) noexcept;
/// Copy existing bounded metadata into the active capture; no application string allocation.
void metadata(std::string_view value) noexcept;
/// Copy a bounded name=value field using stack storage, including before collector connection.
void metadata_field(std::string_view name, std::string_view value) noexcept;
/// Assign a numeric trace association without creating a program metadata registry.
std::uint64_t next_program_id() noexcept;
/// Cold lifetime association using the existing canonical URI (which may already contain a hash).
bool describe_program(std::uint64_t id, std::string_view canonical_uri) noexcept;
/// Account one allocator outcome in actual retained bytes, including allocation headers.
void account_lua_resize(std::size_t old_bytes, std::size_t new_bytes,
                        resize_outcome outcome) noexcept;
/// Read the process-wide custom-Lua-allocator counters; concurrent fields are approximate.
lua_memory_counters lua_memory() noexcept;
/// Emit the named Lua allocator plots only while capturing.
void sample_lua_memory() noexcept;
#else
/// Inert counterpart of the explicit host-owned session when instrumentation is disabled.
class capture_session {
public:
	/// Preserve the call contract while leaving the disabled backend absent.
	constexpr explicit capture_session([[maybe_unused]] bool requested = false) noexcept {}
	capture_session(const capture_session &)            = delete;
	capture_session &operator=(const capture_session &) = delete;
	/// Disabled builds never own a backend.
	constexpr bool active() const noexcept { return false; }
};
/// Disabled builds do not collect Lua allocation telemetry.
inline constexpr lua_memory_counters lua_memory() noexcept { return {}; }
#endif
} // namespace ice::profiling

#define ICE_PROFILE_JOIN_(a, b) a##b
#define ICE_PROFILE_JOIN(a, b) ICE_PROFILE_JOIN_(a, b)
#if ICE_PROFILING_ENABLED
#define ICE_PROFILE_ZONE(variable, label)                                                          \
	static const ::ice::profiling::source_location ICE_PROFILE_JOIN(                               \
	    ice_profile_source_, __LINE__){label, __func__, __FILE__, __LINE__, 0};                    \
	::ice::profiling::zone variable { &ICE_PROFILE_JOIN(ice_profile_source_, __LINE__) }
#define ICE_PROFILE_TEXT(variable, expression) (variable).text(expression)
#define ICE_PROFILE_VALUE(variable, expression) (variable).value(expression)
#define ICE_PROFILE_FRAME() ::ice::profiling::frame()
#define ICE_PROFILE_PLOT(name, value) ::ice::profiling::plot(name, value)
#define ICE_PROFILE_METADATA(value) ::ice::profiling::metadata(value)
#define ICE_PROFILE_METADATA_FIELD(name, value) ::ice::profiling::metadata_field(name, value)
#define ICE_PROFILE_LUA_RESIZE(old_bytes, new_bytes, outcome)                                      \
	::ice::profiling::account_lua_resize(old_bytes, new_bytes, outcome)
#define ICE_PROFILE_LUA_SAMPLE() ::ice::profiling::sample_lua_memory()
#define ICE_PROFILE_PROGRAM(id, uri) ::ice::profiling::describe_program(id, uri)
#else
// Arguments intentionally disappear, including expressions which would allocate or format IDs.
#define ICE_PROFILE_ZONE(variable, label) ((void)0)
#define ICE_PROFILE_TEXT(variable, text) ((void)0)
#define ICE_PROFILE_VALUE(variable, value) ((void)0)
#define ICE_PROFILE_FRAME() ((void)0)
#define ICE_PROFILE_PLOT(name, value) ((void)0)
#define ICE_PROFILE_METADATA(value) ((void)0)
#define ICE_PROFILE_METADATA_FIELD(name, value) ((void)0)
#define ICE_PROFILE_LUA_RESIZE(old_bytes, new_bytes, outcome) ((void)0)
#define ICE_PROFILE_LUA_SAMPLE() ((void)0)
#define ICE_PROFILE_PROGRAM(id, uri) ((void)0)
#endif
