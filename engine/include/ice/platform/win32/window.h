#pragma once
namespace ice::platform::win32 {
#ifdef WIN32
#ifndef __IC_WINDOW_WIN_H__
#define __IC_WINDOW_WIN_H__
} // namespace ice::platform::win32

#include <windows.h>

#include "ice/platform/window.h"
namespace ice::platform::win32 {

/// Native Win32 window. Pump messages on the application thread; destroy graphics contexts before
/// the window.
class window : public ice::platform::window {
public:
	/// Create a native window from logical coordinates; parent is borrowed and must outlive this
	/// window.
	window(const ice::platform::window_def &win_def, ice::platform::window *parent);
	/// Create a native window from logical coordinates; parent is borrowed and must outlive this
	/// window.
	window(const ice::platform::window_def &win_def, ice::platform::window *parent, HWND hwnd);
	/// Create a native window from logical coordinates; parent is borrowed and must outlive this
	/// window.
	virtual ~window(void) { cleanup(); };

	/// Release the owned native window; repeated cleanup is safe.
	virtual ice::result cleanup(void);

	/// Show the native window; report failure through the returned status.
	virtual ice::result show(void);
	/// Hide the native window without destroying its native resources.
	virtual ice::result hide(void);

	/// Apply logical window geometry and presentation settings on the application thread.
	virtual ice::result update_params(const ice::platform::window_def &win_def);

	/// Copy the native window position in logical desktop coordinates; output must be non-null.
	virtual ice::result get_window_pos(ice::math::ivec2 *position);
	/// Copy drawable client dimensions in logical coordinates to non-null outputs.
	void get_client_size(int *width, int *height) const override;

	//! TODO: move this down a level and make platform independant
	/// Drain native events on the application thread and forward input/lifecycle notifications.
	static void pump_events();
	/// Dispatch a native Win32 message; arguments follow the Win32 window-procedure contract.
	virtual LRESULT window_proc(UINT message, WPARAM word_param, LPARAM long_param);

	/// Assign a borrowed native window handle; ownership follows the constructor used.
	inline void set_hwnd(HWND hwnd) { hwnd_ = hwnd; };

	/// Borrow this window's HWND; the handle becomes invalid when the native window is destroyed.
	inline HWND get_hwnd(void) { return hwnd_; };

private:
	window(void);
	window(const window &ref);
	const window &operator=(const window &ref);

	void create(void);

	bool owned_ = true;
	HWND hwnd_; //! Handle to the window
};

#endif //__IC_WINDOW_WIN_H__
#endif // WIN32

} // namespace ice::platform::win32
