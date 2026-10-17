#include "desktop_window_internal.h"
#include "ice/core/utf8.h"
#include "input_cancellation_internal.h"
#include <algorithm>
#include <cmath>
#include <vector>
namespace ice::platform {
namespace {
std::vector<desktop_window *> &windows() {
	static auto *owners = new std::vector<desktop_window *>;
	return *owners;
}
} // namespace
desktop_window::desktop_window() : id_(platform_events::allocate_window_id()) {
	detail::register_window_input(id_);
	const auto cancellation = detail::window_input_cancellation(id_);
	cancellation_all_       = cancellation[0];
	cancellation_window_    = cancellation[1];
	windows().push_back(this);
	platform_events::post(platform_event_type::window_registered, id_);
}
desktop_window::~desktop_window() {
	std::erase(windows(), this);
	detail::unregister_window_input(id_);
	platform_events::post(platform_event_type::window_retired, id_);
}
void desktop_window::request_close() {
	if (!close_requested_) {
		close_requested_ = true;
		(void)set_pointer_mode(ice::platform::pointer_mode::normal);
		cancel_native_physical_input();
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
void desktop_window::pump_events() {
	for (auto *window : windows())
		window->refresh_input_cancellation();
	detail::pump_desktop_events();
	for (auto *window : windows())
		window->refresh_input_cancellation();
}
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
	platform_event event{platform_event_type::input_cancelled, id_, 0};
	event.cancellation = input_cancellation::text;
	platform_events::post(std::move(event));
}
void desktop_window::cancel_input() {
	(void)set_pointer_mode(ice::platform::pointer_mode::normal);
	cancel_native_physical_input();
	cancel_native_text_input();
	platform_events::post(platform_event_type::input_cancelled, id_);
}
void desktop_window::refresh_input_cancellation() {
	auto cancellation = detail::window_input_cancellation(id_);
	if (cancellation_all_ == cancellation[0] && cancellation_window_ == cancellation[1])
		return;
	cancellation_all_    = cancellation[0];
	cancellation_window_ = cancellation[1];
	const bool captured  = pointer_mode_ == ice::platform::pointer_mode::relative;
	if (captured)
		(void)change_pointer_mode(ice::platform::pointer_mode::normal);
	pointer_mode_ = ice::platform::pointer_mode::normal;
	cancel_native_physical_input();
	if (captured)
		cancel_native_text_input();
}
ice::expected<void, window_diagnostic>
desktop_window::change_pointer_mode(ice::platform::pointer_mode mode) {
	if (mode == ice::platform::pointer_mode::normal)
		return {};
	return ice::unexpected(window_diagnostic{"Relative pointer input is unavailable"});
}
ice::expected<void, window_diagnostic>
desktop_window::set_pointer_mode(ice::platform::pointer_mode mode) {
	refresh_input_cancellation();
	if (mode != ice::platform::pointer_mode::normal &&
	    mode != ice::platform::pointer_mode::relative)
		return ice::unexpected(window_diagnostic{"Invalid pointer mode"});
	if (mode == pointer_mode_)
		return {};
	if (close_requested_ && mode != ice::platform::pointer_mode::normal)
		return ice::unexpected(window_diagnostic{"Closed windows cannot capture pointer input"});
	auto changed = change_pointer_mode(mode);
	if (changed)
		pointer_mode_ = mode;
	return changed;
}
ice::platform::pointer_mode desktop_window::pointer_mode() const {
	const_cast<desktop_window *>(this)->refresh_input_cancellation();
	return pointer_mode_;
}
ice::expected<std::string, window_diagnostic>
desktop_window::read_clipboard_text(std::size_t maximum_bytes) {
	if (!maximum_bytes || maximum_bytes > 16 * 1024 * 1024)
		return ice::unexpected(window_diagnostic{"Invalid clipboard text byte limit"});
	auto result = read_native_clipboard_text(maximum_bytes);
	if (result && !ice::core::valid_utf8_text(*result, maximum_bytes))
		return ice::unexpected(
		    window_diagnostic{"Clipboard text exceeds its byte limit or is invalid UTF-8"});
	return result;
}
ice::expected<void, window_diagnostic> desktop_window::write_clipboard_text(std::string_view text) {
	if (!ice::core::valid_utf8_text(text, 16 * 1024 * 1024))
		return ice::unexpected(
		    window_diagnostic{"Clipboard text exceeds 16MiB or is invalid UTF-8"});
	return write_native_clipboard_text(text);
}
ice::expected<std::string, window_diagnostic>
desktop_window::read_native_clipboard_text(std::size_t maximum_bytes) {
	(void)maximum_bytes;
	return ice::unexpected(window_diagnostic{"Native clipboard text is unavailable"});
}
ice::expected<void, window_diagnostic>
desktop_window::write_native_clipboard_text(std::string_view text) {
	(void)text;
	return ice::unexpected(window_diagnostic{"Native clipboard text is unavailable"});
}
bool desktop_window::set_text_input_rect(const text_input_rect &rect) {
	if (!std::isfinite(rect.x) || !std::isfinite(rect.y) || !std::isfinite(rect.width) ||
	    !std::isfinite(rect.height) || std::fabs(rect.x) > 1000000 || std::fabs(rect.y) > 1000000 ||
	    rect.width < 0 || rect.height < 0 || rect.width > 16384 || rect.height > 16384)
		return false;
	if (text_input_rect_.x == rect.x && text_input_rect_.y == rect.y &&
	    text_input_rect_.width == rect.width && text_input_rect_.height == rect.height)
		return true;
	text_input_rect_ = rect;
	text_input_rect_changed();
	return true;
}
} // namespace ice::platform
