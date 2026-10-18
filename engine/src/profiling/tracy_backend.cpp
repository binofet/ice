#include "backend.h"
#include <common/TracyVersion.hpp>
#include <tracy/Tracy.hpp>

static_assert(tracy::Version::Major == 0 && tracy::Version::Minor == 14 &&
              tracy::Version::Patch == 1);
#if !defined(TRACY_MANUAL_LIFETIME) || !defined(TRACY_ONLY_LOCALHOST) ||                           \
    !defined(TRACY_NO_BROADCAST) || !defined(TRACY_ON_DEMAND)
#error "ICE Tracy requires its pinned manual/loopback/on-demand configuration"
#endif
namespace ice::profiling::backend {
void         start() noexcept { tracy::StartupProfiler(); }
void         stop() noexcept { tracy::ShutdownProfiler(); }
bool         connected() noexcept { return ___tracy_connected() != 0; }
zone_context begin(const source_location *source) noexcept {
	return ___tracy_emit_zone_begin_callstack(source, 0, 1);
}
void end(zone_context context) noexcept { ___tracy_emit_zone_end(context); }
void text(zone_context context, std::string_view text) noexcept {
	___tracy_emit_zone_text(context, text.data(), text.size());
}
void value(zone_context context, std::uint64_t value) noexcept {
	___tracy_emit_zone_value(context, value);
}
void frame() noexcept { ___tracy_emit_frame_mark(nullptr); }
void plot(const char *name, std::int64_t value) noexcept { ___tracy_emit_plot_int(name, value); }
void metadata(std::string_view value) noexcept {
	___tracy_emit_message_appinfo(value.data(), value.size());
}
} // namespace ice::profiling::backend
