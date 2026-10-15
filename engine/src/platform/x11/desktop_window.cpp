#include "../desktop_window_internal.h"
#include "../text_input_internal.h"
#include "ice/core/utf8.h"
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <algorithm>
#include <array>
#include <clocale>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <map>

namespace ice::platform::detail {
namespace {
class x11_window;
struct connection {
	Display                       *display      = nullptr;
	Atom                           close        = 0;
	Atom                           protocols    = 0;
	XIM                            input_method = nullptr;
	std::map<Window, x11_window *> windows;
	~connection() {
		if (input_method)
			XCloseIM(input_method);
		if (display)
			XCloseDisplay(display);
	}
};
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
		cancel_text_input();
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
			cancel_text_input();
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
	void event(const XEvent &event) {
		if (event.type == ClientMessage && event.xclient.format == 32 &&
		    event.xclient.message_type == connection_->protocols &&
		    Atom(event.xclient.data.l[0]) == connection_->close)
			request_close();
		if (event.type == ConfigureNotify)
			platform_events::post(platform_event_type::window_resized, id());
		if (event.type == FocusIn) {
			focused_ = true;
			text_input_changed();
			platform_events::post(platform_event_type::focus_gained, id());
		}
		if (event.type == FocusOut) {
			focused_ = false;
			cancel_native_text_input();
			if (input_context_)
				XUnsetICFocus(input_context_);
			platform_events::post(platform_event_type::focus_lost, id());
		}
		if (event.type == KeyPress)
			key_press(event.xkey);
		if (event.type == DestroyNotify) {
			connection_->windows.erase(native_);
			native_ = 0;
			request_close();
		}
	}
	void prepare_key_event(const XEvent &event) {
		if (event.type == KeyPress) {
			modifiers_ = modifiers(event.xkey.state);
			repeated_  = event.xkey.keycode < pressed_.size() && pressed_[event.xkey.keycode];
			if (event.xkey.keycode < pressed_.size())
				pressed_[event.xkey.keycode] = true;
		}
		if (event.type == KeyRelease && event.xkey.keycode < pressed_.size()) {
			bool repeated = false;
			if (XPending(connection_->display)) {
				XEvent next{};
				XPeekEvent(connection_->display, &next);
				repeated = next.type == KeyPress && next.xkey.keycode == event.xkey.keycode &&
				           next.xkey.time == event.xkey.time;
			}
			if (!repeated)
				pressed_[event.xkey.keycode] = false;
		}
	}
	void flush_input_cancel() {
		if (cancel_pending_) {
			cancel_pending_ = false;
			cancel_text_input();
		}
	}

private:
	void cancel_native_text_input() override {
		cancelling_ = true;
		preedit_.clear();
		composing_ = false;
		pressed_.fill(false);
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
			                 KeyReleaseMask | filter);
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
	bool                        candidate_warning_ = false;
	key_modifiers               modifiers_         = 0;
	std::string                 preedit_;
	std::array<bool, 256>       pressed_{};
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
	                 KeyReleaseMask);
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
