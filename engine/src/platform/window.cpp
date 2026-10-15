
#include "ice/platform/window.h"
namespace ice::platform {

#ifdef __APPLE__
} // namespace ice::platform
#include "ice/platform/macos/window.h"
namespace ice::platform {
#elif defined(WIN32)
} // namespace ice::platform
#include "ice/platform/win32/window.h"
namespace ice::platform {
#elif defined(X11)
} // namespace ice::platform
#include "ice/platform/x11/window.h"
namespace ice::platform {
#endif

/*! Creates platform specific window
 **/
window *window::get_window(const window_def &win_def, window *parent) {
#ifdef __APPLE__
	auto window = new ice::platform::macos::window(win_def, parent);
	if (!window->get_ref()) {
		delete window;
		return nullptr;
	}
	return window;
#elif defined(WIN32)
	auto window = new ice::platform::win32::window(win_def, parent);
	if (!window->get_hwnd()) {
		delete window;
		return nullptr;
	}
	return window;
#elif defined(X11)
	auto window = new ice::platform::x11::window(win_def, parent);
	if (!window->get_ref()) {
		delete window;
		return nullptr;
	}
	return window;
#else
#error "This platform is not supported yet"
#endif
}

/*! Protected c'tor
 *
 *
 *    @param    win_def    Struct containing information for creating a window
 *
 **/
window::window(const window_def &win_def, window *parent) {
	id_ = platform_events::allocate_window_id();
	platform_events::post(platform_event_type::window_registered, id_);
	window_info_ = win_def;
	parent_      = parent;
	child_       = NULL;
	active_      = false;
}

void window::pump_events() {
#ifdef __APPLE__
	ice::platform::macos::window::pump_events();
#elif defined(WIN32)
	ice::platform::win32::window::pump_events();
#elif defined(X11)
	ice::platform::x11::window::pump_events();
#endif
}

window::~window() { platform_events::post(platform_event_type::window_retired, id_); }

} // namespace ice::platform
