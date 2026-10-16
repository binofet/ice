#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "../desktop_window_internal.h"
#include "../physical_key_internal.h"
#include <algorithm>
#include <map>
#include <vector>
#include <windows.h>

// The IMM SDK declarations require the Win32 handle and geometry types first.
#include <imm.h>

namespace ice::platform::detail {
namespace {
class win32_window;
std::map<HWND, win32_window *> windows;
win32_window                  *relative_owner = nullptr;
key_modifiers                  modifiers() {
    key_modifiers result = 0;
    if (GetKeyState(VK_SHIFT) < 0)
        result |= modifier_flag(key_modifier::shift);
    if (GetKeyState(VK_CONTROL) < 0)
        result |= modifier_flag(key_modifier::control);
    if (GetKeyState(VK_MENU) < 0)
        result |= modifier_flag(key_modifier::alt);
    if (GetKeyState(VK_LWIN) < 0 || GetKeyState(VK_RWIN) < 0)
        result |= modifier_flag(key_modifier::super);
    if (GetKeyState(VK_CAPITAL) & 1)
        result |= modifier_flag(key_modifier::caps_lock);
    return result;
}
edit_command command_for(WPARAM key, key_modifiers flags) {
	const bool control =
	    has_modifier(flags, key_modifier::control) && !has_modifier(flags, key_modifier::alt);
	if (control)
		switch (key) {
		case 'A':
			return edit_command::select_all;
		case 'C':
			return edit_command::copy;
		case 'X':
			return edit_command::cut;
		case 'V':
			return edit_command::paste;
		case 'U':
			return edit_command::delete_to_line_start;
		case 'K':
			return edit_command::delete_to_line_end;
		case 'W':
			return edit_command::delete_word_backward;
		case 'P':
			return edit_command::move_up;
		case 'N':
			return edit_command::move_down;
		case 'D':
			return edit_command::delete_forward;
		}
	switch (key) {
	case VK_RETURN:
		return edit_command::accept_line;
	case VK_ESCAPE:
		return edit_command::cancel;
	case VK_TAB:
		return edit_command::tab;
	case VK_BACK:
		return control ? edit_command::delete_word_backward : edit_command::backspace;
	case VK_DELETE:
		return control ? edit_command::delete_word_forward : edit_command::delete_forward;
	case VK_LEFT:
		return control ? edit_command::word_left : edit_command::move_left;
	case VK_RIGHT:
		return control ? edit_command::word_right : edit_command::move_right;
	case VK_UP:
		return edit_command::move_up;
	case VK_DOWN:
		return edit_command::move_down;
	case VK_HOME:
		return edit_command::line_start;
	case VK_END:
		return edit_command::line_end;
	case VK_PRIOR:
		return edit_command::page_up;
	case VK_NEXT:
		return edit_command::page_down;
	default:
		return edit_command::none;
	}
}
bool utf8(const std::wstring &text, std::string &result) {
	if (text.empty()) {
		result.clear();
		return true;
	}
	const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
	                                      int(text.size()), nullptr, 0, nullptr, nullptr);
	if (count <= 0 || count > 4096)
		return false;
	result.resize(std::size_t(count));
	return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), int(text.size()),
	                           result.data(), count, nullptr, nullptr) == count;
}
class win32_window final : public desktop_window {
public:
	~win32_window() override {
		cancel_input();
		if (native_)
			DestroyWindow(native_);
	}
	bool initialize(const desktop_window_options &options) {
		int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, options.title.data(),
		                                int(options.title.size()), nullptr, 0);
		if (!count && !options.title.empty())
			return false;
		std::wstring title(std::size_t(count), L'\0');
		MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, options.title.data(),
		                    int(options.title.size()), title.data(), count);
		WNDCLASSEXW descriptor{};
		descriptor.cbSize        = sizeof(descriptor);
		descriptor.lpfnWndProc   = procedure;
		descriptor.hInstance     = GetModuleHandleW(nullptr);
		descriptor.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
		descriptor.lpszClassName = L"ice.native.window";
		if (!RegisterClassExW(&descriptor) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
			return false;
		UINT dpi = GetDpiForSystem();
		RECT bounds{0, 0, MulDiv(options.width, int(dpi), 96),
		            MulDiv(options.height, int(dpi), 96)};
		AdjustWindowRectExForDpi(&bounds, WS_OVERLAPPEDWINDOW, FALSE, 0, dpi);
		native_ = CreateWindowExW(0, descriptor.lpszClassName, title.c_str(), WS_OVERLAPPEDWINDOW,
		                          CW_USEDEFAULT, CW_USEDEFAULT, bounds.right - bounds.left,
		                          bounds.bottom - bounds.top, nullptr, nullptr,
		                          descriptor.hInstance, this);
		// Creation may choose a monitor with a different DPI than the primary monitor.
		const bool ready = native_ && resize(options.width, options.height);
		if (ready)
			text_input_changed();
		return ready;
	}
	void show(bool visible) override {
		if (!visible)
			cancel_input();
		ShowWindow(native_, visible ? SW_SHOW : SW_HIDE);
	}
	bool resize(int width, int height) override {
		if (!valid_extent(width, height))
			return false;
		UINT dpi = GetDpiForWindow(native_);
		RECT bounds{0, 0, MulDiv(width, int(dpi), 96), MulDiv(height, int(dpi), 96)};
		if (!AdjustWindowRectExForDpi(&bounds, DWORD(GetWindowLongPtrW(native_, GWL_STYLE)), FALSE,
		                              0, dpi))
			return false;
		return SetWindowPos(native_, nullptr, 0, 0, bounds.right - bounds.left,
		                    bounds.bottom - bounds.top,
		                    SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != 0;
	}
	void fullscreen(bool enabled) override {
		if (enabled == fullscreen_)
			return;
		if (enabled) {
			GetWindowRect(native_, &windowed_);
			MONITORINFO monitor{sizeof(MONITORINFO), {}, {}, 0};
			if (!GetMonitorInfoW(MonitorFromWindow(native_, MONITOR_DEFAULTTONEAREST), &monitor))
				return;
			SetWindowLongPtrW(native_, GWL_STYLE, WS_POPUP | WS_VISIBLE);
			SetWindowPos(native_, HWND_TOP, monitor.rcMonitor.left, monitor.rcMonitor.top,
			             monitor.rcMonitor.right - monitor.rcMonitor.left,
			             monitor.rcMonitor.bottom - monitor.rcMonitor.top, SWP_FRAMECHANGED);
		} else {
			SetWindowLongPtrW(native_, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
			SetWindowPos(native_, nullptr, windowed_.left, windowed_.top,
			             windowed_.right - windowed_.left, windowed_.bottom - windowed_.top,
			             SWP_NOZORDER | SWP_FRAMECHANGED);
		}
		fullscreen_ = enabled;
	}
	window_extent extent() const override {
		RECT bounds{};
		GetClientRect(native_, &bounds);
		float scale = float(GetDpiForWindow(native_)) / 96.f;
		if (scale <= 0)
			scale = 1;
		int width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
		return {MulDiv(width, 96, int(GetDpiForWindow(native_))),
		        MulDiv(height, 96, int(GetDpiForWindow(native_))), width, height, scale};
	}
	native_window_handle native_handle() const override {
		return {native_window_kind::win32, reinterpret_cast<std::uintptr_t>(native_), nullptr,
		        nullptr};
	}
	relative_pointer_kind relative_pointer_capability() const override {
		return relative_pointer_kind::native;
	}

private:
	void cancel_native_physical_input() override {
		pointer_valid_ = false;
		buttons_       = 0;
		if (!relative_ && GetCapture() == native_)
			ReleaseCapture();
	}
	ice::expected<void, window_diagnostic>
	change_pointer_mode(ice::platform::pointer_mode mode) override {
		if (mode == ice::platform::pointer_mode::normal) {
			if (relative_owner == this) {
				relative_owner = nullptr;
				relative_      = false;
				RAWINPUTDEVICE device{1, 2, RIDEV_REMOVE, nullptr};
				RegisterRawInputDevices(&device, 1, sizeof(device));
				ClipCursor(nullptr);
				if (GetCapture() == native_)
					ReleaseCapture();
				ShowCursor(TRUE);
				SetCursor(LoadCursorW(nullptr, IDC_ARROW));
				pointer_valid_ = false;
			}
			return {};
		}
		if (!focused_ || !IsWindowVisible(native_) || IsIconic(native_) ||
		    (relative_owner && relative_owner != this))
			return ice::unexpected(window_diagnostic{
			    "Relative pointer capture requires the focused visible Win32 window"});
		RECT bounds{};
		if (!GetClientRect(native_, &bounds) || bounds.right <= 0 || bounds.bottom <= 0)
			return ice::unexpected(window_diagnostic{"Window has no pointer capture area"});
		POINT top{bounds.left, bounds.top}, bottom{bounds.right, bounds.bottom};
		ClientToScreen(native_, &top);
		ClientToScreen(native_, &bottom);
		RECT           clip{top.x, top.y, bottom.x, bottom.y};
		RAWINPUTDEVICE device{1, 2, 0, native_};
		if (!RegisterRawInputDevices(&device, 1, sizeof(device)))
			return ice::unexpected(window_diagnostic{"Win32 raw mouse registration failed"});
		SetCapture(native_);
		if (GetCapture() != native_ || !ClipCursor(&clip)) {
			device.dwFlags    = RIDEV_REMOVE;
			device.hwndTarget = nullptr;
			RegisterRawInputDevices(&device, 1, sizeof(device));
			if (GetCapture() == native_)
				ReleaseCapture();
			return ice::unexpected(window_diagnostic{"Win32 pointer confinement failed"});
		}
		ShowCursor(FALSE);
		SetCursor(nullptr);
		POINT position{};
		if (GetCursorPos(&position) && ScreenToClient(native_, &position)) {
			auto scale = extent().scale;
			pointer_x_ = double(position.x) / scale;
			pointer_y_ = double(position.y) / scale;
		}
		relative_      = true;
		relative_owner = this;
		pointer_valid_ = false;
		return {};
	}
	void post_pointer(platform_event event) {
		event.modifiers = modifiers();
		if (!platform_events::post(std::move(event)))
			cancel_input();
	}
	void pointer_position(platform_event &event, LPARAM packed) {
		const float scale = extent().scale;
		event.x           = double(static_cast<short>(LOWORD(packed))) / scale;
		event.y           = double(static_cast<short>(HIWORD(packed))) / scale;
	}
	void cancel_native_text_input() override {
		high_surrogate_ = 0;
		composing_      = false;
		if (HIMC context = ImmGetContext(native_)) {
			ImmNotifyIME(context, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
			ImmReleaseContext(native_, context);
		}
	}
	void text_input_changed() override {
		ImmAssociateContextEx(native_, nullptr, text_input_enabled() ? IACE_DEFAULT : 0);
		if (HIMC context = ImmGetContext(native_)) {
			const auto      rect  = text_input_area();
			const auto      dpi   = GetDpiForWindow(native_);
			const float     scale = dpi ? float(dpi) / 96.f : 1.f;
			COMPOSITIONFORM composition{};
			composition.dwStyle      = CFS_POINT;
			composition.ptCurrentPos = {LONG(rect.x * scale), LONG((rect.y + rect.height) * scale)};
			ImmSetCompositionWindow(context, &composition);
			CANDIDATEFORM candidate{};
			candidate.dwStyle      = CFS_EXCLUDE;
			candidate.ptCurrentPos = composition.ptCurrentPos;
			candidate.rcArea       = {LONG(rect.x * scale), LONG(rect.y * scale),
			                          LONG((rect.x + rect.width) * scale),
			                          LONG((rect.y + rect.height) * scale)};
			ImmSetCandidateWindow(context, &candidate);
			ImmReleaseContext(native_, context);
		}
	}
	bool post_text(platform_event_type type, const std::wstring &text, std::size_t cursor = 0) {
		platform_event event{type, id(), 0};
		if (!utf8(text, event.text)) {
			cancel_text_input();
			return false;
		}
		event.modifiers = modifiers_;
		event.repeat    = repeated_;
		if (type == platform_event_type::text_composition) {
			cursor = std::min(cursor, text.size());
			if (cursor && cursor < text.size() && text[cursor - 1] >= 0xD800 &&
			    text[cursor - 1] <= 0xDBFF && text[cursor] >= 0xDC00 && text[cursor] <= 0xDFFF)
				--cursor;
			std::string prefix;
			if (!utf8(text.substr(0, cursor), prefix)) {
				cancel_text_input();
				return false;
			}
			event.selection_start = prefix.size();
		}
		if (!platform_events::post(std::move(event))) {
			cancel_native_text_input();
			return false;
		}
		return true;
	}
	void composition(LPARAM flags) {
		HIMC context = ImmGetContext(native_);
		if (!context)
			return;
		auto read = [&](DWORD kind, std::wstring &text) {
			const LONG bytes = ImmGetCompositionStringW(context, kind, nullptr, 0);
			if (bytes < 0 || bytes > 8192 || bytes % 2)
				return false;
			text.resize(std::size_t(bytes) / sizeof(wchar_t));
			return !bytes ||
			       ImmGetCompositionStringW(context, kind, text.data(), DWORD(bytes)) == bytes;
		};
		std::wstring result, preedit;
		bool         valid = true;
		if (flags & GCS_RESULTSTR)
			valid = read(GCS_RESULTSTR, result);
		if (flags & GCS_COMPSTR)
			valid = read(GCS_COMPSTR, preedit) && valid;
		const LONG cursor = ImmGetCompositionStringW(context, GCS_CURSORPOS, nullptr, 0);
		ImmReleaseContext(native_, context);
		if (!valid) {
			cancel_text_input();
			return;
		}
		if ((flags & GCS_RESULTSTR) && !post_text(platform_event_type::text_committed, result))
			return;
		if (flags & GCS_COMPSTR)
			post_text(platform_event_type::text_composition, preedit,
			          cursor < 0 ? 0 : std::size_t(cursor));
		if (!flags)
			cancel_text_input();
	}
	bool                    composing_ = false, focused_ = false, repeated_ = false;
	wchar_t                 high_surrogate_ = 0;
	key_modifiers           modifiers_      = 0;
	HWND                    native_         = nullptr;
	RECT                    windowed_{};
	bool                    fullscreen_ = false;
	bool                    relative_ = false, pointer_valid_ = false;
	double                  pointer_x_ = 0, pointer_y_ = 0;
	unsigned                buttons_ = 0;
	static LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM first, LPARAM second) {
		auto *owner = reinterpret_cast<win32_window *>(GetWindowLongPtrW(window, GWLP_USERDATA));
		if (message == WM_NCCREATE) {
			owner = static_cast<win32_window *>(
			    reinterpret_cast<CREATESTRUCTW *>(second)->lpCreateParams);
			owner->native_ = window;
			windows.emplace(window, owner);
			SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(owner));
		}
		if (!owner)
			return DefWindowProcW(window, message, first, second);
		if (message == WM_SETFOCUS)
			owner->focused_ = true;
		if (message == WM_KILLFOCUS) {
			owner->focused_ = false;
			owner->cancel_input();
		}
		if ((message == WM_KEYDOWN || message == WM_SYSKEYDOWN || message == WM_KEYUP ||
		     message == WM_SYSKEYUP) &&
		    owner->focused_) {
			platform_event event{platform_event_type::physical_key, owner->id(), 0};
			event.key       = first == VK_PAUSE ? physical_key::pause
			                                    : win32_physical_key(unsigned((second >> 16) & 255),
			                                                         (second & (LPARAM(1) << 24)) != 0);
			event.pressed   = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
			event.repeat    = event.pressed && (second & (LPARAM(1) << 30)) != 0;
			event.modifiers = modifiers();
			if (event.key != physical_key::unknown && !platform_events::post(std::move(event)))
				owner->cancel_input();
		}
		if ((message == WM_LBUTTONDOWN || message == WM_LBUTTONUP || message == WM_RBUTTONDOWN ||
		     message == WM_RBUTTONUP || message == WM_MBUTTONDOWN || message == WM_MBUTTONUP ||
		     message == WM_XBUTTONDOWN || message == WM_XBUTTONUP) &&
		    owner->focused_) {
			platform_event event{platform_event_type::pointer_button, owner->id(), 0};
			event.button =
			    (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP)   ? mouse_button::primary
			    : (message == WM_RBUTTONDOWN || message == WM_RBUTTONUP) ? mouse_button::secondary
			    : (message == WM_MBUTTONDOWN || message == WM_MBUTTONUP) ? mouse_button::middle
			    : HIWORD(first) == XBUTTON1                              ? mouse_button::back
			                                                             : mouse_button::forward;
			event.pressed = message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN ||
			                message == WM_MBUTTONDOWN || message == WM_XBUTTONDOWN;
			const unsigned bit = 1u << static_cast<unsigned>(event.button);
			if (event.pressed) {
				owner->buttons_ |= bit;
				SetCapture(window);
			} else
				owner->buttons_ &= ~bit;
			owner->pointer_position(event, second);
			owner->post_pointer(std::move(event));
			if (!owner->buttons_ && !owner->relative_ && GetCapture() == window)
				ReleaseCapture();
			return message == WM_XBUTTONDOWN || message == WM_XBUTTONUP ? TRUE : 0;
		}
		if (message == WM_MOUSEMOVE && owner->focused_ && !owner->relative_) {
			platform_event event{platform_event_type::pointer_move, owner->id(), 0};
			owner->pointer_position(event, second);
			if (owner->pointer_valid_) {
				event.delta_x = event.x - owner->pointer_x_;
				event.delta_y = event.y - owner->pointer_y_;
			}
			owner->pointer_x_     = event.x;
			owner->pointer_y_     = event.y;
			owner->pointer_valid_ = true;
			owner->post_pointer(std::move(event));
		}
		if (message == WM_INPUT && owner->focused_ && owner->relative_) {
			RAWINPUT   input{};
			UINT       size     = sizeof(input);
			const UINT received = GetRawInputData(reinterpret_cast<HRAWINPUT>(second), RID_INPUT,
			                                      &input, &size, sizeof(RAWINPUTHEADER));
			if (received == UINT(-1) || received < sizeof(RAWINPUTHEADER) + sizeof(RAWMOUSE))
				owner->cancel_input();
			else if (input.header.dwType == RIM_TYPEMOUSE &&
			         !(input.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)) {
				platform_event event{platform_event_type::pointer_move, owner->id(), 0};
				event.x           = owner->pointer_x_;
				event.y           = owner->pointer_y_;
				event.delta_x     = input.data.mouse.lLastX;
				event.delta_y     = input.data.mouse.lLastY;
				event.delta_units = pointer_delta_unit::device_counts;
				owner->post_pointer(std::move(event));
			}
		}
		if ((message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL) && owner->focused_) {
			platform_event event{platform_event_type::scroll, owner->id(), 0};
			auto           amount = double(static_cast<short>(HIWORD(first))) / WHEEL_DELTA;
			if (message == WM_MOUSEWHEEL)
				event.scroll_y = amount;
			else
				event.scroll_x = amount;
			owner->post_pointer(std::move(event));
			return 0;
		}
		if ((owner->relative_ && (message == WM_MOVE || message == WM_SIZE)) ||
		    (message == WM_CAPTURECHANGED && (owner->relative_ || owner->buttons_) &&
		     reinterpret_cast<HWND>(second) != window) ||
		    message == WM_CANCELMODE || (message == WM_SHOWWINDOW && !first) ||
		    (message == WM_SIZE && first == SIZE_MINIMIZED))
			owner->cancel_input();
		if (message == WM_SETCURSOR && owner->relative_) {
			SetCursor(nullptr);
			return TRUE;
		}
		if ((message == WM_KEYDOWN || message == WM_SYSKEYDOWN) && owner->focused_) {
			// A surrogate pair belongs to one translated key action. Never carry an
			// incomplete pair into the next editing command or unrelated key press.
			if (owner->high_surrogate_)
				owner->cancel_text_input();
			owner->modifiers_ = modifiers();
			owner->repeated_  = (second & (LPARAM(1) << 30)) != 0;
			platform_event activity{platform_event_type::input_activity, owner->id(), 0};
			activity.modifiers = owner->modifiers_;
			activity.repeat    = owner->repeated_;
			if (!platform_events::post(std::move(activity)))
				owner->cancel_native_text_input();
			if (owner->text_input_enabled() && !owner->composing_ && first != VK_PROCESSKEY) {
				const auto command = command_for(first, owner->modifiers_);
				if (command != edit_command::none) {
					platform_event event{platform_event_type::edit_command, owner->id(), 0};
					event.command   = command;
					event.modifiers = owner->modifiers_;
					event.repeat    = owner->repeated_;
					if (!platform_events::post(std::move(event)))
						owner->cancel_native_text_input();
					return 0;
				}
			}
		}
		if (message == WM_UNICHAR && first == UNICODE_NOCHAR)
			return TRUE;
		if (message == WM_CHAR || message == WM_UNICHAR) {
			if (!owner->text_input_enabled() || !owner->focused_)
				return 0;
			if (message == WM_UNICHAR) {
				if (owner->high_surrogate_)
					owner->cancel_text_input();
				if (first > 0x10FFFF || (first >= 0xD800 && first <= 0xDFFF)) {
					owner->cancel_text_input();
					return 0;
				}
			}
			if (first < 32 || first == 127)
				return 0;
			if (message == WM_UNICHAR && first > 0xFFFF && first <= 0x10FFFF) {
				const auto point = first - 0x10000;
				owner->post_text(
				    platform_event_type::text_committed,
				    {wchar_t(0xD800 + (point >> 10)), wchar_t(0xDC00 + (point & 1023))});
			} else if (first >= 0xD800 && first <= 0xDBFF) {
				if (owner->high_surrogate_)
					owner->cancel_text_input();
				owner->high_surrogate_ = wchar_t(first);
			} else {
				std::wstring text;
				if (owner->high_surrogate_)
					text.push_back(owner->high_surrogate_);
				owner->high_surrogate_ = 0;
				if (first > 0xFFFF)
					owner->cancel_text_input();
				else {
					text.push_back(wchar_t(first));
					owner->post_text(platform_event_type::text_committed, text);
				}
			}
			return 0;
		}
		if (message == WM_IME_SETCONTEXT && owner->text_input_enabled())
			return DefWindowProcW(window, message, first, second & ~ISC_SHOWUICOMPOSITIONWINDOW);
		if (message == WM_IME_STARTCOMPOSITION) {
			owner->composing_ = true;
			owner->text_input_changed();
			return 0;
		}
		if (message == WM_IME_COMPOSITION) {
			if (owner->text_input_enabled() && owner->focused_)
				owner->composition(second);
			return 0;
		}
		if (message == WM_IME_ENDCOMPOSITION) {
			owner->composing_ = false;
			if (owner->text_input_enabled() && owner->focused_)
				owner->post_text(platform_event_type::text_composition, {});
			return 0;
		}
		if (message == WM_IME_CHAR)
			return 0;
		if (message == WM_CLOSE) {
			owner->request_close();
			return 0;
		}
		if (message == WM_SETFOCUS || message == WM_KILLFOCUS || message == WM_SIZE) {
			auto type = message == WM_SIZE       ? platform_event_type::window_resized
			            : message == WM_SETFOCUS ? platform_event_type::focus_gained
			                                     : platform_event_type::focus_lost;
			platform_events::post(type, owner->id());
		}
		if (message == WM_DPICHANGED) {
			auto *bounds = reinterpret_cast<const RECT *>(second);
			SetWindowPos(window, nullptr, bounds->left, bounds->top, bounds->right - bounds->left,
			             bounds->bottom - bounds->top, SWP_NOZORDER | SWP_NOACTIVATE);
			platform_events::post(platform_event_type::window_resized, owner->id());
			return 0;
		}
		if (message == WM_PAINT) {
			PAINTSTRUCT paint;
			HDC         context = BeginPaint(window, &paint);
			FillRect(context, &paint.rcPaint, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
			EndPaint(window, &paint);
			return 0;
		}
		if (message == WM_NCDESTROY) {
			windows.erase(window);
			owner->native_ = nullptr;
			SetWindowLongPtrW(window, GWLP_USERDATA, 0);
		}
		return DefWindowProcW(window, message, first, second);
	}
};
} // namespace
ice::expected<std::unique_ptr<desktop_window>, window_diagnostic>
create_desktop_window(const desktop_window_options &options) {
	// Set before the first window. Failure may mean the embedding application already chose a mode.
	if (windows.empty())
		SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	auto result = std::make_unique<win32_window>();
	if (!result->initialize(options))
		return ice::unexpected(window_diagnostic{"Win32 window creation failed"});
	result->show(options.visible);
	return std::unique_ptr<desktop_window>(std::move(result));
}
void pump_desktop_events() {
	MSG message;
	while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
		if (message.message == WM_QUIT) {
			for (const auto &[native, window] : windows) {
				(void)native;
				window->request_close();
			}
			platform_events::post(platform_event_type::application_quit_requested);
		} else {
			TranslateMessage(&message);
			DispatchMessageW(&message);
		}
	}
}
} // namespace ice::platform::detail
