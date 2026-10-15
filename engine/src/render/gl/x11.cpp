#include "native_context.h"
#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/Xlib.h>
#include <cstring>
namespace ice::render::gl::detail {
namespace {
struct connection {
	Display    *display = nullptr;
	GLXFBConfig format  = nullptr;
	~connection() {
		if (display)
			XCloseDisplay(display);
	}
};
// GLX reports unsupported context attributes through Xlib. Keep this short process-wide
// trap on the UI thread, synchronizing before restoration; unrelated display errors retain
// the previous handler. It is not an X-server-loss containment mechanism.
Display      *trapped_display  = nullptr;
bool          creation_failed  = false;
XErrorHandler previous_handler = nullptr;
int           context_error(Display *display, XErrorEvent *event) {
    if (display == trapped_display) {
        creation_failed = true;
        return 0;
    }
    return previous_handler ? previous_handler(display, event) : 0;
}
class x11_context final : public native_context {
public:
	std::shared_ptr<connection>     server;
	std::shared_ptr<native_context> shared;
	GLXContext                      context  = nullptr;
	GLXPbuffer                      pbuffer  = 0;
	Window                          child    = 0;
	Colormap                        colormap = 0;
	~x11_context() override {
		if (!server || !server->display)
			return;
		auto *display = server->display;
		if (glXGetCurrentContext() == context)
			glXMakeContextCurrent(display, None, None, nullptr);
		if (context)
			glXDestroyContext(display, context);
		if (pbuffer)
			glXDestroyPbuffer(display, pbuffer);
		if (child)
			XDestroyWindow(display, child);
		if (colormap)
			XFreeColormap(display, colormap);
		// Complete child destruction before the host may destroy its parent on another
		// Xlib connection; otherwise the server can process parent destruction first.
		XSync(display, False);
	}
	bool make_current() override {
		GLXDrawable drawable = child ? child : pbuffer;
		return glXMakeContextCurrent(server->display, drawable, drawable, context) != 0;
	}
	void resize(int width, int height) override {
		if (child && width > 0 && height > 0) {
			XResizeWindow(server->display, child, unsigned(width), unsigned(height));
			XFlush(server->display);
		}
	}
	bool present() override {
		glXSwapBuffers(server->display, child);
		return true;
	}
	raw_proc resolve(const char *name) override {
		return glXGetProcAddressARB(reinterpret_cast<const GLubyte *>(name));
	}
};
} // namespace
ice::expected<std::shared_ptr<native_context>, std::string>
create_context(std::shared_ptr<native_context>            shared,
               const ice::platform::native_window_handle *target) {
	auto parent    = std::dynamic_pointer_cast<x11_context>(shared);
	auto result    = std::make_shared<x11_context>();
	result->shared = std::move(shared);
	if (parent)
		result->server = parent->server;
	else {
		result->server          = std::make_shared<connection>();
		result->server->display = XOpenDisplay(nullptr);
		if (!result->server->display)
			return ice::unexpected(std::string("Cannot open X11 display for GLX"));
		int  attributes[] = {GLX_X_RENDERABLE,
		                     True,
		                     GLX_DRAWABLE_TYPE,
		                     GLX_WINDOW_BIT | GLX_PBUFFER_BIT,
		                     GLX_RENDER_TYPE,
		                     GLX_RGBA_BIT,
		                     GLX_RED_SIZE,
		                     8,
		                     GLX_GREEN_SIZE,
		                     8,
		                     GLX_BLUE_SIZE,
		                     8,
		                     GLX_ALPHA_SIZE,
		                     8,
		                     GLX_DOUBLEBUFFER,
		                     True,
		                     None};
		int  count        = 0;
		auto formats      = glXChooseFBConfig(
            result->server->display, DefaultScreen(result->server->display), attributes, &count);
		if (!formats || !count) {
			if (formats)
				XFree(formats);
			return ice::unexpected(std::string("No GLX window/pbuffer format"));
		}
		result->server->format = formats[0];
		XFree(formats);
	}
	auto *display = result->server->display;
	auto  create  = reinterpret_cast<PFNGLXCREATECONTEXTATTRIBSARBPROC>(
        glXGetProcAddressARB(reinterpret_cast<const GLubyte *>("glXCreateContextAttribsARB")));
	if (!create)
		return ice::unexpected(std::string("GLX_ARB_create_context is unavailable"));
	int attributes[] = {GLX_CONTEXT_MAJOR_VERSION_ARB,
	                    4,
	                    GLX_CONTEXT_MINOR_VERSION_ARB,
	                    1,
	                    GLX_CONTEXT_PROFILE_MASK_ARB,
	                    GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
	                    None};
	XSync(display, False);
	trapped_display  = display;
	creation_failed  = false;
	previous_handler = XSetErrorHandler(context_error);
	result->context  = create(display, result->server->format, parent ? parent->context : nullptr,
	                          True, attributes);
	XSync(display, False);
	XSetErrorHandler(previous_handler);
	trapped_display = nullptr;
	if (!result->context || creation_failed)
		return ice::unexpected(std::string("GLX OpenGL 4.1 core creation failed"));
	if (target) {
		if (target->kind != ice::platform::native_window_kind::x11 || !target->window)
			return ice::unexpected(std::string("GLX presentation requires an X11 window"));
		// The host owns a separate Xlib connection. Process its queued creation before
		// this connection refers to the parent XID; flushing alone does not order clients.
		if (!target->display)
			return ice::unexpected(
			    std::string("GLX presentation requires the host X11 connection"));
		XSync(static_cast<Display *>(target->display), False);
		auto visual = glXGetVisualFromFBConfig(display, result->server->format);
		if (!visual)
			return ice::unexpected(std::string("GLX visual selection failed"));
		result->colormap =
		    XCreateColormap(display, Window(target->window), visual->visual, AllocNone);
		XSetWindowAttributes settings{};
		settings.colormap     = result->colormap;
		settings.border_pixel = 0;
		result->child =
		    XCreateWindow(display, Window(target->window), 0, 0, 1, 1, 0, visual->depth,
		                  InputOutput, visual->visual, CWColormap | CWBorderPixel, &settings);
		XFree(visual);
		if (!result->child)
			return ice::unexpected(std::string("GLX child presentation window failed"));
		XMapWindow(display, result->child);
		XFlush(display);
	} else {
		int size[]      = {GLX_PBUFFER_WIDTH, 1, GLX_PBUFFER_HEIGHT, 1, None};
		result->pbuffer = glXCreatePbuffer(display, result->server->format, size);
		if (!result->pbuffer)
			return ice::unexpected(std::string("GLX preparation pbuffer creation failed"));
	}
	if (!result->make_current())
		return ice::unexpected(std::string("Cannot make GLX context current"));
	return std::shared_ptr<native_context>(std::move(result));
}
} // namespace ice::render::gl::detail
