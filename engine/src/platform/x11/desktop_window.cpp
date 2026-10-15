#include "../desktop_window_internal.h"
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <map>

namespace ice::platform::detail {
namespace {
class x11_window;
struct connection {
	Display                       *display   = nullptr;
	Atom                           close     = 0;
	Atom                           protocols = 0;
	std::map<Window, x11_window *> windows;
	~connection() {
		if (display)
			XCloseDisplay(display);
	}
};
std::weak_ptr<connection> current;
class x11_window          final : public desktop_window {
public:
    x11_window(std::shared_ptr<connection> connection, Window window)
        : connection_(std::move(connection)), native_(window) {
        connection_->windows.emplace(native_, this);
    }
    ~x11_window() override {
        connection_->windows.erase(native_);
        if (native_)
            XDestroyWindow(connection_->display, native_);
        XFlush(connection_->display);
    }
    void show(bool visible) override {
        if (!native_)
            return;
        if (visible)
            XMapWindow(connection_->display, native_);
        else
            XUnmapWindow(connection_->display, native_);
        XFlush(connection_->display);
    }
    bool resize(int width, int height) override {
        if (!native_ || !valid_extent(width, height))
            return false;
        XResizeWindow(connection_->display, native_, unsigned(width), unsigned(height));
        XFlush(connection_->display);
        return true;
    }
    void fullscreen(bool enabled) override {
        if (!native_)
            return;
        XEvent event{};
        event.type           = ClientMessage;
        event.xclient.window = native_;
        event.xclient.message_type = XInternAtom(connection_->display, "_NET_WM_STATE", False);
        event.xclient.format    = 32;
        event.xclient.data.l[0] = enabled ? 1 : 0;
        event.xclient.data.l[1] =
            static_cast<long>(XInternAtom(connection_->display, "_NET_WM_STATE_FULLSCREEN", False));
        event.xclient.data.l[3] = 1;
        XSendEvent(connection_->display, DefaultRootWindow(connection_->display), False,
		                    SubstructureRedirectMask | SubstructureNotifyMask, &event);
        XFlush(connection_->display);
    }
    window_extent extent() const override {
        if (!native_)
            return {};
        XWindowAttributes attributes{};
        if (!XGetWindowAttributes(connection_->display, native_, &attributes))
            return {};
        return {attributes.width, attributes.height, attributes.width, attributes.height, 1};
    }
    native_window_handle native_handle() const override {
        return {native_window_kind::x11, std::uintptr_t(native_), nullptr, connection_->display};
    }
    void event(const XEvent &event) {
        if (event.type == ClientMessage && event.xclient.format == 32 &&
            event.xclient.message_type == connection_->protocols &&
            Atom(event.xclient.data.l[0]) == connection_->close)
            request_close();
        if (event.type == ConfigureNotify)
            platform_events::post(platform_event_type::window_resized, id());
        if (event.type == FocusIn)
            platform_events::post(platform_event_type::focus_gained, id());
        if (event.type == FocusOut)
            platform_events::post(platform_event_type::focus_lost, id());
        if (event.type == DestroyNotify) {
            connection_->windows.erase(native_);
            native_ = 0;
            request_close();
        }
    }

private:
    std::shared_ptr<connection> connection_;
    Window                      native_ = 0;
};
} // namespace
ice::expected<std::unique_ptr<desktop_window>, window_diagnostic>
create_desktop_window(const desktop_window_options &options) {
	auto shared = current.lock();
	if (!shared) {
		shared          = std::make_shared<connection>();
		shared->display = XOpenDisplay(nullptr);
		if (!shared->display)
			return ice::unexpected(window_diagnostic{"Cannot connect to the X11 display"});
		shared->close     = XInternAtom(shared->display, "WM_DELETE_WINDOW", False);
		shared->protocols = XInternAtom(shared->display, "WM_PROTOCOLS", False);
		current           = shared;
	}
	Window native =
	    XCreateSimpleWindow(shared->display, DefaultRootWindow(shared->display), 0, 0,
	                        unsigned(options.width), unsigned(options.height), 0, 0,
	                        BlackPixel(shared->display, DefaultScreen(shared->display)));
	if (!native)
		return ice::unexpected(window_diagnostic{"X11 window creation failed"});
	XSelectInput(shared->display, native, StructureNotifyMask | FocusChangeMask | ExposureMask);
	XSetWMProtocols(shared->display, native, &shared->close, 1);
	XStoreName(shared->display, native, options.title.c_str());
	XChangeProperty(shared->display, native, XInternAtom(shared->display, "_NET_WM_NAME", False),
	                XInternAtom(shared->display, "UTF8_STRING", False), 8, PropModeReplace,
	                reinterpret_cast<const unsigned char *>(options.title.data()),
	                int(options.title.size()));
	auto result = std::make_unique<x11_window>(shared, native);
	result->show(options.visible);
	return std::unique_ptr<desktop_window>(std::move(result));
}
void pump_desktop_events() {
	auto shared = current.lock();
	if (!shared)
		return;
	while (XPending(shared->display)) {
		XEvent event;
		XNextEvent(shared->display, &event);
		auto owner = shared->windows.find(event.xany.window);
		if (owner != shared->windows.end())
			owner->second->event(event);
	}
}
} // namespace ice::platform::detail
