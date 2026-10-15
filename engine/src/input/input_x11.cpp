#include "ice/input/input.h"
#include "ice/platform/x11/window.h"

#include <algorithm>
namespace ice::input {
static double previous_x = 0, previous_y = 0;
static bool   mouse_baseline       = false;
input        *input::ptr_instance_ = nullptr;
input::input() : hide_cursor_(false), last_pressed_(0), cur_key_(0), mouse_{}, elapsed_(0) {
	memset(keys_, 0, sizeof(keys_));
	memset(key_buffer_, 0, sizeof(key_buffer_));
	memset(axis_, 0, sizeof(axis_));
}
input::~input() {}
ice::result input::init() { return IC_OK; }
ice::result input::update() {
	memcpy(keys_[1], keys_[0], sizeof(keys_[0]));
	memset(axis_, 0, sizeof(axis_));
	last_pressed_ = 0;
	return IC_OK;
}
void input::update_mouse_position(double x, double y, int height) {
	if (mouse_baseline && height > 0) {
		axis_[0] += (x - previous_x) / height;
		axis_[1] += (y - previous_y) / height;
	}
	previous_x     = x;
	previous_y     = y;
	mouse_baseline = true;
	mouse_.xpos    = (short)std::max(-32767.0, std::min(x, 32767.0));
	mouse_.ypos    = (short)std::max(-32767.0, std::min(height - y, 32767.0));
}
void input::warp_cursor(short x, short y) {
	reset_mouse_tracking();
	ice::platform::x11::window::warp_cursor(x, y);
}
void input::get_pos(short *x, short *y) {
	if (x)
		*x = mouse_.xpos;
	if (y)
		*y = mouse_.ypos;
}
ice::math::real input::get_axis(ice::uchar axis) { return axis < 16 ? axis_[axis] : 0; }
void            input::hide_curs() {
    mouse_baseline = false;
    memset(axis_, 0, sizeof(axis_));
    hide_cursor_ = true;
    ice::platform::x11::window::set_cursor_captured(true);
}
void input::show_curs() {
	mouse_baseline = false;
	memset(axis_, 0, sizeof(axis_));
	hide_cursor_ = false;
	ice::platform::x11::window::set_cursor_captured(false);
}

void input::add_mouse_delta(double x, double y, int height) {
	if (height > 0) {
		axis_[0] += x / height;
		axis_[1] += y / height;
	}
}
void input::reset_mouse_tracking() {
	mouse_baseline = false;
	memset(axis_, 0, sizeof(axis_));
}

} // namespace ice::input
