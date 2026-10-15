#pragma once

#include "ice/math/common_math.h"
#include "ice/util/time.h"

#include "ice/input/button_map.h"
namespace ice::input {
/// Xlib defines AnyKey as a macro; preserve ICE's existing method name.
#ifdef AnyKey
#undef AnyKey
#endif
} // namespace ice::input
namespace ice::platform {
class window;
}
namespace ice::input {
#ifdef WIN32
/// Active native window for legacy cursor capture; updated on the host thread.
extern HWND g_hwnd;
#endif

/// Legacy per-frame mouse position, button, and wheel accumulator in logical window coordinates.
struct mouse {
	/// Current logical mouse X coordinate.
	short xpos;
	/// Current logical mouse Y coordinate.
	short ypos;
	/// Previous logical mouse X coordinate used for delta accumulation.
	short old_x;
	/// Previous logical mouse Y coordinate used for delta accumulation.
	short old_y;
};

/*! Provides interface to various input devices
 **/
class input {
public:
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	~input(void);

	static input *instance(void);

	/// Initialize against the supplied borrowed dependencies; inspect the result before using the
	/// subsystem.
	ice::result init(void);
	/// Advance frame-dependent state on the owning thread; elapsed-time arguments are seconds.
	ice::result update(void);

	/// Move the native cursor to logical client coordinates and reset relative tracking.
	void warp_cursor(short x, short y);
	/// Return/copy the current native cursor/file position in the units defined by this interface.
	void get_pos(short *x, short *y);

	/// Return the accumulated legacy input-axis value for this frame.
	ice::math::real get_axis(ice::uchar axis);

	/// Hide the native cursor on the owning thread.
	void hide_curs(void);
	/// Show the native cursor on the owning thread.
	void show_curs(void);

	/// INQUIRY
	bool is_down(ice::uchar key) { return keys_[0][key]; };
	/// Return whether the key/button transitioned to pressed during this frame.
	bool is_pressed(ice::uchar key) { return (!keys_[0][key] && keys_[1][key]); };

	/// Return whether any mapped key/button is currently active.
	bool any_key(void) { return (last_pressed_ != 0); };
#ifdef WIN32
	/// Update focus state; losing focus cancels held input and cursor capture.
	void focus_changed(bool focused);
	/// Consume the borrowed native raw-input message handle on the application thread.
	void get_raw_input(LPARAM handle);
	/// Apply the Win32 key transition; key is a virtual-key code and up marks release.
	void get_input(WPARAM key, LPARAM long_param, bool up);
	/// Accumulate the Win32 wheel delta in native WHEEL_DELTA units.
	void get_scroll_wheel(short delta);
	/// Update logical cursor coordinates supplied by a Win32 window message.
	void update_mouse_pos(int x, int y, bool fullscreen = false);

	//  static HCURSOR invisCurs;
#endif

	/// Set the native held-key/button state in the input accumulator.
	void set_down(ice::uchar key, bool is_down) {
		keys_[0][key] = is_down;
		if (is_down)
			last_pressed_ = key + 1;
	};
	/// Update logical cursor position and dimensions supplied by the native backend.
	void update_mouse_position(double x, double y, int height);
	/// Accumulate native relative mouse movement in logical input units.
	void add_mouse_delta(double x, double y, int height);
	/// Reset relative mouse baselines without generating a movement spike.
	void reset_mouse_tracking();
	/// Focus loss cancels held controls; it must not synthesize a UI click/release.
	void cancel_held_input() {
		memset(keys_, 0, sizeof(keys_));
		last_pressed_ = 0;
		reset_mouse_tracking();
	}
	/// Accumulate native scroll-wheel input for the next update.
	void add_scroll(ice::math::real value) { axis_[mouse_z] += value; }
	/// Set an explicit press transition in the legacy input accumulator.
	void set_pressed(ice::uchar key, bool is_down) {
		keys_[0][key] = !is_down;
		keys_[1][key] = is_down;
	};

private:
	input(void);

	bool keys_[2][256];
	bool hide_cursor_;

	ice::uchar                   last_pressed_;
	ice::uchar                   key_buffer_[1024];
	[[maybe_unused]] ice::ushort cur_key_;

	mouse mouse_;

	ice::math::real                  axis_[16];
	[[maybe_unused]] ice::math::real elapsed_;
	ice::util::timer                 timer_;

	static input *ptr_instance_;
};

/*! Function gets the singleton instance
 *
 *  @returns    icInput*    Pointer to the singleton input instance
 **/
FORCE_INLINE input *input::instance(void) {
	// TODO: thread safety
	if (!ptr_instance_) {
		ptr_instance_ = new input();
	}
	return ptr_instance_;
}

} // namespace ice::input
