#include "desktop_window_internal.h"
#include "ice/core/utf8.h"
#include <cmath>
namespace ice::platform {
desktop_window::desktop_window() : id_(platform_events::allocate_window_id()) {
	platform_events::post(platform_event_type::window_registered, id_);
}
desktop_window::~desktop_window() {
	platform_events::post(platform_event_type::window_retired, id_);
}
void desktop_window::request_close() {
	if (!close_requested_) {
		close_requested_ = true;
		cancel_native_text_input();
		platform_events::post(platform_event_type::close_requested, id_);
	}
}
ice::expected<std::unique_ptr<desktop_window>, window_diagnostic>
desktop_window::create(const desktop_window_options &options) {
	if (!detail::valid_extent(options.width, options.height) ||
	    !ice::core::valid_utf8_text(options.title, 1024))
		return ice::unexpected(window_diagnostic{"Invalid window title or client dimensions"});
	return detail::create_desktop_window(options);
}
void desktop_window::pump_events() { detail::pump_desktop_events(); }
void desktop_window::enable_text_input(bool enabled) {
	if (text_input_enabled_ == enabled)
		return;
	text_input_enabled_ = enabled;
	if (!enabled)
		cancel_text_input();
	text_input_changed();
}
void desktop_window::cancel_text_input() {
	cancel_native_text_input();
	platform_events::post(platform_event_type::input_cancelled, id_);
}
bool desktop_window::set_text_input_rect(const text_input_rect &rect) {
	if (!std::isfinite(rect.x) || !std::isfinite(rect.y) || !std::isfinite(rect.width) ||
	    !std::isfinite(rect.height) || std::fabs(rect.x) > 1000000 || std::fabs(rect.y) > 1000000 ||
	    rect.width < 0 || rect.height < 0 || rect.width > 16384 || rect.height > 16384)
		return false;
	text_input_rect_ = rect;
	text_input_changed();
	return true;
}
} // namespace ice::platform
