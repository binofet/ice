#include "native_context.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <atomic>
#include <dlfcn.h>
#include <limits>
#include <mutex>

namespace ice::render::gl::detail {
namespace {
struct display_owner {
	EGLDisplay display = EGL_NO_DISPLAY;
	~display_owner() {
		if (display != EGL_NO_DISPLAY)
			eglTerminate(display);
	}
};
ice::expected<std::shared_ptr<display_owner>, std::string> shared_display() {
	// EGL initialization is display-wide, not per context: terminate only after the last
	// resource/context owner retires, including independently created logical devices.
	static std::mutex                   mutex;
	static std::weak_ptr<display_owner> cached;
	std::lock_guard                     lock(mutex);
	if (auto existing = cached.lock())
		return existing;
	auto       result  = std::make_shared<display_owner>();
	EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (display == EGL_NO_DISPLAY || !eglInitialize(display, nullptr, nullptr)) {
#ifndef __ANDROID__
		// Mesa's headless platform permits software GPU regressions without an X server.
		auto platform = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
		    eglGetProcAddress("eglGetPlatformDisplayEXT"));
		display = platform ? platform(0x31DD /* EGL_PLATFORM_SURFACELESS_MESA */,
		                              EGL_DEFAULT_DISPLAY, nullptr)
		                   : EGL_NO_DISPLAY;
		if (display == EGL_NO_DISPLAY || !eglInitialize(display, nullptr, nullptr))
#endif
			return ice::unexpected(std::string("Cannot initialize native EGL display: ") +
			                       std::to_string(eglGetError()));
	}
	result->display = display;
	cached          = result;
	return result;
}
class egl_context final : public native_context {
public:
	std::shared_ptr<display_owner> owner;
	EGLContext                     context  = EGL_NO_CONTEXT;
	EGLSurface                     surface  = EGL_NO_SURFACE;
	EGLConfig                      config   = nullptr;
	std::uint64_t                  identity = 0;
	void                          *library  = nullptr;
	~egl_context() override {
		if (owner) {
			if (eglGetCurrentContext() == context)
				eglMakeCurrent(owner->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
			if (context != EGL_NO_CONTEXT)
				eglDestroyContext(owner->display, context);
			if (surface != EGL_NO_SURFACE)
				eglDestroySurface(owner->display, surface);
		}
		if (library)
			dlclose(library);
	}
	bool make_current() override {
		return eglMakeCurrent(owner->display, surface, surface, context) == EGL_TRUE;
	}
	void resize(int, int) override {}
	bool present() override { return false; } // No implicit Android/native swap or runtime release.
	raw_proc resolve(const char *name) override {
		auto result = reinterpret_cast<raw_proc>(eglGetProcAddress(name));
		return result    ? result
		       : library ? reinterpret_cast<raw_proc>(dlsym(library, name))
		                 : nullptr;
	}
	egl_binding egl() const override { return {owner->display, config, context, identity}; }
};
} // namespace
ice::expected<std::shared_ptr<native_context>, std::string> create_gles_context() {
	auto display = shared_display();
	if (!display)
		return ice::unexpected(display.error());
	auto result   = std::make_shared<egl_context>();
	result->owner = std::move(*display);
	if (!eglBindAPI(EGL_OPENGL_ES_API))
		return ice::unexpected(std::string("Cannot select native OpenGL ES API"));
	const EGLint attributes[]{EGL_SURFACE_TYPE,
	                          EGL_PBUFFER_BIT,
	                          EGL_RENDERABLE_TYPE,
	                          EGL_OPENGL_ES3_BIT_KHR,
	                          EGL_RED_SIZE,
	                          8,
	                          EGL_GREEN_SIZE,
	                          8,
	                          EGL_BLUE_SIZE,
	                          8,
	                          EGL_ALPHA_SIZE,
	                          8,
	                          EGL_NONE};
	EGLint       count = 0;
	if (!eglChooseConfig(result->owner->display, attributes, &result->config, 1, &count) ||
	    count != 1)
		return ice::unexpected(std::string("No EGL ES3 RGBA8 pbuffer configuration"));
	const EGLint size[]{EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
	result->surface = eglCreatePbufferSurface(result->owner->display, result->config, size);
	const EGLint version[]{EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
	result->context =
	    eglCreateContext(result->owner->display, result->config, EGL_NO_CONTEXT, version);
	if (result->surface == EGL_NO_SURFACE || result->context == EGL_NO_CONTEXT ||
	    !result->make_current())
		return ice::unexpected(std::string("Cannot create/activate EGL ES3 root context: ") +
		                       std::to_string(eglGetError()));
	static std::atomic<std::uint64_t> next{1};
	auto                              identity = next.load(std::memory_order_relaxed);
	do {
		if (identity == std::numeric_limits<std::uint64_t>::max())
			return ice::unexpected(std::string("EGL context identities exhausted"));
	} while (!next.compare_exchange_weak(identity, identity + 1, std::memory_order_relaxed));
	result->identity = identity;
	result->library  = dlopen("libGLESv2.so", RTLD_NOW | RTLD_LOCAL);
	return std::static_pointer_cast<native_context>(result);
}
} // namespace ice::render::gl::detail
