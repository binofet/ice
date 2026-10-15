#pragma once

#include "ice/core/expected.h"
#include "ice/platform/platform_events.h"
#include <cstdint>
#include <memory>
#include <string>

namespace ice::platform {
/// Native windowing family; graphics APIs are selected independently.
enum class native_window_kind { appkit, win32, x11 };
/// Borrowed OS presentation endpoints. Ownership remains with the window.
struct native_window_handle {
	/// Identifies the interpretation of the other fields.
	native_window_kind kind;
	/// NSWindow/HWND pointer or X11 Window integer encoded without truncation.
	std::uintptr_t window = 0;
	/// Borrowed NSView for AppKit; null on Win32/X11.
	void *view = nullptr;
	/// Borrowed Display pointer on X11; null on other backends.
	void *display = nullptr;
};
/// Logical client dimensions and drawable pixel dimensions, independently of a GPU device.
struct window_extent {
	/// Client size in logical UI units.
	int width = 0, height = 0;
	/// Client size in drawable pixels.
	int pixel_width = 0, pixel_height = 0;
	/// Drawable pixels per logical UI unit.
	float scale = 1;
};
/// Initial native window description; no renderer or context is implicitly created.
struct desktop_window_options {
	/// UTF-8 title, with no embedded NUL and at most 1024 bytes.
	std::string title = "ICE";
	/// Initial logical client size, each in [64,16384].
	int width = 1280, height = 800;
	/// Show the window after successful creation.
	bool visible = true;
};
/// Recoverable window-creation diagnostic, independent of graphics initialization.
struct window_diagnostic {
	/// Explanation suitable for a host or command response.
	std::string message;
};
/// Native desktop window owner. All operations, event pumping and destruction belong on
/// the application's UI thread. Native endpoints are borrowed until this owner is destroyed.
/// A window owns no graphics device/context; a backend attaches its own presentation surface.
class desktop_window {
public:
	/// Destroy the native window after clients have detached their presentation surfaces.
	virtual ~desktop_window();
	/// Create a native window or return a diagnostic; never initialize a GPU implicitly.
	static ice::expected<std::unique_ptr<desktop_window>, window_diagnostic>
	create(const desktop_window_options &options = {});
	/// Pump the process event queue once; no root or hidden delivery window is required.
	static void pump_events();
	/// Return the process-local routing identity; zero is reserved for application scope.
	window_id id() const { return id_; }
	/// Return the authoritative close request even if the bounded event stream overflowed.
	bool should_close() const { return close_requested_; }
	/// Request closure once; native destruction remains explicit and owned by the caller.
	void request_close();
	/// Show/hide the native window; showing may request focus according to OS policy.
	virtual void show(bool visible = true) = 0;
	/// Request a new logical client size; invalid bounds return false without a native request.
	virtual bool resize(int width, int height) = 0;
	/// Request native fullscreen/windowed presentation; the OS may complete asynchronously.
	virtual void fullscreen(bool enabled) = 0;
	/// Query current logical/drawable size; no GPU context is needed.
	virtual window_extent extent() const = 0;
	/// Borrow backend-specific OS handles for renderer/native extension integration.
	virtual native_window_handle native_handle() const = 0;

protected:
	/// Allocate a routing identity and register this owner with the process event stream.
	desktop_window();

private:
	desktop_window(const desktop_window &other)            = delete;
	desktop_window &operator=(const desktop_window &other) = delete;
	window_id       id_                                    = 0;
	bool            close_requested_                       = false;
};
} // namespace ice::platform
