#include "ice/platform/platform_events.h"
#include <array>
#include <mutex>
namespace ice::platform {
namespace {
struct event_state {
	std::mutex                       mutex;
	std::array<platform_event, 1024> events;
	std::size_t                      head = 0, count = 0, dropped = 0;
	std::uint64_t                    next_window = 1, next_sequence = 1;
};
event_state &state() {
	// Process lifetime avoids static teardown order hazards with legacy global apps.
	// An explicit application owner can replace this during lifecycle migration.
	static event_state *instance = new event_state;
	return *instance;
}
} // namespace
window_id platform_events::allocate_window_id() {
	auto                       &data = state();
	std::lock_guard<std::mutex> guard(data.mutex);
	return data.next_window++;
}
void platform_events::post(platform_event_type type, window_id window) {
	auto                       &data = state();
	std::lock_guard<std::mutex> guard(data.mutex);
	if (data.count == data.events.size()) {
		++data.dropped;
		return;
	}
	const std::size_t tail = (data.head + data.count) % data.events.size();
	data.events[tail]      = {type, window, data.next_sequence++};
	++data.count;
}
bool platform_events::poll(platform_event &event) {
	auto                       &data = state();
	std::lock_guard<std::mutex> guard(data.mutex);
	if (!data.count)
		return false;
	event     = data.events[data.head];
	data.head = (data.head + 1) % data.events.size();
	--data.count;
	return true;
}
std::size_t platform_events::dropped_events() {
	auto                       &data = state();
	std::lock_guard<std::mutex> guard(data.mutex);
	return data.dropped;
}

} // namespace ice::platform
