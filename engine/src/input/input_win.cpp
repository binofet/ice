namespace ice::input {
#ifdef WIN32
} // namespace ice::input
#include "ice/input/input.h"
#include <algorithm>
#include <vector>
namespace ice::input {
static bool   hidden = false, baseline = false;
static double previous_x = 0, previous_y = 0;
static void   sync_cursor(bool capture) {
    bool active = capture && g_hwnd && GetForegroundWindow() == g_hwnd;
    if (active != hidden) {
        if (active)
            while (ShowCursor(FALSE) >= 0) {
            }
        else
            while (ShowCursor(TRUE) < 0) {
            }
        hidden = active;
    }
    if (active) {
        RECT rect;
        GetClientRect(g_hwnd, &rect);
        POINT first = {rect.left, rect.top}, last = {rect.right, rect.bottom};
        ClientToScreen(g_hwnd, &first);
        ClientToScreen(g_hwnd, &last);
        RECT clip = {first.x, first.y, last.x, last.y};
        ClipCursor(&clip);
    } else
        ClipCursor(nullptr);
}
input *input::ptr_instance_ = nullptr;
input::input() : hide_cursor_(false), last_pressed_(0), cur_key_(0), mouse_{}, elapsed_(0) {
	memset(keys_, 0, sizeof(keys_));
	memset(key_buffer_, 0, sizeof(key_buffer_));
	memset(axis_, 0, sizeof(axis_));
}
input::~input() { sync_cursor(false); }
ice::result input::init() {
	RAWINPUTDEVICE mouse = {0x01, 0x02, 0, nullptr};
	return RegisterRawInputDevices(&mouse, 1, sizeof(mouse)) ? IC_OK : IC_FAIL_GEN;
}
ice::result input::update() {
	memcpy(keys_[1], keys_[0], sizeof(keys_[0]));
	memset(axis_, 0, sizeof(axis_));
	last_pressed_ = 0;
	sync_cursor(hide_cursor_);
	return IC_OK;
}
void input::get_input(WPARAM key, LPARAM long_param [[maybe_unused]], bool up) {
	if (key < 256)
		set_down((ice::uchar)key, !up);
}
void input::get_scroll_wheel(short delta) {
	axis_[mouse_z] += (ice::math::real)delta / WHEEL_DELTA;
}
void input::update_mouse_position(double x, double y, int height) {
	if (baseline && height > 0) {
		axis_[0] += (x - previous_x) / height;
		axis_[1] += (y - previous_y) / height;
	}
	previous_x  = x;
	previous_y  = y;
	baseline    = true;
	mouse_.xpos = (short)x;
	mouse_.ypos = (short)(height - y);
}
void input::update_mouse_pos(int x, int y, bool fullscreen [[maybe_unused]]) {
	if (!g_hwnd || hide_cursor_)
		return;
	RECT rect;
	GetClientRect(g_hwnd, &rect);
	update_mouse_position(x, y, rect.bottom);
}
void input::add_mouse_delta(double x, double y, int height) {
	if (height > 0) {
		axis_[0] += x / height;
		axis_[1] += y / height;
	}
}
void input::get_raw_input(LPARAM handle) {
	if (!hide_cursor_ || !g_hwnd)
		return;
	UINT size = 0;
	GetRawInputData((HRAWINPUT)handle, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
	std::vector<unsigned char> bytes(size);
	if (GetRawInputData((HRAWINPUT)handle, RID_INPUT, bytes.data(), &size,
	                    sizeof(RAWINPUTHEADER)) != size)
		return;
	auto raw = reinterpret_cast<RAWINPUT *>(bytes.data());
	if (raw->header.dwType != RIM_TYPEMOUSE || (raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE))
		return;
	RECT rect;
	GetClientRect(g_hwnd, &rect);
	add_mouse_delta(raw->data.mouse.lLastX, raw->data.mouse.lLastY, rect.bottom);
}
void input::reset_mouse_tracking() {
	baseline = false;
	memset(axis_, 0, sizeof(axis_));
}
void input::focus_changed(bool focused) {
	cancel_held_input();
	if (focused)
		init();
	sync_cursor(hide_cursor_);
}
void input::warp_cursor(short x, short y) {
	if (g_hwnd) {
		POINT point = {x, y};
		ClientToScreen(g_hwnd, &point);
		SetCursorPos(point.x, point.y);
	}
	reset_mouse_tracking();
}
void input::get_pos(short *x, short *y) {
	if (x)
		*x = mouse_.xpos;
	if (y)
		*y = mouse_.ypos;
}
ice::math::real input::get_axis(ice::uchar axis) { return axis < 16 ? axis_[axis] : 0; }
void            input::hide_curs() {
    hide_cursor_ = true;
    reset_mouse_tracking();
    init();
    sync_cursor(true);
}
void input::show_curs() {
	hide_cursor_ = false;
	reset_mouse_tracking();
	sync_cursor(false);
}
#endif

} // namespace ice::input
