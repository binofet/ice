#include "ice/platform/desktop_window.h"
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <algorithm>
#include <cstdio>
#include <string_view>
#include <utility>
#include <vector>

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::fprintf(stderr, "Native X11 input failed at line %d: %s\n", __LINE__,             \
			             #condition);                                                              \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

namespace {
using ice::platform::desktop_window;
using ice::platform::platform_event;
using ice::platform::platform_event_type;
using ice::platform::platform_events;
using ice::platform::window_id;

std::vector<platform_event> collect(Display *display) {
	// A server round trip makes queued requests/events observable without sleeps or a
	// human timeout. Pump twice for native input-method work triggered by the first pass.
	for (int pass = 0; pass < 2; ++pass) {
		XSync(display, False);
		desktop_window::pump_events();
	}
	std::vector<platform_event> result;
	platform_event              event;
	while (platform_events::poll(event))
		result.push_back(std::move(event));
	return result;
}

bool send_code(Display *display, Window window, KeyCode code, int type, unsigned modifiers,
               Time time) {
	if (!code)
		return false;
	XEvent event{};
	event.type             = type;
	event.xkey.display     = display;
	event.xkey.window      = window;
	event.xkey.root        = DefaultRootWindow(display);
	event.xkey.time        = time;
	event.xkey.state       = modifiers;
	event.xkey.keycode     = code;
	event.xkey.same_screen = True;
	event.xkey.x = event.xkey.y = 1;
	return XSendEvent(display, window, False, type == KeyPress ? KeyPressMask : KeyReleaseMask,
	                  &event) != 0;
}
bool send_key(Display *display, Window window, KeySym symbol, int type, unsigned modifiers,
              Time time) {
	return send_code(display, window, XKeysymToKeycode(display, symbol), type, modifiers, time);
}

KeyCode physical_w(Display *display) {
	KeyCode result = 0;
	if (auto *keyboard = XkbGetKeyboard(display, XkbAllComponentsMask, XkbUseCoreKbd)) {
		if (keyboard->names && keyboard->names->keys)
			for (unsigned code = keyboard->min_key_code; code <= keyboard->max_key_code; ++code)
				if (std::string_view(keyboard->names->keys[code].name, 4) == "AD02")
					result = static_cast<KeyCode>(code);
		XkbFreeKeyboard(keyboard, XkbAllComponentsMask, True);
	}
	return result;
}
class remapped_key {
public:
	remapped_key(Display *display, KeyCode code) : display_(display), code_(code) {
		previous_ = XGetKeyboardMapping(display_, code_, 1, &count_);
		if (previous_ && count_ > 0) {
			std::vector<KeySym> symbols(static_cast<std::size_t>(count_), NoSymbol);
			symbols[0] = XK_z;
			if (symbols.size() > 1)
				symbols[1] = XK_Z;
			XChangeKeyboardMapping(display_, code_, count_, symbols.data(), 1);
		}
	}
	~remapped_key() {
		if (previous_) {
			XChangeKeyboardMapping(display_, code_, count_, previous_, 1);
			XFree(previous_);
		}
	}
	bool valid() const { return previous_ && count_ > 0; }

private:
	Display *display_;
	KeyCode  code_;
	int      count_    = 0;
	KeySym  *previous_ = nullptr;
};

bool stroke(Display *display, Window window, KeySym symbol, unsigned modifiers, Time time) {
	return send_key(display, window, symbol, KeyPress, modifiers, time) &&
	       send_key(display, window, symbol, KeyRelease, modifiers, time + 1);
}

std::vector<platform_event> actions(const std::vector<platform_event> &events) {
	std::vector<platform_event> result;
	for (const auto &event : events)
		if (event.type == platform_event_type::text_committed ||
		    event.type == platform_event_type::text_composition ||
		    event.type == platform_event_type::edit_command)
			result.push_back(event);
	return result;
}

bool contains(const std::vector<platform_event> &events, platform_event_type type,
              window_id window) {
	return std::any_of(events.begin(), events.end(), [&](const auto &event) {
		return event.type == type && event.window == window;
	});
}

bool committed(const platform_event &event, window_id window, const char *text) {
	return event.type == platform_event_type::text_committed && event.window == window &&
	       event.text == text;
}
} // namespace

int main() {
	using namespace ice::platform;
	// Run explicitly under an isolated Xvfb server. This test changes real native focus
	// and sends native key events, rather than injecting translated engine events.
	auto first = desktop_window::create({"ICE native input one", 320, 200, true});
	if (!first) {
		std::fprintf(stderr, "%s\n", first.error().message.c_str());
		return 1;
	}
	auto second = desktop_window::create({"ICE native input two", 320, 200, true});
	CHECK(second);
	const auto first_handle  = (*first)->native_handle();
	const auto second_handle = (*second)->native_handle();
	CHECK(first_handle.kind == native_window_kind::x11 &&
	      second_handle.kind == native_window_kind::x11);
	CHECK(first_handle.display && first_handle.display == second_handle.display);
	auto      *display       = static_cast<Display *>(first_handle.display);
	const auto first_native  = static_cast<Window>(first_handle.window);
	const auto second_native = static_cast<Window>(second_handle.window);
	const auto first_id      = (*first)->id();
	const auto second_id     = (*second)->id();
	CHECK(first_native && second_native && first_id != second_id);
	(void)collect(display);
	XWindowAttributes attributes{};
	CHECK(XGetWindowAttributes(display, first_native, &attributes) &&
	      attributes.map_state == IsViewable);
	CHECK(XGetWindowAttributes(display, second_native, &attributes) &&
	      attributes.map_state == IsViewable);
	(*first)->enable_text_input();
	(*second)->enable_text_input();
	CHECK((*first)->set_text_input_rect({12, 20, 1, 18}));
	XSetInputFocus(display, DefaultRootWindow(display), RevertToPointerRoot, CurrentTime);
	(void)collect(display);
	XSetInputFocus(display, first_native, RevertToPointerRoot, CurrentTime);
	auto events = collect(display);
	CHECK(contains(events, platform_event_type::focus_gained, first_id));
	Window focused = None;
	int    revert  = 0;
	XGetInputFocus(display, &focused, &revert);
	CHECK(focused == first_native);
	// This isolated server's physical AD02 position produces z after a temporary
	// layout change. Text follows the layout; physical W must retain its position.
	const auto w = physical_w(display);
	CHECK(w != 0);
	{
		remapped_key remap(display, w);
		CHECK(remap.valid());
		(void)collect(display);
		CHECK(send_code(display, first_native, w, KeyPress, 0, 80));
		CHECK(send_code(display, first_native, w, KeyRelease, 0, 81));
		events    = collect(display);
		auto text = actions(events);
		CHECK(text.size() == 1 && committed(text[0], first_id, "z"));
		bool down = false, up = false;
		for (const auto &event : events)
			if (event.type == platform_event_type::physical_key && event.window == first_id) {
				CHECK(event.key == physical_key::w);
				if (event.pressed)
					down = true;
				else
					up = true;
			}
		CHECK(down && up);
	}
	(void)collect(display);

	CHECK(stroke(display, first_native, XK_a, 0, 100));
	CHECK(stroke(display, first_native, XK_b, ShiftMask, 110));
	auto input = actions(collect(display));
	CHECK(input.size() == 2 && committed(input[0], first_id, "a") &&
	      committed(input[1], first_id, "B"));
	CHECK(!input[0].repeat && !input[1].repeat &&
	      has_modifier(input[1].modifiers, key_modifier::shift));
	CHECK(input[0].sequence < input[1].sequence);

	CHECK(stroke(display, first_native, XK_Return, 0, 120));
	CHECK(stroke(display, first_native, XK_BackSpace, 0, 130));
	CHECK(stroke(display, first_native, XK_Left, ControlMask | ShiftMask, 140));
	CHECK(stroke(display, first_native, XK_c, ControlMask, 150));
	input = actions(collect(display));
	CHECK(input.size() == 4);
	const edit_command commands[] = {edit_command::accept_line, edit_command::backspace,
	                                 edit_command::word_left, edit_command::copy};
	for (std::size_t index = 0; index < input.size(); ++index)
		CHECK(input[index].type == platform_event_type::edit_command &&
		      input[index].window == first_id && input[index].command == commands[index]);
	CHECK(has_modifier(input[2].modifiers, key_modifier::control) &&
	      has_modifier(input[2].modifiers, key_modifier::shift));

	// X11 autorepeat can arrive as release/press with the same key and timestamp.
	// A later ordinary release must clear that repeat state for the next stroke.
	CHECK(send_key(display, first_native, XK_a, KeyPress, 0, 200));
	CHECK(send_key(display, first_native, XK_a, KeyRelease, 0, 201));
	CHECK(send_key(display, first_native, XK_a, KeyPress, 0, 201));
	CHECK(send_key(display, first_native, XK_a, KeyRelease, 0, 202));
	CHECK(stroke(display, first_native, XK_a, 0, 210));
	input = actions(collect(display));
	CHECK(input.size() == 3);
	for (const auto &event : input)
		CHECK(committed(event, first_id, "a"));
	CHECK(!input[0].repeat && input[1].repeat && !input[2].repeat);

	// Leave a key held across a real focus transfer. Losing focus must cancel input
	// and discard repeat state, without committing any pending text to either client.
	CHECK(send_key(display, first_native, XK_a, KeyPress, 0, 300));
	input = actions(collect(display));
	CHECK(input.size() == 1 && committed(input[0], first_id, "a") && !input[0].repeat);
	XSetInputFocus(display, second_native, RevertToPointerRoot, CurrentTime);
	events = collect(display);
	CHECK(contains(events, platform_event_type::focus_lost, first_id));
	CHECK(contains(events, platform_event_type::input_cancelled, first_id));
	CHECK(contains(events, platform_event_type::focus_gained, second_id));
	CHECK(actions(events).empty());
	XSetInputFocus(display, first_native, RevertToPointerRoot, CurrentTime);
	(void)collect(display);
	CHECK(stroke(display, first_native, XK_a, 0, 310));
	input = actions(collect(display));
	CHECK(input.size() == 1 && committed(input[0], first_id, "a") && !input[0].repeat);

	XSetInputFocus(display, second_native, RevertToPointerRoot, CurrentTime);
	(void)collect(display);
	CHECK(stroke(display, first_native, XK_a, 0, 400));
	CHECK(stroke(display, first_native, XK_Return, 0, 410));
	CHECK(stroke(display, second_native, XK_b, 0, 420));
	CHECK(stroke(display, second_native, XK_Return, 0, 430));
	input = actions(collect(display));
	CHECK(input.size() == 2 && committed(input[0], second_id, "b"));
	CHECK(input[1].type == platform_event_type::edit_command && input[1].window == second_id &&
	      input[1].command == edit_command::accept_line);

	(*second)->enable_text_input(false);
	events = collect(display);
	CHECK(contains(events, platform_event_type::input_cancelled, second_id));
	CHECK(stroke(display, second_native, XK_b, 0, 500));
	CHECK(stroke(display, second_native, XK_Return, 0, 510));
	CHECK(actions(collect(display)).empty());
	(*second)->enable_text_input();
	CHECK(stroke(display, second_native, XK_b, 0, 520));
	input = actions(collect(display));
	CHECK(input.size() == 1 && committed(input[0], second_id, "b"));
	XSetInputFocus(display, first_native, RevertToPointerRoot, CurrentTime);
	(void)collect(display);
	CHECK((*first)->relative_pointer_capability() == relative_pointer_kind::warp);
	CHECK(!(*second)->set_pointer_mode(pointer_mode::relative));
	CHECK((*first)->set_pointer_mode(pointer_mode::relative));
	(void)collect(display); // Consume the initial center warp without synthetic motion.
	(*first)->enable_text_input(false);
	events = collect(display);
	CHECK((*first)->pointer_mode() == pointer_mode::relative);
	bool text_only = false;
	for (const auto &event : events)
		if (event.type == platform_event_type::input_cancelled && event.window == first_id)
			text_only = event.cancellation == input_cancellation::text;
	CHECK(text_only);
	CHECK(XGetWindowAttributes(display, first_native, &attributes));
	XWarpPointer(display, None, first_native, 0, 0, 0, 0, attributes.width / 2 + 7,
	             attributes.height / 2 + 3);
	events         = collect(display);
	double delta_x = 0, delta_y = 0;
	bool   moved = false;
	for (const auto &event : events)
		if (event.type == platform_event_type::pointer_move && event.window == first_id) {
			CHECK(event.delta_units == pointer_delta_unit::logical);
			delta_x += event.delta_x;
			delta_y += event.delta_y;
			moved = true;
		}
	CHECK(moved && delta_x == 7 && delta_y == 3);
	XSetInputFocus(display, second_native, RevertToPointerRoot, CurrentTime);
	events = collect(display);
	CHECK((*first)->pointer_mode() == pointer_mode::normal);
	CHECK(contains(events, platform_event_type::input_cancelled, first_id));
	XSetInputFocus(display, first_native, RevertToPointerRoot, CurrentTime);
	(void)collect(display);
	CHECK((*first)->set_pointer_mode(pointer_mode::relative));
	(*first)->show(false);
	CHECK((*first)->pointer_mode() == pointer_mode::normal);
	(void)collect(display);
	CHECK(!(*first)->set_pointer_mode(pointer_mode::relative));
	(*first)->show();
	(void)collect(display);
	XSetInputFocus(display, first_native, RevertToPointerRoot, CurrentTime);
	(void)collect(display);
	CHECK((*first)->set_pointer_mode(pointer_mode::relative));
	(void)collect(display);
	CHECK((*first)->resize(400, 240));
	events = collect(display);
	CHECK((*first)->pointer_mode() == pointer_mode::normal);
	CHECK(contains(events, platform_event_type::input_cancelled, first_id));
	XSetInputFocus(display, second_native, RevertToPointerRoot, CurrentTime);
	(void)collect(display);
	first->reset();
	events = collect(display);
	CHECK(contains(events, platform_event_type::window_retired, first_id));
	CHECK(contains(events, platform_event_type::input_cancelled, first_id));
	CHECK(stroke(display, second_native, XK_b, 0, 530));
	input = actions(collect(display));
	CHECK(input.size() == 1 && committed(input[0], second_id, "b"));
	std::puts("Native X11 input passed: layout-independent physical W/text z, two-window "
	          "commits/edit routing, modifiers/repeat, warp motion without synthetic jumps, "
	          "focus/hide/resize capture release and independent retirement");
}
