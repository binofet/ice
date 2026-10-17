#pragma once

#include "ice/core/expected.h"
#include "ice/platform/platform_events.h"
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

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
/// Text caret/candidate anchor in logical client units, measured from the top-left corner.
struct text_input_rect {
	/// Caret origin; finite coordinates with absolute value at most one million.
	float x = 0, y = 0;
	/// Caret dimensions; finite values in [0,16384].
	float width = 1, height = 18;
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
	/// Request closure once and discard native preedit; destruction remains caller-owned.
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
	/// Enable native committed-text/IME and interpreted editing actions for this window.
	/// Disabled by default. Disabling cancels preedit; focus loss never commits marked text.
	/// This insertion/preedit client does not expose surrounding-document reconversion.
	/// Clipboard commands are application actions; explicit text transport is available below.
	void enable_text_input(bool enabled = true);
	/// Read native clipboard text on the UI thread, without modifying it or synthesizing input.
	/// The byte limit must be in [1,16MiB]; invalid UTF-8/NUL, oversized data and unsupported
	/// transfers return a diagnostic without truncation. Empty/non-text clipboards return empty.
	/// X11 external selection transfers have a bounded wait and may reject incremental transfers.
	ice::expected<std::string, window_diagnostic>
	read_clipboard_text(std::size_t maximum_bytes = 4096);
	/// Replace native clipboard text explicitly on the UI thread. Accept valid UTF-8 without NUL,
	/// at most 16MiB. On X11 ownership lasts while this window lives and ordinary events are
	/// pumped. This capability is independent of text focus and never submits a command.
	ice::expected<void, window_diagnostic> write_clipboard_text(std::string_view text);
	/// Return whether this window accepts native text/IME input.
	bool text_input_enabled() const { return text_input_enabled_; }
	/// Discard current native preedit and emit text-only input_cancelled without committing text.
	/// Physical held controls and relative capture remain active.
	void cancel_text_input();
	/// Release relative capture, discard native preedit and cancel all held input for this window.
	void cancel_input();
	/// Set cursor behavior while focused/visible. Relative mode never implicitly steals focus;
	/// failure preserves the effective mode. Focus/hide/close/overflow release capture without
	/// automatically reacquiring it. All calls and queries belong on the UI thread.
	ice::expected<void, window_diagnostic> set_pointer_mode(ice::platform::pointer_mode mode);
	/// Return effective behavior, consuming pending native cancellation on the UI thread.
	ice::platform::pointer_mode pointer_mode() const;
	/// Report the backend path, including an explicit warp fallback or unsupported capability.
	virtual relative_pointer_kind relative_pointer_capability() const {
		return relative_pointer_kind::unavailable;
	}
	/// Update the logical candidate anchor; reject nonfinite/out-of-bounds values unchanged.
	bool set_text_input_rect(const text_input_rect &rect);
	/// Return the last accepted text caret/candidate anchor.
	text_input_rect text_input_area() const { return text_input_rect_; }

protected:
	/// Allocate a routing identity and register this owner with the process event stream.
	desktop_window();
	/// Refresh native text-input activation/candidate placement after a public setting changes.
	virtual void text_input_changed() {}
	/// Discard native preedit after focus/hide/disable or an explicit client cancellation.
	virtual void cancel_native_text_input() {}
	/// Backend-only bounded text retrieval, after public argument validation.
	virtual ice::expected<std::string, window_diagnostic>
	read_native_clipboard_text(std::size_t maximum_bytes);
	/// Backend-only text publication, after public UTF-8/size validation.
	virtual ice::expected<void, window_diagnostic>
	write_native_clipboard_text(std::string_view text);
	/// Reset native held-key/repeat/pointer tracking after all-input cancellation, independently of
	/// IME.
	virtual void cancel_native_physical_input() {}
	/// Change native cursor ownership. Normal mode must be safe during shutdown and focus loss.
	virtual ice::expected<void, window_diagnostic>
	change_pointer_mode(ice::platform::pointer_mode mode);

private:
	desktop_window(const desktop_window &other)                        = delete;
	desktop_window             &operator=(const desktop_window &other) = delete;
	window_id                   id_                                    = 0;
	bool                        close_requested_                       = false;
	bool                        text_input_enabled_                    = false;
	text_input_rect             text_input_rect_;
	ice::platform::pointer_mode pointer_mode_     = ice::platform::pointer_mode::normal;
	std::uint64_t               cancellation_all_ = 0, cancellation_window_ = 0;
	void                        refresh_input_cancellation();
};
} // namespace ice::platform
