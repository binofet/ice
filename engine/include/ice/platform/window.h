#pragma once

#include "ice/globals.h"
#include "ice/math/ivec2.h"
#include "ice/platform/platform_events.h"
namespace ice::platform {

/*! Helper for window creation
 **/
struct window_def {
	/// Platform-native window-style flags; zero selects the backend default.
	ice::uint   win_flags;   //! Flags that define this window
	short       width;       //! Width of the window
	short       height;      //! Height of the window
	short       pos_x;       //! Position of Upper Left Corner on X
	short       pos_y;       //! Position of Upper Left Corner on Y
	const char *window_name; //! Name of the window
	bool        full_screen; //! If the window is fullscreen
};

/*! Class provides a common interface to platform specific
 *    implementations.
 **/
class window {
public:
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	virtual ~window(void);

	/// Release resources owned by this subsystem; borrowed dependencies must remain alive until
	/// cleanup completes.
	virtual ice::result cleanup(void) { return IC_FAIL_GEN; };

	static window *get_window(const window_def &win_def, window *parent = 0);

	/// Show the native window on its owning thread.
	virtual ice::result show(void) = 0;
	/// Hide the native window on its owning thread.
	virtual ice::result hide(void) = 0;

	/// Apply changed native window parameters on the owning thread; inspect status for platform
	/// failures.
	virtual ice::result update_params(const window_def &win_def) = 0;

	/// Write the logical native window position; inspect status for invalid handle/output errors.
	virtual ice::result get_window_pos(ice::math::ivec2 *pos) = 0;

	/// Event pumping is application-wide; close state and sizes belong to each window.
	static void pump_events();
	/// Return whether this window has received/requested closure.
	bool should_close() const { return close_requested_; }
	/// Return the process-local window identity used to route application events.
	window_id get_id() const { return id_; }
	/// Request closure and publish a close event once; native destruction remains explicit.
	virtual void request_close() {
		if (!close_requested_)
			platform_events::post(platform_event_type::close_requested, id_);
		close_requested_ = true;
	}
	/// Write logical client dimensions to non-null outputs; values are UI units, not necessarily
	/// drawable pixels.
	virtual void get_client_size(int *width, int *height) const {
		if (width)
			*width = window_info_.width;
		if (height)
			*height = window_info_.height;
	}
	/// Write drawable pixel dimensions to non-null outputs for viewport/render-target sizing.
	virtual void get_framebuffer_size(int *width, int *height) const {
		get_client_size(width, height);
	}

	/// Copy current parameters into caller storage; inspect status for invalid output/state.
	ice::result get_params(window_def *const win_def);

protected:
	window(const window_def &win_def, window *parent);

	window_def window_info_; //! Contains info about the window
	window    *parent_;
	window    *child_;
	bool       active_;
	bool       close_requested_ = false;
	window_id  id_;

private:
	window(const window &ref);                  //! Disable copy
	const window &operator=(const window &ref); //! Disable assignment
};

FORCE_INLINE ice::result window::get_params(window_def *const win_def) {
	*win_def = window_info_;
	/// memcpy(pWinDef,&m_WindowInfo,sizeof(icWindowDef));
	return IC_OK;
}

} // namespace ice::platform
