#pragma once
namespace ice::platform::macos {
#ifdef __APPLE__
} // namespace ice::platform::macos
#include "ice/platform/window.h"
namespace ice::platform::macos {
/// Opaque native state owned by the macOS window backend; clients must not inspect or delete it.
struct window_state;
/// AppKit lives in the Objective-C++ implementation; public ICE headers stay C++.
class window : public ice::platform::window {
public:
	/// Construct with the borrowed dependencies described by this type; initialize explicitly
	/// before active use.
	window(const ice::platform::window_def &def, ice::platform::window *parent = nullptr);
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	~window() override;
	/// Release resources owned by this subsystem; borrowed dependencies must remain alive until
	/// cleanup completes.
	ice::result cleanup() override;
	/// Show the native window on its owning thread.
	ice::result show() override;
	/// Hide the native window on its owning thread.
	ice::result hide() override;
	/// Apply changed native window parameters on the owning thread; inspect status for platform
	/// failures.
	ice::result update_params(const ice::platform::window_def &win_def) override;
	/// Write the logical native window position; inspect status for invalid handle/output errors.
	ice::result get_window_pos(ice::math::ivec2 *pos) override;
	/// Write logical client dimensions to non-null outputs; values are UI units, not necessarily
	/// drawable pixels.
	void get_client_size(int *width, int *height) const override;
	/// Write drawable pixel dimensions to non-null outputs for viewport/render-target sizing.
	void get_framebuffer_size(int *width, int *height) const override;
	/// Borrowed NSWindow/NSView pointers. No ownership transfer.
	void *get_ref() const;
	/// Borrow the current legacy camera-to-world view pose.
	void *get_view() const;
	/// Create and own a native GL context for this window; false reports native creation failure.
	void *create_gl_context(void *shared_context = nullptr);
	/// Release the windows native GL context on its owning thread before destroying its surface.
	void destroy_gl_context();
	/// Make this windows live GL context current on the owning thread.
	void make_current();
	/// Present this windows current backbuffer.
	void swap_buffers();
	/// Return true to consume a borrowed NSEvent before AppKit handles it.
	virtual bool handle_native_event(void *event [[maybe_unused]]) { return false; }
	/// Pump application-wide native events on the owning thread; no root window is required.
	static void pump_events();
	/// Request native relative/captured cursor input on the owning thread.
	static void set_cursor_captured(bool captured);
	/// Move the native cursor to logical client coordinates and reset relative tracking.
	static void warp_cursor(short x, short y);
	/// Apply a native focus transition and cancel held input when focus is lost.
	static void focus_changed();

private:
	window_state *native_ = nullptr;
};
#endif

} // namespace ice::platform::macos
