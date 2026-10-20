#include "../desktop_window_internal.h"
#include "../physical_key_internal.h"
#include "../text_input_internal.h"
#include "ice/core/utf8.h"
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <clocale>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <map>
#include <poll.h>

namespace ice::platform::detail {
namespace {
class x11_window;
struct connection {
	Display                       *display   = nullptr;
	Atom                           close     = 0;
	Atom                           protocols = 0;
	Atom                           clipboard = 0, targets = 0, utf8_string = 0, incremental = 0;
	XIM                            input_method = nullptr;
	std::map<Window, x11_window *> windows;
	std::array<physical_key, 256>  keys{};
	~connection() {
		if (input_method)
			XCloseIM(input_method);
		if (display)
			XCloseDisplay(display);
	}
};
// X11 requestors can disappear between a SelectionRequest and our reply. Trap
// errors only around clipboard protocol requests, on the documented UI thread;
// do not make a stale peer window fatal or change the embedding app's handler.
class clipboard_errors {
public:
	explicit clipboard_errors(Display *display) : display_(display) {
		XSync(display_, False);
		first_    = NextRequest(display_);
		previous_ = XSetErrorHandler(handle);
		active_   = this;
	}
	~clipboard_errors() {
		finish();
		active_ = nullptr;
		XSetErrorHandler(previous_);
	}
	bool finish() {
		XSync(display_, False);
		return !failed_;
	}

private:
	static int handle(Display *display, XErrorEvent *event) {
		if (active_ && display == active_->display_ && event->serial >= active_->first_) {
			active_->failed_ = true;
			return 0;
		}
		return active_ && active_->previous_ ? active_->previous_(display, event) : 0;
	}
	Display                        *display_;
	unsigned long                   first_    = 0;
	bool                            failed_   = false;
	XErrorHandler                   previous_ = nullptr;
	static inline clipboard_errors *active_   = nullptr;
};
struct clipboard_requestor {
	Display *display = nullptr;
	Window   window  = 0;
	~clipboard_requestor() {
		if (display) {
			if (window) {
				// The selection owner knows this requestor ID and may already have
				// destroyed it. Teardown must contain that BadWindow as well.
				clipboard_errors errors(display);
				XDestroyWindow(display, window);
				errors.finish();
			}
			XCloseDisplay(display);
		}
	}
};
struct clipboard_property {
	unsigned char *bytes = nullptr;
	~clipboard_property() {
		if (bytes)
			XFree(bytes);
	}
};
void load_physical_keys(connection &owner) {
	owner.keys.fill(physical_key::unknown);
	if (auto *keyboard = XkbGetKeyboard(owner.display, XkbAllComponentsMask, XkbUseCoreKbd)) {
		if (keyboard->names && keyboard->names->keys)
			for (unsigned code = keyboard->min_key_code;
			     code <= keyboard->max_key_code && code < owner.keys.size(); ++code)
				owner.keys[code] =
				    x11_physical_key(std::string_view(keyboard->names->keys[code].name, 4));
		XkbFreeKeyboard(keyboard, XkbAllComponentsMask, True);
	}
}
std::weak_ptr<connection> current;
key_modifiers             modifiers(unsigned state) {
    key_modifiers result = 0;
    if (state & ShiftMask)
        result |= modifier_flag(key_modifier::shift);
    if (state & ControlMask)
        result |= modifier_flag(key_modifier::control);
    if (state & Mod1Mask)
        result |= modifier_flag(key_modifier::alt);
    if (state & Mod4Mask)
        result |= modifier_flag(key_modifier::super);
    if (state & LockMask)
        result |= modifier_flag(key_modifier::caps_lock);
    return result;
}
std::size_t byte_offset(const std::string &text, std::size_t characters) {
	std::size_t offset = 0;
	while (offset < text.size() && characters) {
		++offset;
		while (offset < text.size() && (static_cast<unsigned char>(text[offset]) & 0xC0) == 0x80)
			++offset;
		--characters;
	}
	return offset;
}
edit_command command_for(KeySym symbol, key_modifiers flags) {
	const bool control =
	    has_modifier(flags, key_modifier::control) && !has_modifier(flags, key_modifier::alt);
	if (control)
		switch (symbol) {
		case XK_a:
		case XK_A:
			return edit_command::select_all;
		case XK_c:
		case XK_C:
			return edit_command::copy;
		case XK_x:
		case XK_X:
			return edit_command::cut;
		case XK_v:
		case XK_V:
			return edit_command::paste;
		case XK_u:
		case XK_U:
			return edit_command::delete_to_line_start;
		case XK_k:
		case XK_K:
			return edit_command::delete_to_line_end;
		case XK_w:
		case XK_W:
			return edit_command::delete_word_backward;
		case XK_p:
		case XK_P:
			return edit_command::move_up;
		case XK_n:
		case XK_N:
			return edit_command::move_down;
		case XK_d:
		case XK_D:
			return edit_command::delete_forward;
		}
	switch (symbol) {
	case XK_Return:
	case XK_KP_Enter:
		return edit_command::accept_line;
	case XK_Escape:
		return edit_command::cancel;
	case XK_Tab:
	case XK_ISO_Left_Tab:
		return edit_command::tab;
	case XK_BackSpace:
		return control ? edit_command::delete_word_backward : edit_command::backspace;
	case XK_Delete:
	case XK_KP_Delete:
		return control ? edit_command::delete_word_forward : edit_command::delete_forward;
	case XK_Left:
	case XK_KP_Left:
		return control ? edit_command::word_left : edit_command::move_left;
	case XK_Right:
	case XK_KP_Right:
		return control ? edit_command::word_right : edit_command::move_right;
	case XK_Up:
	case XK_KP_Up:
		return edit_command::move_up;
	case XK_Down:
	case XK_KP_Down:
		return edit_command::move_down;
	case XK_Home:
	case XK_KP_Home:
		return edit_command::line_start;
	case XK_End:
	case XK_KP_End:
		return edit_command::line_end;
	case XK_Prior:
	case XK_KP_Prior:
		return edit_command::page_up;
	case XK_Next:
	case XK_KP_Next:
		return edit_command::page_down;
	default:
		return edit_command::none;
	}
}
class x11_window final : public desktop_window {
public:
	x11_window(std::shared_ptr<connection> connection, Window window)
	    : connection_(std::move(connection)), native_(window) {
		connection_->windows.emplace(native_, this);
		create_input_context();
	}
	~x11_window() override {
		destroying_ = true;
		cancel_input();
		if (input_context_)
			XDestroyIC(input_context_);
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
		else {
			cancel_input();
			XUnmapWindow(connection_->display, native_);
		}
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
		event.type                 = ClientMessage;
		event.xclient.window       = native_;
		event.xclient.message_type = XInternAtom(connection_->display, "_NET_WM_STATE", False);
		event.xclient.format       = 32;
		event.xclient.data.l[0]    = enabled ? 1 : 0;
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
	relative_pointer_kind relative_pointer_capability() const override {
		return relative_pointer_kind::warp;
	}
	void event(const XEvent &event) {
		if (event.type == SelectionRequest) {
			serve_clipboard(event.xselectionrequest);
			return;
		}
		if (event.type == SelectionClear &&
		    event.xselectionclear.selection == connection_->clipboard) {
			if (XGetSelectionOwner(connection_->display, connection_->clipboard) != native_)
				clipboard_.clear();
			return;
		}
		if (event.type == ClientMessage && event.xclient.format == 32 &&
		    event.xclient.message_type == connection_->protocols &&
		    Atom(event.xclient.data.l[0]) == connection_->close)
			request_close();
		if (event.type == ConfigureNotify) {
			// A pending warp belongs to the previous client geometry. Release capture
			// instead of interpreting that synthetic position as camera movement.
			if (relative_)
				cancel_input();
			platform_events::post(platform_event_type::window_resized, id());
		}
		if (event.type == FocusIn) {
			focused_ = true;
			text_input_changed();
			platform_events::post(platform_event_type::focus_gained, id());
		}
		if (event.type == FocusOut) {
			focused_ = false;
			cancel_input();
			if (input_context_)
				XUnsetICFocus(input_context_);
			platform_events::post(platform_event_type::focus_lost, id());
		}
		if (event.type == KeyPress)
			key_press(event.xkey);
		if (event.type == UnmapNotify) {
			focused_ = false;
			cancel_input();
			pointer_valid_ = false;
		}
		if (event.type == EnterNotify || event.type == LeaveNotify)
			pointer_valid_ = false;
		if ((event.type == ButtonPress || event.type == ButtonRelease) && focused_) {
			unsigned       button = event.xbutton.button;
			platform_event input{platform_event_type::pointer_button, id(), 0};
			input.x         = event.xbutton.x;
			input.y         = event.xbutton.y;
			input.modifiers = modifiers(event.xbutton.state);
			input.pressed   = event.type == ButtonPress;
			if (button >= 4 && button <= 7) {
				if (!input.pressed)
					return;
				input.type = platform_event_type::scroll;
				if (button <= 5)
					input.scroll_y = button == 4 ? 1 : -1;
				else
					input.scroll_x = button == 6 ? -1 : 1;
			} else {
				input.button = button == 1   ? mouse_button::primary
				               : button == 2 ? mouse_button::middle
				               : button == 3 ? mouse_button::secondary
				               : button == 8 ? mouse_button::back
				               : button == 9 ? mouse_button::forward
				                             : mouse_button::none;
				if (input.button == mouse_button::none)
					return;
			}
			if (!platform_events::post(std::move(input)))
				cancel_input();
		}
		if (event.type == MotionNotify && focused_)
			pointer_motion(event.xmotion);
		if (event.type == DestroyNotify) {
			connection_->windows.erase(native_);
			native_ = 0;
			request_close();
		}
	}
	void prepare_key_event(const XEvent &event) {
		if (!focused_)
			return;
		if (event.type == KeyPress) {
			modifiers_ = modifiers(event.xkey.state);
			repeated_  = event.xkey.keycode < pressed_.size() && pressed_[event.xkey.keycode];
			if (event.xkey.keycode < pressed_.size())
				pressed_[event.xkey.keycode] = true;
			post_physical(event.xkey, true, repeated_);
		}
		if (event.type == KeyRelease && event.xkey.keycode < pressed_.size()) {
			bool repeated = false;
			if (XPending(connection_->display)) {
				XEvent next{};
				XPeekEvent(connection_->display, &next);
				repeated = next.type == KeyPress && next.xkey.keycode == event.xkey.keycode &&
				           next.xkey.time == event.xkey.time;
			}
			if (!repeated) {
				pressed_[event.xkey.keycode] = false;
				post_physical(event.xkey, false, false);
			}
		}
	}
	void flush_input_cancel() {
		if (cancel_pending_) {
			cancel_pending_ = false;
			cancel_text_input();
		}
	}

private:
	ice::expected<std::string, window_diagnostic>
	read_native_clipboard_text(std::size_t maximum_bytes) override {
		const auto owner = XGetSelectionOwner(connection_->display, connection_->clipboard);
		if (owner == None)
			return std::string{};
		if (const auto local = connection_->windows.find(owner);
		    local != connection_->windows.end()) {
			if (local->second->clipboard_.size() > maximum_bytes)
				return ice::unexpected(
				    window_diagnostic{"X11 clipboard text exceeds the byte limit"});
			return local->second->clipboard_;
		}
		// A private connection avoids consuming/reentering the main keyboard, IME,
		// lifecycle and window queues while an external owner answers the request.
		clipboard_requestor request;
		request.display = XOpenDisplay(DisplayString(connection_->display));
		if (!request.display)
			return ice::unexpected(window_diagnostic{"X11 clipboard connection failed"});
		clipboard_errors errors(request.display);
		request.window = XCreateSimpleWindow(request.display, DefaultRootWindow(request.display), 0,
		                                     0, 1, 1, 0, 0, 0);
		const auto property = XInternAtom(request.display, "_ICE_CLIPBOARD_TEXT", False);
		if (!request.window || !errors.finish())
			return ice::unexpected(window_diagnostic{"X11 clipboard request window failed"});
		XConvertSelection(request.display, connection_->clipboard, connection_->utf8_string,
		                  property, request.window, CurrentTime);
		XFlush(request.display);
		const auto deadline  = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
		unsigned   processed = 0;
		while (std::chrono::steady_clock::now() < deadline) {
			while (XPending(request.display)) {
				if (std::chrono::steady_clock::now() >= deadline)
					break;
				if (++processed > 256)
					return ice::unexpected(window_diagnostic{"X11 clipboard event limit exceeded"});
				XEvent event{};
				XNextEvent(request.display, &event);
				if (event.type != SelectionNotify || event.xselection.requestor != request.window ||
				    event.xselection.selection != connection_->clipboard ||
				    event.xselection.target != connection_->utf8_string)
					continue;
				if (event.xselection.property == None)
					return std::string{};
				if (event.xselection.property != property)
					return ice::unexpected(
					    window_diagnostic{"X11 clipboard returned an unexpected property"});
				Atom               type   = None;
				int                format = 0;
				unsigned long      items = 0, remaining = 0;
				clipboard_property data;
				// XGetWindowProperty measures its length in four-byte units. Read at
				// most one unit beyond the caller limit and reject all unread tails.
				const auto status = XGetWindowProperty(
				    request.display, request.window, property, 0, long(maximum_bytes / 4 + 1), True,
				    connection_->utf8_string, &type, &format, &items, &remaining, &data.bytes);
				if (status != Success || !errors.finish())
					return ice::unexpected(window_diagnostic{"X11 clipboard property read failed"});
				if (type == connection_->incremental)
					return ice::unexpected(
					    window_diagnostic{"X11 incremental clipboard transfers are unsupported"});
				if (type != connection_->utf8_string || format != 8 || remaining ||
				    items > maximum_bytes)
					return ice::unexpected(window_diagnostic{
					    "X11 clipboard text has an unsupported format or exceeds the byte limit"});
				return items ? std::string(reinterpret_cast<const char *>(data.bytes), items)
				             : std::string{};
			}
			const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
			                           deadline - std::chrono::steady_clock::now())
			                           .count();
			if (remaining <= 0)
				break;
			pollfd    ready{ConnectionNumber(request.display), POLLIN, 0};
			const int result = ::poll(&ready, 1, int(remaining));
			if ((result < 0 && errno != EINTR) || (ready.revents & (POLLERR | POLLHUP | POLLNVAL)))
				return ice::unexpected(
				    window_diagnostic{"X11 clipboard connection was interrupted"});
		}
		return ice::unexpected(
		    window_diagnostic{"X11 clipboard owner did not respond within 500 ms"});
	}
	ice::expected<void, window_diagnostic>
	write_native_clipboard_text(std::string_view text) override {
		// This profile serves direct properties only. Do not accept a write that
		// cannot fit the server's request budget and later fail to deliver it.
		const auto extended = XExtendedMaxRequestSize(connection_->display);
		const auto units    = extended ? extended : XMaxRequestSize(connection_->display);
		if (units <= 64 || text.size() > std::size_t(units - 64) * 4)
			return ice::unexpected(
			    window_diagnostic{"X11 clipboard text exceeds the direct-transfer server limit"});
		std::string      prepared(text);
		clipboard_errors errors(connection_->display);
		XSetSelectionOwner(connection_->display, connection_->clipboard, native_, CurrentTime);
		if (XGetSelectionOwner(connection_->display, connection_->clipboard) != native_ ||
		    !errors.finish())
			return ice::unexpected(window_diagnostic{"X11 clipboard ownership failed"});
		clipboard_.swap(prepared);
		return {};
	}
	void serve_clipboard(const XSelectionRequestEvent &request) {
		XEvent reply{};
		reply.xselection.type      = SelectionNotify;
		reply.xselection.display   = request.display;
		reply.xselection.requestor = request.requestor;
		reply.xselection.selection = request.selection;
		reply.xselection.target    = request.target;
		reply.xselection.time      = request.time;
		reply.xselection.property  = None;
		clipboard_errors errors(connection_->display);
		const auto       property = request.property == None ? request.target : request.property;
		if (request.owner == native_ && request.selection == connection_->clipboard &&
		    XGetSelectionOwner(connection_->display, connection_->clipboard) == native_) {
			if (request.target == connection_->targets) {
				const std::array<Atom, 2> targets{connection_->targets, connection_->utf8_string};
				XChangeProperty(
				    connection_->display, request.requestor, property, XA_ATOM, 32, PropModeReplace,
				    reinterpret_cast<const unsigned char *>(targets.data()), int(targets.size()));
				reply.xselection.property = property;
			} else if (request.target == connection_->utf8_string) {
				XChangeProperty(connection_->display, request.requestor, property,
				                connection_->utf8_string, 8, PropModeReplace,
				                reinterpret_cast<const unsigned char *>(clipboard_.data()),
				                int(clipboard_.size()));
				reply.xselection.property = property;
			}
		}
		XSendEvent(connection_->display, request.requestor, False, 0, &reply);
		errors.finish();
	}
	void cancel_native_physical_input() override {
		pressed_.fill(false);
		pointer_valid_ = false;
	}
	void post_physical(const XKeyEvent &native, bool pressed, bool repeated) {
		if (native.keycode >= connection_->keys.size())
			return;
		auto key = connection_->keys[native.keycode];
		if (key == physical_key::unknown)
			return;
		platform_event input{platform_event_type::physical_key, id(), 0};
		input.key       = key;
		input.pressed   = pressed;
		input.repeat    = repeated;
		input.modifiers = modifiers(native.state);
		if (!platform_events::post(std::move(input)))
			cancel_input();
	}
	void pointer_motion(const XMotionEvent &native) {
		platform_event input{platform_event_type::pointer_move, id(), 0};
		input.modifiers = modifiers(native.state);
		if (relative_) {
			if (warp_pending_ && native.x == warp_x_ && native.y == warp_y_) {
				warp_pending_ = false;
				last_x_       = warp_x_;
				last_y_       = warp_y_;
				return;
			}
			input.x       = anchor_x_;
			input.y       = anchor_y_;
			input.delta_x = native.x - last_x_;
			input.delta_y = native.y - last_y_;
			last_x_       = native.x;
			last_y_       = native.y;
			if (!warp_pending_)
				warp_pointer();
		} else {
			input.x = native.x;
			input.y = native.y;
			if (pointer_valid_) {
				input.delta_x = native.x - last_x_;
				input.delta_y = native.y - last_y_;
			}
			last_x_        = native.x;
			last_y_        = native.y;
			pointer_valid_ = true;
		}
		if (!platform_events::post(std::move(input)))
			cancel_input();
	}
	void warp_pointer() {
		warp_pending_ = true;
		XWarpPointer(connection_->display, None, native_, 0, 0, 0, 0, warp_x_, warp_y_);
		XFlush(connection_->display);
	}
	ice::expected<void, window_diagnostic>
	change_pointer_mode(ice::platform::pointer_mode mode) override {
		if (mode == ice::platform::pointer_mode::normal) {
			if (relative_) {
				relative_      = false;
				warp_pending_  = false;
				pointer_valid_ = false;
				XUngrabPointer(connection_->display, CurrentTime);
				if (hidden_cursor_) {
					XFreeCursor(connection_->display, hidden_cursor_);
					hidden_cursor_ = 0;
				}
				XFlush(connection_->display);
			}
			return {};
		}
		XWindowAttributes attributes{};
		if (!native_ || !focused_ ||
		    !XGetWindowAttributes(connection_->display, native_, &attributes) ||
		    attributes.map_state != IsViewable)
			return ice::unexpected(window_diagnostic{
			    "Relative pointer capture requires the focused visible X11 window"});
		const char empty  = 0;
		auto       bitmap = XCreateBitmapFromData(connection_->display, native_, &empty, 1, 1);
		if (!bitmap)
			return ice::unexpected(window_diagnostic{"X11 could not create the capture cursor"});
		XColor black{};
		hidden_cursor_ =
		    XCreatePixmapCursor(connection_->display, bitmap, bitmap, &black, &black, 0, 0);
		XFreePixmap(connection_->display, bitmap);
		if (!hidden_cursor_ ||
		    XGrabPointer(connection_->display, native_, False,
		                 ButtonPressMask | ButtonReleaseMask | PointerMotionMask, GrabModeAsync,
		                 GrabModeAsync, native_, hidden_cursor_, CurrentTime) != GrabSuccess) {
			if (hidden_cursor_)
				XFreeCursor(connection_->display, hidden_cursor_);
			hidden_cursor_ = 0;
			return ice::unexpected(window_diagnostic{"X11 warp-relative pointer grab failed"});
		}
		Window   root = 0, child = 0;
		int      root_x = 0, root_y = 0, local_x = 0, local_y = 0;
		unsigned mask = 0;
		XQueryPointer(connection_->display, native_, &root, &child, &root_x, &root_y, &local_x,
		              &local_y, &mask);
		anchor_x_ = local_x;
		anchor_y_ = local_y;
		warp_x_   = attributes.width / 2;
		warp_y_   = attributes.height / 2;
		last_x_   = warp_x_;
		last_y_   = warp_y_;
		XEvent stale{};
		while (XCheckTypedWindowEvent(connection_->display, native_, MotionNotify, &stale)) {
		}
		relative_ = true;
		warp_pointer();
		return {};
	}
	void cancel_native_text_input() override {
		cancelling_ = true;
		preedit_.clear();
		composing_ = false;
		if (input_context_) {
			char *discarded = Xutf8ResetIC(input_context_);
			if (discarded)
				XFree(discarded);
		}
		cancelling_ = false;
	}
	void text_input_changed() override {
		if (!input_context_)
			return;
		if (focused_ && text_input_enabled())
			XSetICFocus(input_context_);
		else
			XUnsetICFocus(input_context_);
		const auto    rect = text_input_area();
		XPoint        spot{short(std::clamp(rect.x, -32768.f, 32767.f)),
                    short(std::clamp(rect.y + rect.height, -32768.f, 32767.f))};
		XVaNestedList attributes = XVaCreateNestedList(0, XNSpotLocation, &spot, nullptr);
		if (attributes) {
			const bool unsupported =
			    XSetICValues(input_context_, XNPreeditAttributes, attributes, nullptr) != nullptr;
			XFree(attributes);
			if (unsupported && !candidate_warning_ && text_input_enabled()) {
				candidate_warning_ = true;
				std::fprintf(
				    stderr,
				    "ICE X11: input method does not accept application candidate placement\n");
			}
		}
	}
	bool post(platform_event event) {
		if (destroying_ || cancelling_ || !focused_ || !text_input_enabled())
			return false;
		event.modifiers = modifiers_;
		event.repeat    = repeated_;
		if (!platform_events::post(std::move(event))) {
			cancel_pending_ = true;
			return false;
		}
		return true;
	}
	void publish_preedit(int cursor) {
		platform_event event{platform_event_type::text_composition, id(), 0};
		event.text            = preedit_;
		event.selection_start = byte_offset(preedit_, std::size_t(std::max(cursor, 0)));
		post(std::move(event));
	}
	static Bool preedit_start([[maybe_unused]] XIC input_context, XPointer client,
	                          [[maybe_unused]] XPointer call_data) {
		auto &owner      = *reinterpret_cast<x11_window *>(client);
		owner.composing_ = true;
		return 4096;
	}
	static void preedit_done([[maybe_unused]] XIM input_method, XPointer client,
	                         [[maybe_unused]] XPointer call_data) {
		auto &owner      = *reinterpret_cast<x11_window *>(client);
		owner.composing_ = false;
		owner.preedit_.clear();
		owner.publish_preedit(0);
	}
	static void preedit_draw([[maybe_unused]] XIM input_method, XPointer client, XPointer data) {
		auto &owner = *reinterpret_cast<x11_window *>(client);
		if (owner.cancelling_ || owner.destroying_)
			return;
		const auto &change = *reinterpret_cast<XIMPreeditDrawCallbackStruct *>(data);
		if (change.chg_first < 0 || change.chg_length < 0) {
			owner.cancel_pending_ = true;
			return;
		}
		std::string replacement;
		if (change.text) {
			const auto &text = *change.text;
			if (!text.string.multi_byte) {
				owner.publish_preedit(change.caret);
				return;
			}
			if (text.encoding_is_wchar) {
				if (text.length > 4096 || (!text.string.wide_char && text.length)) {
					owner.cancel_pending_ = true;
					return;
				}
				for (unsigned index = 0; index < text.length; ++index)
					if (!append_utf8(replacement, std::uint32_t(text.string.wide_char[index]))) {
						owner.cancel_pending_ = true;
						return;
					}
			} else if (text.string.multi_byte) {
				if (!locale_characters(text.string.multi_byte, text.length, replacement)) {
					owner.cancel_pending_ = true;
					return;
				}
			}
		}
		const auto first = byte_offset(owner.preedit_, std::size_t(change.chg_first));
		const auto end   = byte_offset(owner.preedit_, std::size_t(change.chg_first) +
		                                                   std::size_t(change.chg_length));
		if (owner.preedit_.size() - (end - first) + replacement.size() > 4096) {
			owner.cancel_pending_ = true;
			return;
		}
		owner.preedit_.replace(first, end - first, replacement);
		owner.publish_preedit(change.caret);
	}
	static void preedit_caret([[maybe_unused]] XIM input_method, XPointer client, XPointer data) {
		auto &owner = *reinterpret_cast<x11_window *>(client);
		auto &caret = *reinterpret_cast<XIMPreeditCaretCallbackStruct *>(data);
		// The preedit renderer exposes the IM's absolute caret; relative movement is
		// left to the IM so terminal history never consumes composition arrows.
		if (caret.direction == XIMAbsolutePosition)
			owner.publish_preedit(caret.position);
	}
	static void context_destroyed([[maybe_unused]] XIM input_method, XPointer client,
	                              [[maybe_unused]] XPointer call_data) {
		auto &owner          = *reinterpret_cast<x11_window *>(client);
		owner.input_context_ = nullptr;
		owner.preedit_.clear();
		owner.composing_ = false;
		if (!owner.destroying_)
			platform_events::post(platform_event_type::input_cancelled, owner.id());
	}
	void create_input_context() {
		if (!connection_->input_method)
			return;
		XIMStyles *styles = nullptr;
		if (XGetIMValues(connection_->input_method, XNQueryInputStyle, &styles, nullptr) || !styles)
			return;
		const auto supports = [&](XIMStyle style) {
			return std::find(styles->supported_styles,
			                 styles->supported_styles + styles->count_styles,
			                 style) != styles->supported_styles + styles->count_styles;
		};
		const XIMStyle callbacks = XIMPreeditCallbacks | XIMStatusNothing;
		XICCallback    start{reinterpret_cast<XPointer>(this), preedit_start};
		XIMCallback    done{reinterpret_cast<XPointer>(this), preedit_done};
		XIMCallback    draw{reinterpret_cast<XPointer>(this), preedit_draw};
		XIMCallback    caret{reinterpret_cast<XPointer>(this), preedit_caret};
		if (supports(callbacks)) {
			XVaNestedList attributes = XVaCreateNestedList(
			    0, XNPreeditStartCallback, &start, XNPreeditDoneCallback, &done,
			    XNPreeditDrawCallback, &draw, XNPreeditCaretCallback, &caret, nullptr);
			input_context_ = XCreateIC(connection_->input_method, XNInputStyle, callbacks,
			                           XNClientWindow, native_, XNFocusWindow, native_,
			                           XNPreeditAttributes, attributes, nullptr);
			if (attributes)
				XFree(attributes);
		}
		if (!input_context_ && supports(XIMPreeditNothing | XIMStatusNothing)) {
			input_context_ = XCreateIC(connection_->input_method, XNInputStyle,
			                           XIMPreeditNothing | XIMStatusNothing, XNClientWindow,
			                           native_, XNFocusWindow, native_, nullptr);
			if (input_context_)
				std::fprintf(stderr, "ICE X11: input method owns preedit UI; application "
				                     "preedit/candidate placement unavailable\n");
		}
		XFree(styles);
		if (input_context_) {
			XIMCallback destroyed{reinterpret_cast<XPointer>(this), context_destroyed};
			XSetICValues(input_context_, XNDestroyCallback, &destroyed, nullptr);
			long filter = 0;
			XGetICValues(input_context_, XNFilterEvents, &filter, nullptr);
			XSelectInput(connection_->display, native_,
			             StructureNotifyMask | FocusChangeMask | ExposureMask | KeyPressMask |
			                 KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
			                 PointerMotionMask | EnterWindowMask | LeaveWindowMask | filter);
		} else
			std::fprintf(stderr,
			             "ICE X11: no input context; native ASCII lookup fallback active\n");
	}
	void key_press(const XKeyEvent &source) {
		platform_event activity{platform_event_type::input_activity, id(), 0};
		activity.modifiers = modifiers_;
		activity.repeat    = repeated_;
		if (!platform_events::post(std::move(activity))) {
			cancel_text_input();
			return;
		}
		if (!text_input_enabled() || !focused_)
			return;
		XKeyEvent              key = source;
		std::array<char, 4096> bytes{};
		KeySym                 symbol = NoSymbol;
		Status                 status = XLookupNone;
		int                    count  = 0;
		if (input_context_) {
			count = Xutf8LookupString(input_context_, &key, bytes.data(), int(bytes.size()),
			                          &symbol, &status);
			if (status == XBufferOverflow || cancel_pending_) {
				cancel_pending_ = false;
				cancel_text_input();
				return;
			}
		} else {
			count  = XLookupString(&key, bytes.data(), int(bytes.size()), &symbol, nullptr);
			status = count ? XLookupBoth : XLookupKeySym;
			// Without XIM, only native ASCII bytes are guaranteed; never reinterpret
			// locale bytes as UTF-8 or synthesize printable text from physical key codes.
			for (int index = 0; index < count; ++index)
				if (static_cast<unsigned char>(bytes[std::size_t(index)]) >= 128)
					count = 0;
		}
		if ((status == XLookupChars || status == XLookupBoth) && count > 0 &&
		    count <= int(bytes.size()) && static_cast<unsigned char>(bytes[0]) >= 32 &&
		    bytes[0] != 127) {
			platform_event event{platform_event_type::text_committed, id(), 0};
			event.text.assign(bytes.data(), std::size_t(count));
			post(std::move(event));
			return;
		}
		if (status == XLookupKeySym || status == XLookupBoth) {
			const auto command = command_for(symbol, modifiers_);
			if (!composing_ && command != edit_command::none) {
				platform_event event{platform_event_type::edit_command, id(), 0};
				event.command = command;
				post(std::move(event));
				return;
			}
		}
	}
	XIC  input_context_ = nullptr;
	bool focused_ = false, composing_ = false, cancelling_ = false, cancel_pending_ = false,
	     destroying_ = false, repeated_ = false;
	bool                  candidate_warning_ = false;
	key_modifiers         modifiers_         = 0;
	std::string           preedit_;
	std::string           clipboard_;
	std::array<bool, 256> pressed_{};
	bool                  relative_ = false, pointer_valid_ = false, warp_pending_ = false;
	int    warp_x_ = 0, warp_y_ = 0, last_x_ = 0, last_y_ = 0, anchor_x_ = 0, anchor_y_ = 0;
	Cursor hidden_cursor_ = 0;
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
		shared->close       = XInternAtom(shared->display, "WM_DELETE_WINDOW", False);
		shared->protocols   = XInternAtom(shared->display, "WM_PROTOCOLS", False);
		shared->clipboard   = XInternAtom(shared->display, "CLIPBOARD", False);
		shared->targets     = XInternAtom(shared->display, "TARGETS", False);
		shared->utf8_string = XInternAtom(shared->display, "UTF8_STRING", False);
		shared->incremental = XInternAtom(shared->display, "INCR", False);
		// Only character classification/conversion follows the user's locale. Numeric
		// parsing/formatting retains its existing locale. XIM binds this CTYPE locale.
		const char *locale = std::setlocale(LC_CTYPE, nullptr);
		if (locale && (std::strcmp(locale, "C") == 0 || std::strcmp(locale, "POSIX") == 0))
			std::setlocale(LC_CTYPE, "");
		XSetLocaleModifiers("");
		shared->input_method = XOpenIM(shared->display, nullptr, nullptr, nullptr);
		if (shared->input_method) {
			XIMCallback destroyed{
			    reinterpret_cast<XPointer>(shared.get()), [](XIM, XPointer client, XPointer) {
				    reinterpret_cast<connection *>(client)->input_method = nullptr;
			    }};
			XSetIMValues(shared->input_method, XNDestroyCallback, &destroyed, nullptr);
		}
		if (!shared->input_method)
			std::fprintf(stderr, "ICE X11: XIM unavailable; native ASCII lookup fallback active\n");
		load_physical_keys(*shared);
		current = shared;
	}
	Window native =
	    XCreateSimpleWindow(shared->display, DefaultRootWindow(shared->display), 0, 0,
	                        unsigned(options.width), unsigned(options.height), 0, 0,
	                        BlackPixel(shared->display, DefaultScreen(shared->display)));
	if (!native)
		return ice::unexpected(window_diagnostic{"X11 window creation failed"});
	XSelectInput(shared->display, native,
	             StructureNotifyMask | FocusChangeMask | ExposureMask | KeyPressMask |
	                 KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
	                 EnterWindowMask | LeaveWindowMask);
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
		if (event.type == MappingNotify) {
			XRefreshKeyboardMapping(&event.xmapping);
			load_physical_keys(*shared);
		}
		auto owner = shared->windows.find(event.xany.window);
		if (owner != shared->windows.end())
			owner->second->prepare_key_event(event);
		const bool filtered = XFilterEvent(&event, None);
		if (owner != shared->windows.end() &&
		    (!filtered || (event.type != KeyPress && event.type != KeyRelease)))
			owner->second->event(event);
		for (const auto &[native, window] : shared->windows) {
			(void)native;
			window->flush_input_cancel();
		}
	}
}
} // namespace ice::platform::detail
