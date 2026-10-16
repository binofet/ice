#include "ice/platform/platform_events.h"
#include "ice/core/utf8.h"
#include "input_cancellation_internal.h"
#include <array>
#include <cmath>
#include <map>
#include <mutex>
namespace ice::platform {
namespace {
struct event_state {
	std::mutex                         mutex;
	std::array<platform_event, 1024>   events;
	std::size_t                        head = 0, count = 0, dropped = 0, text_bytes = 0;
	std::uint64_t                      next_window = 1, next_sequence = 1;
	std::uint64_t                      cancel_all = 0;
	std::map<window_id, std::uint64_t> cancellations;
};
event_state &state() {
	// A process-lifetime service outlives registered windows and their retirement events.
	static event_state *instance = new event_state;
	return *instance;
}
bool boundary(const std::string &text, std::size_t index) {
	return index <= text.size() &&
	       (index == text.size() || (static_cast<unsigned char>(text[index]) & 0xC0) != 0x80);
}
bool enqueue(event_state &data, platform_event event) {
	if (data.count == data.events.size() || event.text.size() > 256 * 1024 - data.text_bytes) {
		data.dropped += data.count + 1;
		++data.cancel_all;
		for (auto &queued : data.events)
			queued = {};
		data.head = data.count = data.text_bytes = 0;
		event             = {platform_event_type::input_cancelled, application_scope, 0};
		const auto tail   = data.head;
		event.sequence    = data.next_sequence++;
		data.events[tail] = std::move(event);
		data.count        = 1;
		return false;
	}
	const auto tail = (data.head + data.count) % data.events.size();
	event.sequence  = data.next_sequence++;
	data.text_bytes += event.text.size();
	data.events[tail] = std::move(event);
	++data.count;
	return true;
}
} // namespace
window_id platform_events::allocate_window_id() {
	auto                       &data = state();
	std::lock_guard<std::mutex> guard(data.mutex);
	return data.next_window++;
}
void platform_events::post(platform_event_type type, window_id window) {
	(void)post(platform_event{type, window, 0});
}
bool platform_events::post(platform_event event) {
	auto                       &data = state();
	std::lock_guard<std::mutex> guard(data.mutex);
	const bool                  text = event.type == platform_event_type::text_committed ||
	                  event.type == platform_event_type::text_composition;
	const bool physical = event.type == platform_event_type::physical_key ||
	                      event.type == platform_event_type::pointer_button ||
	                      event.type == platform_event_type::pointer_move ||
	                      event.type == platform_event_type::scroll;
	const bool payload_valid = std::isfinite(event.x) && std::isfinite(event.y) &&
	                           std::isfinite(event.delta_x) && std::isfinite(event.delta_y) &&
	                           std::isfinite(event.scroll_x) && std::isfinite(event.scroll_y) &&
	                           std::fabs(event.x) <= 1e9 && std::fabs(event.y) <= 1e9 &&
	                           std::fabs(event.delta_x) <= 1e9 && std::fabs(event.delta_y) <= 1e9 &&
	                           std::fabs(event.scroll_x) <= 1e9 && std::fabs(event.scroll_y) <= 1e9;
	if (event.type < platform_event_type::window_registered ||
	    event.type > platform_event_type::scroll ||
	    (text && (!event.window || !ice::core::valid_utf8_text(event.text, 4096))) ||
	    (!text && !event.text.empty()) || !payload_valid || (physical && !event.window) ||
	    (event.type == platform_event_type::physical_key && !valid_physical_key(event.key)) ||
	    (event.type == platform_event_type::pointer_button && !valid_mouse_button(event.button)) ||
	    (event.type == platform_event_type::physical_key && !event.pressed && event.repeat) ||
	    (event.delta_units != pointer_delta_unit::logical &&
	     event.delta_units != pointer_delta_unit::device_counts) ||
	    (event.cancellation != input_cancellation::all &&
	     event.cancellation != input_cancellation::text) ||
	    (event.type == platform_event_type::text_composition &&
	     (event.selection_start > event.text.size() ||
	      event.selection_length > event.text.size() - event.selection_start ||
	      !boundary(event.text, event.selection_start) ||
	      !boundary(event.text, event.selection_start + event.selection_length)))) {
		++data.dropped;
		if (!event.window)
			++data.cancel_all;
		else if (auto found = data.cancellations.find(event.window);
		         found != data.cancellations.end())
			++found->second;
		(void)enqueue(data, {platform_event_type::input_cancelled, event.window, 0});
		return false;
	}
	const auto type   = event.type;
	const auto window = event.window;
	if ((type == platform_event_type::input_cancelled &&
	     event.cancellation == input_cancellation::all) ||
	    type == platform_event_type::focus_lost || type == platform_event_type::window_retired ||
	    type == platform_event_type::close_requested ||
	    type == platform_event_type::application_quit_requested) {
		if (!window)
			++data.cancel_all;
		else if (auto found = data.cancellations.find(window); found != data.cancellations.end())
			++found->second;
	}
	const bool result = enqueue(data, std::move(event));
	if (result &&
	    (type == platform_event_type::focus_lost || type == platform_event_type::window_retired ||
	     type == platform_event_type::close_requested))
		return enqueue(data, {platform_event_type::input_cancelled, window, 0});
	return result;
}
bool platform_events::poll(platform_event &event) {
	auto                       &data = state();
	std::lock_guard<std::mutex> guard(data.mutex);
	if (!data.count)
		return false;
	data.text_bytes -= data.events[data.head].text.size();
	event                  = std::move(data.events[data.head]);
	data.events[data.head] = {};
	data.head              = (data.head + 1) % data.events.size();
	--data.count;
	return true;
}
std::size_t platform_events::dropped_events() {
	auto                       &data = state();
	std::lock_guard<std::mutex> guard(data.mutex);
	return data.dropped;
}
namespace detail {
void register_window_input(window_id window) {
	auto                       &data = state();
	std::lock_guard<std::mutex> guard(data.mutex);
	data.cancellations.emplace(window, 0);
}
void unregister_window_input(window_id window) {
	auto                       &data = state();
	std::lock_guard<std::mutex> guard(data.mutex);
	data.cancellations.erase(window);
}
std::array<std::uint64_t, 2> window_input_cancellation(window_id window) {
	auto                       &data = state();
	std::lock_guard<std::mutex> guard(data.mutex);
	auto                        found = data.cancellations.find(window);
	return {data.cancel_all, found == data.cancellations.end() ? 0 : found->second};
}
} // namespace detail

} // namespace ice::platform
