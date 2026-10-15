namespace ice::platform::win32 {
#ifdef WIN32
} // namespace ice::platform::win32
#include "ice/platform/win32/window.h"
#include "ice/input/input.h"
#include <algorithm>
#include <vector>
#include <windowsx.h>
namespace ice::input {
HWND g_hwnd = nullptr;
}
namespace ice::platform::win32 {
static std::vector<window *> nativeWindows;
HINSTANCE                    g_hInstance = GetModuleHandleA(nullptr);
using ice::input::g_hwnd;
static LRESULT CALLBACK Dispatch(HWND handle, UINT message, WPARAM word_param, LPARAM long_param) {
	auto window =
	    reinterpret_cast<ice::platform::win32::window *>(GetWindowLongPtrA(handle, GWLP_USERDATA));
	if (message == WM_NCCREATE) {
		window = static_cast<ice::platform::win32::window *>(
		    reinterpret_cast<CREATESTRUCTA *>(long_param)->lpCreateParams);
		window->set_hwnd(handle);
		SetWindowLongPtrA(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
	}
	return window ? window->window_proc(message, word_param, long_param)
	              : DefWindowProcA(handle, message, word_param, long_param);
}
window::window(const ice::platform::window_def &win_def, ice::platform::window *parent)
    : ice::platform::window(win_def, parent), hwnd_(nullptr) {
	create();
	if (hwnd_)
		nativeWindows.push_back(this);
}
window::window(const ice::platform::window_def &win_def, ice::platform::window *parent, HWND handle)
    : ice::platform::window(win_def, parent), hwnd_(handle), owned_(false) {
	if (hwnd_)
		nativeWindows.push_back(this);
}
ice::result window::cleanup() {
	nativeWindows.erase(std::remove(nativeWindows.begin(), nativeWindows.end(), this),
	                    nativeWindows.end());
	if (hwnd_) {
		if (g_hwnd == hwnd_) {
			g_hwnd = nullptr;
			ice::input::input::instance()->focus_changed(false);
		}
		SetWindowLongPtrA(hwnd_, GWLP_USERDATA, 0);
		if (owned_)
			DestroyWindow(hwnd_);
		hwnd_ = nullptr;
	}
	return IC_OK;
}
void window::create() {
	if (window_info_.width <= 0 || window_info_.height <= 0)
		return;
	WNDCLASSEXA type   = {};
	type.cbSize        = sizeof(type);
	type.style         = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	type.lpfnWndProc   = Dispatch;
	type.hInstance     = g_hInstance;
	type.hCursor       = LoadCursor(nullptr, IDC_ARROW);
	type.lpszClassName = "ICE.NativeWindow";
	if (!RegisterClassExA(&type) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
		return;
	DWORD style = window_info_.win_flags ? window_info_.win_flags : WS_OVERLAPPEDWINDOW;
	if (window_info_.full_screen)
		style = WS_POPUP;
	RECT rect = {0, 0, window_info_.width, window_info_.height};
	AdjustWindowRectEx(&rect, style, FALSE, 0);
	HWND owner =
	    parent_ ? static_cast<ice::platform::win32::window *>(parent_)->get_hwnd() : nullptr;
	hwnd_ = CreateWindowExA(0, type.lpszClassName, window_info_.window_name, style,
	                        window_info_.pos_x, window_info_.pos_y, rect.right - rect.left,
	                        rect.bottom - rect.top, owner, nullptr, g_hInstance, this);
	if (hwnd_)
		update_params(window_info_);
}
ice::result window::show() {
	if (!hwnd_)
		return IC_FAIL_CREATE_WIND;
	ShowWindow(hwnd_, SW_SHOW);
	UpdateWindow(hwnd_);
	return IC_OK;
}
ice::result window::hide() {
	if (!hwnd_)
		return IC_FAIL_CREATE_WIND;
	ShowWindow(hwnd_, SW_HIDE);
	return IC_OK;
}
ice::result window::update_params(const ice::platform::window_def &win_def) {
	if (!hwnd_ || win_def.width <= 0 || win_def.height <= 0)
		return IC_FAIL_GEN;
	DWORD style = win_def.full_screen
	                  ? WS_POPUP
	                  : (win_def.win_flags ? win_def.win_flags : WS_OVERLAPPEDWINDOW);
	SetWindowLongPtrA(hwnd_, GWL_STYLE, style);
	SetWindowTextA(hwnd_, win_def.window_name ? win_def.window_name : "ICE");
	RECT rect = {0, 0, win_def.width, win_def.height};
	AdjustWindowRectEx(&rect, style, FALSE, 0);
	int x = win_def.pos_x, y = win_def.pos_y, width = rect.right - rect.left,
	    height = rect.bottom - rect.top;
	if (win_def.full_screen) {
		MONITORINFO monitor = {};
		monitor.cbSize      = sizeof(monitor);
		GetMonitorInfo(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &monitor);
		x      = monitor.rcMonitor.left;
		y      = monitor.rcMonitor.top;
		width  = monitor.rcMonitor.right - x;
		height = monitor.rcMonitor.bottom - y;
	}
	if (!SetWindowPos(hwnd_, nullptr, x, y, width, height,
	                  SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED))
		return IC_FAIL_GEN;
	window_info_ = win_def;
	return IC_OK;
}
ice::result window::get_window_pos(ice::math::ivec2 *position) {
	if (!hwnd_ || !position)
		return IC_FAIL_NPE;
	RECT rect;
	if (!GetWindowRect(hwnd_, &rect))
		return IC_FAIL_GEN;
	*position = ice::math::ivec2(rect.left, rect.top);
	return IC_OK;
}
void window::get_client_size(int *width, int *height) const {
	RECT rect = {};
	if (hwnd_)
		GetClientRect(hwnd_, &rect);
	if (width)
		*width = rect.right;
	if (height)
		*height = rect.bottom;
}
LRESULT window::window_proc(UINT message, WPARAM word_param, LPARAM long_param) {
	auto input = ice::input::input::instance();
	switch (message) {
	case WM_CLOSE:
		request_close();
		return 0;
	case WM_NCDESTROY: {
		HWND handle = hwnd_;
		hwnd_       = nullptr;
		if (g_hwnd == handle)
			g_hwnd = nullptr;
		request_close();
		return DefWindowProcA(handle, message, word_param, long_param);
	}
	case WM_DESTROY:
		request_close();
		return 0; // Closing a preview must not post an application-wide quit.
	case WM_SETFOCUS:
		ice::platform::platform_events::post(ice::platform::platform_event_type::focus_gained,
		                                     get_id());
		g_hwnd = hwnd_;
		input->focus_changed(true);
		return 0;
	case WM_KILLFOCUS:
		ice::platform::platform_events::post(ice::platform::platform_event_type::focus_lost,
		                                     get_id());
		if (g_hwnd == hwnd_)
			g_hwnd = nullptr;
		input->focus_changed(false);
		return 0;
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		input->get_input(word_param, long_param, false);
		break;
	case WM_KEYUP:
	case WM_SYSKEYUP:
		input->get_input(word_param, long_param, true);
		break;
	case WM_LBUTTONDOWN:
		input->set_down(ice::input::mouse_l, true);
		break;
	case WM_LBUTTONUP:
		input->set_down(ice::input::mouse_l, false);
		break;
	case WM_RBUTTONDOWN:
		input->set_down(ice::input::mouse_r, true);
		break;
	case WM_RBUTTONUP:
		input->set_down(ice::input::mouse_r, false);
		break;
	case WM_MBUTTONDOWN:
		input->set_down(ice::input::mouse_m, true);
		break;
	case WM_MBUTTONUP:
		input->set_down(ice::input::mouse_m, false);
		break;
	case WM_MOUSEWHEEL:
		input->get_scroll_wheel(GET_WHEEL_DELTA_WPARAM(word_param));
		return 0;
	case WM_MOUSEMOVE:
		input->update_mouse_pos(GET_X_LPARAM(long_param), GET_Y_LPARAM(long_param));
		return 0;
	case WM_INPUT:
		input->get_raw_input(long_param);
		break;
	}
	return DefWindowProcA(hwnd_, message, word_param, long_param);
}
void window::pump_events() {
	MSG event;
	while (PeekMessage(&event, nullptr, 0, 0, PM_REMOVE)) {
		if (event.message == WM_QUIT) {
			ice::platform::platform_events::post(
			    ice::platform::platform_event_type::application_quit_requested);
			for (auto window : nativeWindows)
				window->request_close();
			continue;
		}
		TranslateMessage(&event);
		DispatchMessage(&event);
	}
}
#endif

} // namespace ice::platform::win32
