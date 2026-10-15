#pragma once
#include "ice/platform/desktop_window.h"
namespace ice::platform::detail {
inline bool valid_extent(int width, int height) {
	return width >= 64 && height >= 64 && width <= 16384 && height <= 16384;
}
ice::expected<std::unique_ptr<desktop_window>, window_diagnostic>
     create_desktop_window(const desktop_window_options &options);
void pump_desktop_events();
} // namespace ice::platform::detail
