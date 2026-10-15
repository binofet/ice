#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "../desktop_window_internal.h"
#include <map>
#include <vector>
#include <windows.h>

namespace ice::platform::detail {
namespace {
class win32_window;
std::map<HWND, win32_window *> windows;
class win32_window             final : public desktop_window {
public:
    ~win32_window() override {
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
        return native_ && resize(options.width, options.height);
    }
    void show(bool visible) override { ShowWindow(native_, visible ? SW_SHOW : SW_HIDE); }
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

private:
    HWND                    native_ = nullptr;
    RECT                    windowed_{};
    bool                    fullscreen_ = false;
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
