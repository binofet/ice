#pragma once
#include "ice/platform/window.h"
namespace ice::platform::x11 {
#ifdef X11
} // namespace ice::platform::x11
#include <GL/glx.h>
#include <X11/Xlib.h>
namespace ice::platform::x11 {
/// Native X11 window with a compatibility GL context; native operations run on the application
/// thread.
class window : public ice::platform::window {
public:
	/// Create a native window from logical coordinates; parent is borrowed and must outlive this
	/// window.
	window(const ice::platform::window_def &win_def, ice::platform::window *parent = nullptr);
	/// Create a native window from logical coordinates; parent is borrowed and must outlive this
	/// window.
	~window() override;
	/// Release the owned native window; repeated cleanup is safe.
	ice::result cleanup() override;
	/// Show the native window; report failure through the returned status.
	ice::result show() override;
	/// Hide the native window without destroying its native resources.
	ice::result hide() override;
	/// Apply logical window geometry and presentation settings on the application thread.
	ice::result update_params(const ice::platform::window_def &win_def) override;
	/// Copy the native window position in logical desktop coordinates; output must be non-null.
	ice::result get_window_pos(ice::math::ivec2 *position) override;
	/// Copy drawable client dimensions in logical coordinates to non-null outputs.
	void get_client_size(int *width, int *height) const override;
	/// Create a compatibility OpenGL context, optionally sharing objects with a borrowed native
	/// context.
	void *create_gl_context(void *shared = nullptr);
	/// Destroy this window's graphics context after releasing context-owned rendering resources.
	void destroy_gl_context();
	/// Make this window's graphics context current on the calling render thread.
	void make_current();
	/// Present the back buffer of this window's current graphics context.
	void swap_buffers();
	/// Borrow the Xlib display connection; do not close it through this pointer.
	Display *get_display() const { return display_; }
	/// Borrow the native X11 window identity; zero denotes an unavailable window.
	::Window get_ref() const { return window_; }
	/// Drain native events on the application thread and forward input/lifecycle notifications.
	static void pump_events();
	/// Constrain/hide the cursor while capture is active; release capture before destroying the
	/// window.
	static void set_cursor_captured(bool capture);
	/// Move the native cursor using logical window coordinates.
	static void warp_cursor(short x, short y);
	/// Optional native-event hook; true marks the borrowed event as handled.
	virtual bool handle_native_event(XEvent &event [[maybe_unused]]) { return false; }

private:
	Display     *display_   = nullptr;
	::Window     window_    = 0;
	XVisualInfo *visual_    = nullptr;
	Colormap     colormap_  = 0;
	Cursor       cursor_    = 0;
	GLXContext   context_   = nullptr;
	Atom         delete_    = 0;
	bool         destroyed_ = false;
};
#endif

} // namespace ice::platform::x11
