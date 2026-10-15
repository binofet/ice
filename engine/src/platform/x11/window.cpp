#include "ice/platform/x11/window.h"
#include "ice/input/input.h"
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <map>
namespace ice::platform::x11 {
static Display                     *display = nullptr;
static std::map<::Window, window *> windows;
static window                      *focused           = nullptr;
static bool                         capture_requested = false;
static window                      *captured          = nullptr;
static int                          ice_key(KeySym key) {
    if (key >= XK_a && key <= XK_z)
        return ice::input::key_a + key - XK_a;
    if (key >= XK_A && key <= XK_Z)
        return ice::input::key_a + key - XK_A;
    if (key >= XK_0 && key <= XK_9)
        return ice::input::key_0 + key - XK_0;
    if (key >= XK_F1 && key <= XK_F12)
        return ice::input::key_f1 + key - XK_F1;
    switch (key) {
    case XK_Escape:
        return ice::input::key_escape;
    case XK_Return:
    case XK_KP_Enter:
        return ice::input::key_return;
    case XK_space:
        return ice::input::key_space;
    case XK_Tab:
        return ice::input::key_tab;
    case XK_BackSpace:
        return ice::input::key_back;
    case XK_Left:
        return ice::input::key_left;
    case XK_Right:
        return ice::input::key_right;
    case XK_Up:
        return ice::input::key_up;
    case XK_Down:
        return ice::input::key_down;
    case XK_Shift_L:
    case XK_Shift_R:
        return ice::input::key_shift;
    case XK_Control_L:
    case XK_Control_R:
        return ice::input::key_control;
    case XK_Alt_L:
    case XK_Alt_R:
        return ice::input::key_menu;
    case XK_Home:
        return ice::input::key_home;
    case XK_End:
        return ice::input::key_end;
    case XK_Page_Up:
        return ice::input::key_pg_up;
    case XK_Page_Down:
        return ice::input::key_pg_dn;
    case XK_Delete:
        return ice::input::key_delete;
    default:
        return -1;
    }
}
window::window(const ice::platform::window_def &win_def, ice::platform::window *parent)
    : ice::platform::window(win_def, parent) {
	if (win_def.width <= 0 || win_def.height <= 0)
		return;
	if (!display)
		display = XOpenDisplay(nullptr);
	if (!display) {
		ice::warning("Cannot open X display");
		return;
	}
	display_         = display;
	int screen       = DefaultScreen(display);
	int attributes[] = {
	    GLX_RGBA, GLX_DOUBLEBUFFER, GLX_RED_SIZE, 8,   GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE,
	    8,        GLX_DEPTH_SIZE,   24,           None};
	visual_ = glXChooseVisual(display, screen, attributes);
	if (!visual_)
		return;
	colormap_ = XCreateColormap(display, RootWindow(display, screen), visual_->visual, AllocNone);
	XSetWindowAttributes values = {};
	values.colormap             = colormap_;
	values.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask | KeyReleaseMask |
	                    ButtonPressMask | ButtonReleaseMask | PointerMotionMask | FocusChangeMask;
	window_ = XCreateWindow(display, RootWindow(display, screen), win_def.pos_x, win_def.pos_y,
	                        win_def.width, win_def.height, 0, visual_->depth, InputOutput,
	                        visual_->visual, CWColormap | CWEventMask, &values);
	if (!window_)
		return;
	windows[window_] = this;
	delete_          = XInternAtom(display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(display, window_, &delete_, 1);
	XClassHint hint = {const_cast<char *>("ice"), const_cast<char *>("ICE")};
	XSetClassHint(display, window_, &hint);
	if (auto owner = dynamic_cast<window *>(parent))
		XSetTransientForHint(display, window_, owner->get_ref());
	char   blank  = 0;
	Pixmap pixmap = XCreateBitmapFromData(display, window_, &blank, 1, 1);
	XColor color_ = {};
	cursor_       = XCreatePixmapCursor(display, pixmap, pixmap, &color_, &color_, 0, 0);
	XFreePixmap(display, pixmap);
	update_params(win_def);
}
window::~window() { cleanup(); }
ice::result window::cleanup() {
	if (!display_)
		return IC_OK;
	if (focused == this)
		focused = nullptr;
	if (captured == this) {
		XUngrabPointer(display_, CurrentTime);
		captured = nullptr;
	}
	destroy_gl_context();
	windows.erase(window_);
	if (window_ && !destroyed_)
		XDestroyWindow(display_, window_);
	if (cursor_)
		XFreeCursor(display_, cursor_);
	if (colormap_)
		XFreeColormap(display_, colormap_);
	if (visual_)
		XFree(visual_);
	window_   = 0;
	cursor_   = 0;
	colormap_ = 0;
	visual_   = nullptr;
	display_  = nullptr;
	if (windows.empty()) {
		XCloseDisplay(display);
		display = nullptr;
	}
	return IC_OK;
}
ice::result window::show() {
	if (!window_)
		return IC_FAIL_CREATE_WIND;
	XMapRaised(display_, window_);
	XFlush(display_);
	return IC_OK;
}
ice::result window::hide() {
	if (!window_)
		return IC_FAIL_CREATE_WIND;
	XUnmapWindow(display_, window_);
	XFlush(display_);
	return IC_OK;
}
ice::result window::update_params(const ice::platform::window_def &win_def) {
	if (!window_ || win_def.width <= 0 || win_def.height <= 0)
		return IC_FAIL_GEN;
	XStoreName(display_, window_, win_def.window_name ? win_def.window_name : "ICE");
	Atom state                 = XInternAtom(display_, "_NET_WM_STATE", False),
	     fullscreen            = XInternAtom(display_, "_NET_WM_STATE_FULLSCREEN", False);
	XEvent event               = {};
	event.xclient.type         = ClientMessage;
	event.xclient.window       = window_;
	event.xclient.message_type = state;
	event.xclient.format       = 32;
	event.xclient.data.l[0]    = win_def.full_screen ? 1 : 0;
	event.xclient.data.l[1]    = fullscreen;
	event.xclient.data.l[3]    = 1;
	XSendEvent(display_, DefaultRootWindow(display_), False,
	           SubstructureRedirectMask | SubstructureNotifyMask, &event);
	int screen = DefaultScreen(display_);
	XMoveResizeWindow(display_, window_, win_def.full_screen ? 0 : win_def.pos_x,
	                  win_def.full_screen ? 0 : win_def.pos_y,
	                  win_def.full_screen ? DisplayWidth(display_, screen) : win_def.width,
	                  win_def.full_screen ? DisplayHeight(display_, screen) : win_def.height);
	window_info_ = win_def;
	XSync(display_, False);
	return IC_OK;
}
ice::result window::get_window_pos(ice::math::ivec2 *position) {
	if (!position || !window_)
		return IC_FAIL_NPE;
	::Window child;
	XTranslateCoordinates(display_, window_, DefaultRootWindow(display_), 0, 0, &position->x,
	                      &position->y, &child);
	return IC_OK;
}
void window::get_client_size(int *width, int *height) const {
	XWindowAttributes attributes = {};
	if (window_)
		XGetWindowAttributes(display_, window_, &attributes);
	if (width)
		*width = attributes.width;
	if (height)
		*height = attributes.height;
}
void *window::create_gl_context(void *shared) {
	if (context_)
		return context_;
	if (!visual_)
		return nullptr;
	context_ = glXCreateContext(display_, visual_, (GLXContext)shared, True);
	make_current();
	using SwapInterval = void (*)(Display *, GLXDrawable, int);
	auto swap          = (SwapInterval)glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalEXT");
	if (swap &&
	    strstr(glXQueryExtensionsString(display_, DefaultScreen(display_)), "GLX_EXT_swap_control"))
		swap(display_, window_, 1);
	return context_;
}
void window::destroy_gl_context() {
	if (!context_)
		return;
	if (glXGetCurrentContext() == context_)
		glXMakeCurrent(display_, None, nullptr);
	glXDestroyContext(display_, context_);
	context_ = nullptr;
}
void window::make_current() {
	if (context_)
		glXMakeCurrent(display_, window_, context_);
}
void window::swap_buffers() {
	if (context_)
		glXSwapBuffers(display_, window_);
}
void window::set_cursor_captured(bool capture) {
	capture_requested = capture;
	if (captured) {
		XUngrabPointer(display, CurrentTime);
		XUndefineCursor(display, captured->window_);
		captured = nullptr;
	}
	if (capture && focused) {
		int result =
		    XGrabPointer(display, focused->window_, True,
		                 PointerMotionMask | ButtonPressMask | ButtonReleaseMask, GrabModeAsync,
		                 GrabModeAsync, focused->window_, focused->cursor_, CurrentTime);
		if (result == GrabSuccess) {
			captured = focused;
			int width, height;
			focused->get_client_size(&width, &height);
			warp_cursor(width / 2, height / 2);
		}
	}
}
void window::warp_cursor(short x, short y) {
	if (focused) {
		XWarpPointer(display, None, focused->window_, 0, 0, 0, 0, x, y);
		XFlush(display);
	}
	ice::input::input::instance()->reset_mouse_tracking();
}
void window::pump_events() {
	if (!display)
		return;
	while (XPending(display)) {
		XEvent event;
		XNextEvent(display, &event);
		auto found = windows.find(event.xany.window);
		if (found == windows.end())
			continue;
		auto window = found->second;
		if (window->handle_native_event(event))
			continue;
		auto input = ice::input::input::instance();
		int  width, height;
		window->get_client_size(&width, &height);
		switch (event.type) {
		case ClientMessage:
			if ((Atom)event.xclient.data.l[0] == window->delete_)
				window->request_close();
			break;
		case DestroyNotify:
			window->destroyed_ = true;
			window->request_close();
			break;
		case FocusIn:
			ice::platform::platform_events::post(ice::platform::platform_event_type::focus_gained,
			                                     window->get_id());
			focused = window;
			input->reset_mouse_tracking();
			set_cursor_captured(capture_requested);
			break;
		case FocusOut:
			ice::platform::platform_events::post(ice::platform::platform_event_type::focus_lost,
			                                     window->get_id());
			if (focused == window)
				focused = nullptr;
			set_cursor_captured(capture_requested);
			input->cancel_held_input();
			break;
		case KeyPress:
		case KeyRelease: {
			// X autorepeat emits release/press pairs; do not turn them into release edges.
			if (event.type == KeyRelease && XPending(display)) {
				XEvent next;
				XPeekEvent(display, &next);
				if (next.type == KeyPress && next.xkey.keycode == event.xkey.keycode &&
				    next.xkey.time == event.xkey.time)
					break;
			}
			int key = ice_key(XLookupKeysym(&event.xkey, 0));
			if (key >= 0)
				input->set_down(key, event.type == KeyPress);
			break;
		}
		case ButtonPress:
		case ButtonRelease: {
			unsigned button = event.xbutton.button;
			if (button == 4 || button == 5) {
				if (event.type == ButtonPress)
					input->add_scroll(button == 4 ? 1 : -1);
			} else
				input->set_down(button == 1   ? ice::input::mouse_l
				                : button == 3 ? ice::input::mouse_r
				                              : ice::input::mouse_m,
				                event.type == ButtonPress);
			if (!captured)
				input->update_mouse_position(event.xbutton.x, event.xbutton.y, height);
			break;
		}
		case MotionNotify:
			if (captured == window) {
				int dx = event.xmotion.x - width / 2, dy = event.xmotion.y - height / 2;
				if (dx || dy) {
					input->add_mouse_delta(dx, dy, height);
					XWarpPointer(display, None, window->window_, 0, 0, 0, 0, width / 2, height / 2);
					XFlush(display);
				}
			} else
				input->update_mouse_position(event.xmotion.x, event.xmotion.y, height);
			break;
		default:
			break;
		}
	}
}

} // namespace ice::platform::x11
