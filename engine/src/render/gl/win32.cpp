#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "native_context.h"
#include <GL/gl.h>
#include <cstdint>
namespace ice::render::gl::detail {
namespace {
using create_attribs = HGLRC(WINAPI *)(HDC, HGLRC, const int *);
class win32_context final : public native_context {
public:
	HWND                            window   = nullptr;
	HDC                             drawable = nullptr;
	HGLRC                           context  = nullptr;
	int                             format   = 0;
	bool                            child    = false;
	std::shared_ptr<native_context> shared;
	~win32_context() override {
		if (context && wglGetCurrentContext() == context)
			wglMakeCurrent(nullptr, nullptr);
		if (context)
			wglDeleteContext(context);
		if (drawable)
			ReleaseDC(window, drawable);
		if (window)
			DestroyWindow(window);
	}
	bool make_current() override { return wglMakeCurrent(drawable, context) != 0; }
	void resize(int width, int height) override {
		if (child && width > 0 && height > 0)
			SetWindowPos(window, nullptr, 0, 0, width, height, SWP_NOACTIVATE | SWP_NOZORDER);
	}
	bool     present() override { return SwapBuffers(drawable) != 0; }
	raw_proc resolve(const char *name) override {
		PROC entry = wglGetProcAddress(name);
		auto value = reinterpret_cast<std::uintptr_t>(entry);
		if (!entry || value <= 3 || value == UINTPTR_MAX)
			entry = GetProcAddress(GetModuleHandleW(L"opengl32.dll"), name);
		return reinterpret_cast<raw_proc>(entry);
	}
};
} // namespace
ice::expected<std::shared_ptr<native_context>, std::string>
create_context(std::shared_ptr<native_context>            shared,
               const ice::platform::native_window_handle *target) {
	// Device preparation may create the first HWND before the host creates a window.
	// Select the same DPI contract as the desktop layer before that bootstrap HWND exists.
	if (!shared)
		SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	auto parent    = std::dynamic_pointer_cast<win32_context>(shared);
	auto result    = std::make_shared<win32_context>();
	result->shared = std::move(shared);
	WNDCLASSEXW descriptor{};
	descriptor.cbSize        = sizeof(descriptor);
	descriptor.style         = CS_OWNDC;
	descriptor.lpfnWndProc   = DefWindowProcW;
	descriptor.hInstance     = GetModuleHandleW(nullptr);
	descriptor.lpszClassName = L"ice.gl.core.surface";
	if (!RegisterClassExW(&descriptor) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
		return ice::unexpected(std::string("WGL window class registration failed"));
	if (target && (target->kind != ice::platform::native_window_kind::win32 || !target->window))
		return ice::unexpected(std::string("WGL presentation requires a Win32 window"));
	result->child = target != nullptr;
	result->window =
	    CreateWindowExW(0, descriptor.lpszClassName, L"", target ? WS_CHILD | WS_VISIBLE : WS_POPUP,
	                    0, 0, 1, 1, target ? reinterpret_cast<HWND>(target->window) : nullptr,
	                    nullptr, descriptor.hInstance, nullptr);
	if (!result->window || !(result->drawable = GetDC(result->window)))
		return ice::unexpected(std::string("WGL drawable creation failed"));
	PIXELFORMATDESCRIPTOR pixels{};
	pixels.nSize      = sizeof(pixels);
	pixels.nVersion   = 1;
	pixels.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pixels.iPixelType = PFD_TYPE_RGBA;
	pixels.cColorBits = 24;
	pixels.cAlphaBits = 8;
	result->format    = parent ? parent->format : ChoosePixelFormat(result->drawable, &pixels);
	if (!result->format || !SetPixelFormat(result->drawable, result->format, &pixels))
		return ice::unexpected(std::string("WGL pixel format selection failed"));
	HGLRC bootstrap = nullptr;
	if (parent) {
		if (!parent->make_current())
			return ice::unexpected(std::string("Cannot activate WGL share group"));
	} else {
		// A temporary bootstrap context discovers WGL's modern context factory. No fixed-function
		// commands are issued; all retained contexts require a 4.1 core profile.
		bootstrap = wglCreateContext(result->drawable);
		if (!bootstrap || !wglMakeCurrent(result->drawable, bootstrap)) {
			if (bootstrap)
				wglDeleteContext(bootstrap);
			return ice::unexpected(std::string("WGL bootstrap creation failed"));
		}
	}
	auto create = reinterpret_cast<create_attribs>(result->resolve("wglCreateContextAttribsARB"));
	constexpr int major = 0x2091, minor = 0x2092, profile = 0x9126, core = 1;
	int           attributes[] = {major, 4, minor, 1, profile, core, 0};
	if (create)
		result->context = create(result->drawable, parent ? parent->context : nullptr, attributes);
	if (bootstrap) {
		wglMakeCurrent(nullptr, nullptr);
		wglDeleteContext(bootstrap);
	}
	if (!result->context || !result->make_current())
		return ice::unexpected(std::string("WGL OpenGL 4.1 core creation failed"));
	return std::shared_ptr<native_context>(std::move(result));
}
} // namespace ice::render::gl::detail
