#include "backend.h"
#include "recording.h"
#include <array>
#include <cstring>

namespace ice::profiling {
namespace {
std::array<test::event, 1024> recorded{};
std::size_t                   count               = 0;
std::uint32_t                 next_id             = 0;
bool                          started             = false;
bool                          collector_connected = true;
test::recording_stats         totals;
void emit(test::event_kind kind, const char *name = nullptr, std::uint64_t id = 0,
          std::uint64_t value = 0, std::string_view text = {}) noexcept {
	if (count == recorded.size()) {
		++totals.dropped;
		return;
	}
	auto &event     = recorded[count++];
	event.kind      = kind;
	event.name      = name;
	event.id        = id;
	event.value     = value;
	event.text_size = text.size();
	if (text.size())
		std::memcpy(event.text, text.data(), text.size());
}
} // namespace
namespace backend {
void start() noexcept {
	started             = true;
	collector_connected = true;
	++totals.starts;
}
void stop() noexcept {
	started = false;
	++totals.stops;
}
bool         connected() noexcept { return started && collector_connected; }
zone_context begin(const source_location *source) noexcept {
	const auto id = ++next_id;
	emit(test::event_kind::begin, source->name, id);
	return {id, 1};
}
void end(zone_context context) noexcept { emit(test::event_kind::end, nullptr, context.id); }
void text(zone_context context, std::string_view text) noexcept {
	emit(test::event_kind::text, nullptr, context.id, 0, text);
}
void value(zone_context context, std::uint64_t value) noexcept {
	emit(test::event_kind::value, nullptr, context.id, value);
}
void frame() noexcept { emit(test::event_kind::frame); }
void plot(const char *name, std::int64_t value) noexcept {
	emit(test::event_kind::plot, name, 0, std::uint64_t(value));
}
void metadata(std::string_view value) noexcept {
	emit(test::event_kind::metadata, nullptr, 0, 0, value);
}
} // namespace backend
namespace test {
std::span<const event> events() noexcept { return {recorded.data(), count}; }
recording_stats        stats() noexcept { return totals; }
void                   set_connected(bool connected) noexcept { collector_connected = connected; }
} // namespace test
} // namespace ice::profiling
