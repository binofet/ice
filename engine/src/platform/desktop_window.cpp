#include "desktop_window_internal.h"
#include "ice/core/utf8.h"
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
} // namespace ice::platform
