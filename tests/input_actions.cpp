#include "ice/input/actions.h"
#include <cmath>
#include <cstdio>
#include <limits>
#include <utility>

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::fprintf(stderr, "Action check failed at %d: %s\n", __LINE__, #condition);         \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

namespace {
using namespace ice::input;
using namespace ice::platform;
constexpr window_id window = 42;
constexpr action_id move = 1, look_delta = 2, look_rate = 3, jump = 4, sprint = 5, speed = 6,
                    interact = 7, lift = 8;
bool close(double first, double second, double tolerance = 1e-9) {
	return std::abs(first - second) <= tolerance;
}
platform_event key(physical_key code, bool pressed = true, bool repeat = false) {
	platform_event event{platform_event_type::physical_key, window};
	event.key     = code;
	event.pressed = pressed;
	event.repeat  = repeat;
	return event;
}
action_binding bind_key(action_id action, physical_key key, std::array<double, 2> scale = {1, 1}) {
	action_binding binding;
	binding.action = action;
	binding.key    = key;
	binding.scale  = scale;
	return binding;
}
action_binding bind_virtual(action_id action, control_id control, action_kind kind) {
	action_binding binding;
	binding.action          = action;
	binding.control         = control_kind::virtual_control;
	binding.virtual_control = control;
	binding.virtual_kind    = kind;
	return binding;
}
std::vector<action_descriptor> descriptors() {
	return {{move, "player.move", action_kind::axis2},
	        {look_delta, "view.look_delta", action_kind::axis2, action_behavior::delta, false},
	        {look_rate, "view.look_rate", action_kind::axis2, action_behavior::rate, false},
	        {jump, "player.jump"},
	        {sprint, "player.sprint"},
	        {speed, "view.speed", action_kind::axis1, action_behavior::delta, false},
	        {interact, "player.interact"},
	        {lift, "player.lift", action_kind::axis1}};
}
std::vector<action_binding> bindings() {
	std::vector<action_binding> result{
	    bind_key(move, physical_key::w, {0, 1}),    bind_key(move, physical_key::s, {0, -1}),
	    bind_key(move, physical_key::a, {-1, 0}),   bind_key(move, physical_key::d, {1, 0}),
	    bind_key(jump, physical_key::space),        bind_key(sprint, physical_key::left_shift),
	    bind_key(sprint, physical_key::right_shift)};
	action_binding mouse;
	mouse.action             = look_delta;
	mouse.control            = control_kind::pointer_delta;
	mouse.scale              = {.01, .02};
	mouse.device_count_scale = {2, 3};
	result.push_back(mouse);
	mouse.action               = speed;
	mouse.control              = control_kind::scroll;
	mouse.component            = axis_component::y;
	mouse.scale                = {2, 1};
	mouse.precise_scroll_scale = {.5, .25};
	result.push_back(mouse);
	mouse         = {};
	mouse.action  = interact;
	mouse.control = control_kind::mouse_button;
	mouse.button  = mouse_button::primary;
	result.push_back(mouse);
	auto stick     = bind_virtual(move, 1, action_kind::axis2);
	stick.deadzone = .2;
	result.push_back(stick);
	stick       = bind_virtual(look_rate, 2, action_kind::axis2);
	stick.scale = {2, 4};
	result.push_back(stick);
	result.push_back(bind_virtual(look_delta, 3, action_kind::axis2));
	result.push_back(bind_virtual(jump, 4, action_kind::button));
	result.push_back(bind_virtual(lift, 5, action_kind::axis1));
	return result;
}
ice::expected<action_reducer, action_diagnostic> configured() {
	return action_reducer::create(window, descriptors(), bindings());
}
int desktop() {
	auto created = configured();
	CHECK(created);
	auto &input = *created;
	CHECK(!input.focused() && input.context() == input_context::terminal);
	CHECK(input.discover().size() == 8);
	platform_event focus{platform_event_type::focus_gained, window};
	CHECK(input.consume(focus));
	CHECK(input.focused() && input.set_context(input_context::player));
	CHECK(input.consume_tick(0, 0));
	platform_event text{platform_event_type::text_committed, window};
	text.text = "W ";
	CHECK(input.consume(text) && !*input.consume(text));
	CHECK(input.consume(key(physical_key::w)) && input.consume(key(physical_key::d)));
	auto frame = input.consume_tick(1, .01);
	CHECK(frame && close(frame->find(move)->x, std::sqrt(.5)) &&
	      close(frame->find(move)->y, std::sqrt(.5)) && !frame->find(jump)->down);
	CHECK(!frame->find(999));
	CHECK(input.consume(key(physical_key::w, false)));
	frame = input.consume_tick(2, .01);
	CHECK(frame && frame->find(move)->x == 1 && frame->find(move)->y == 0);
	CHECK(input.consume(key(physical_key::left_shift)) &&
	      input.consume(key(physical_key::right_shift)));
	frame = input.consume_tick(3, .01);
	CHECK(frame && frame->find(sprint)->down && frame->find(sprint)->pressed);
	CHECK(input.consume(key(physical_key::left_shift, false)));
	frame = input.consume_tick(4, .01);
	CHECK(frame && frame->find(sprint)->down && !frame->find(sprint)->released &&
	      !frame->find(sprint)->pressed);
	CHECK(input.consume(key(physical_key::right_shift, false)));
	CHECK(input.consume(key(physical_key::space)));
	CHECK(input.consume(key(physical_key::space, true, true)));
	frame = input.consume_tick(5, .01);
	CHECK(frame && frame->find(jump)->pressed && frame->find(jump)->down &&
	      frame->find(sprint)->released);
	frame = input.consume_tick(6, .01);
	CHECK(frame && frame->find(jump)->down && !frame->find(jump)->pressed);
	CHECK(input.consume(key(physical_key::space, false)));
	CHECK(input.consume(key(physical_key::space)));
	CHECK(input.consume(key(physical_key::space, false)));
	frame = input.consume_tick(7, .01);
	CHECK(frame && frame->find(jump)->pressed && frame->find(jump)->released &&
	      !frame->find(jump)->down);
	platform_event motion{platform_event_type::pointer_move, window};
	motion.delta_x = 3;
	motion.delta_y = 4;
	CHECK(input.consume(motion));
	motion.delta_units = pointer_delta_unit::device_counts;
	CHECK(input.consume(motion));
	CHECK(!input.consume_tick(7, .01));
	CHECK(!input.consume_tick(8, -1));
	CHECK(!input.consume_tick(8, 1.01));
	CHECK(!input.consume_tick(8, std::numeric_limits<double>::quiet_NaN()));
	frame = input.consume_tick(8, 0);
	CHECK(frame && close(frame->find(look_delta)->x, .09) &&
	      close(frame->find(look_delta)->y, .32));
	frame = input.consume_tick(9, 1);
	CHECK(frame && frame->find(look_delta)->x == 0 && frame->find(look_delta)->y == 0);
	platform_event wheel{platform_event_type::scroll, window};
	wheel.scroll_y       = 8;
	wheel.precise_scroll = true;
	CHECK(input.consume(wheel));
	frame = input.consume_tick(10, .01);
	CHECK(frame && frame->find(speed)->x == 4); // Selected Y uses Y's precise scale.
	platform_event cancellation{platform_event_type::input_cancelled, window};
	cancellation.cancellation = input_cancellation::text;
	CHECK(input.consume(cancellation) && !*input.consume(cancellation));
	frame = input.consume_tick(11, .01);
	CHECK(frame && frame->find(move)->x == 1 && !frame->cancelled);
	CHECK(input.set_context(input_context::terminal));
	CHECK(input.consume(key(physical_key::space)));
	CHECK(input.set_context(input_context::player));
	CHECK(input.consume(key(physical_key::space, true, true)));
	frame = input.consume_tick(12, .01);
	CHECK(frame && frame->cancelled && !frame->find(jump)->pressed && !frame->find(jump)->down &&
	      frame->find(move)->x == 0);
	CHECK(input.consume(key(physical_key::space, false)) &&
	      input.consume(key(physical_key::space)));
	frame = input.consume_tick(13, .01);
	CHECK(frame && frame->find(jump)->pressed);
	cancellation.window       = window + 1;
	cancellation.cancellation = input_cancellation::all;
	CHECK(input.consume(cancellation) && !*input.consume(cancellation));
	frame = input.consume_tick(14, .01);
	CHECK(frame && frame->find(jump)->down && !frame->cancelled);
	CHECK(input.consume(motion));
	focus.type = platform_event_type::focus_lost;
	CHECK(input.consume(focus));
	frame = input.consume_tick(15, .01);
	CHECK(frame && frame->cancelled && frame->find(jump)->released &&
	      frame->find(look_delta)->x == 0 && !input.focused());
	focus.type = platform_event_type::focus_gained;
	CHECK(input.consume(focus));
	CHECK(input.consume(key(physical_key::space, true, true)));
	frame = input.consume_tick(16, .01);
	CHECK(frame && !frame->find(jump)->down && !frame->find(jump)->pressed);
	// A fresh native keydown after a release outside the app is an observed new press.
	CHECK(input.consume(key(physical_key::space)));
	frame = input.consume_tick(17, .01);
	CHECK(frame && frame->find(jump)->pressed);
	cancellation.window = application_scope;
	CHECK(input.consume(cancellation));
	frame = input.consume_tick(18, .01);
	CHECK(frame && frame->cancelled && !frame->find(jump)->down);
	return 0;
}
int virtual_controls() {
	auto created = configured();
	CHECK(created);
	auto &input = *created;
	input.set_focused(true);
	CHECK(input.set_context(input_context::player));
	auto touch = input.add_source(), xr = input.add_source();
	CHECK(touch && xr && *touch != *xr && *touch != desktop_source);
	CHECK(input.submit(*touch, {1, action_kind::axis2, 0, .6}));
	auto frame = input.consume_tick(0, .1);
	CHECK(frame && frame->find(move)->y == 0); // Unknown held analog must first center.
	CHECK(input.submit(*touch, {1, action_kind::axis2, 0, 0}));
	CHECK(input.submit(*touch, {1, action_kind::axis2, 0, .6}));
	CHECK(input.consume(key(physical_key::d)));
	frame = input.consume_tick(1, .1);
	CHECK(frame && close(frame->find(move)->x, 1 / std::sqrt(1.25)) &&
	      close(frame->find(move)->y, .5 / std::sqrt(1.25)));
	CHECK(input.consume(key(physical_key::d, false)));
	frame = input.consume_tick(2, .1);
	CHECK(frame && close(frame->find(move)->y, .5) && frame->find(move)->x == 0);
	CHECK(input.submit(*xr, {2, action_kind::axis2}));
	CHECK(input.submit(*xr, {2, action_kind::axis2, 1, 0}));
	CHECK(input.submit(*touch, {3, action_kind::axis2, .25, -.5}));
	frame = input.consume_tick(3, .1);
	CHECK(frame && frame->find(look_rate)->behavior == action_behavior::rate &&
	      close(frame->find(look_rate)->x, .2) && close(frame->find(look_delta)->x, .25));
	frame = input.consume_tick(4, .25);
	CHECK(frame && close(frame->find(look_rate)->x, .5) && frame->find(look_delta)->x == 0);
	frame = input.consume_tick(5, 0);
	CHECK(frame && frame->find(look_rate)->x == 0);
	CHECK(input.submit(*xr, {4, action_kind::button, 0}));
	CHECK(input.submit(*xr, {4, action_kind::button, 1}));
	CHECK(input.consume(key(physical_key::space)));
	frame = input.consume_tick(6, .01);
	CHECK(frame && frame->find(jump)->pressed && frame->find(jump)->down);
	CHECK(input.consume(key(physical_key::space, false)));
	frame = input.consume_tick(7, .01);
	CHECK(frame && frame->find(jump)->down && !frame->find(jump)->released);
	CHECK(input.submit(*xr, {4, action_kind::button, 0, 0, false}));
	frame = input.consume_tick(8, .01);
	CHECK(frame && frame->cancelled && frame->find(jump)->released && !frame->find(jump)->down);
	CHECK(input.submit(*xr, {4, action_kind::button, 1}));
	frame = input.consume_tick(9, .01);
	CHECK(frame && !frame->find(jump)->down && !frame->find(jump)->pressed);
	CHECK(input.submit(*xr, {4, action_kind::button, 0}));
	CHECK(input.submit(*xr, {4, action_kind::button, 1}));
	frame = input.consume_tick(10, .01);
	CHECK(frame && frame->find(jump)->pressed);
	CHECK(input.set_context(input_context::terminal));
	CHECK(input.set_context(input_context::player));
	CHECK(input.submit(*touch, {1, action_kind::axis2, 0, .6}));
	CHECK(input.submit(*xr, {4, action_kind::button, 1}));
	frame = input.consume_tick(11, .01);
	CHECK(frame && frame->find(move)->y == 0 && !frame->find(jump)->down &&
	      !frame->find(jump)->pressed);
	CHECK(input.submit(*touch, {1, action_kind::axis2, .1, .1})); // Within the radial deadzone.
	CHECK(input.submit(*touch, {1, action_kind::axis2, 0, .6}));
	CHECK(input.submit(*xr, {4, action_kind::button, 0}));
	CHECK(input.submit(*xr, {4, action_kind::button, 1}));
	frame = input.consume_tick(12, .01);
	CHECK(frame && close(frame->find(move)->y, .5) && frame->find(jump)->pressed);
	CHECK(input.submit(*xr, {2, action_kind::axis2}));
	CHECK(input.submit(*xr, {2, action_kind::axis2, 1, 0}));
	CHECK(input.submit(*touch, {5, action_kind::axis1, 0}));
	CHECK(input.submit(*touch, {5, action_kind::axis1, -.75}));
	frame = input.consume_tick(13, .01);
	CHECK(frame && frame->find(lift)->x == -.75 && frame->find(lift)->y == 0);
	CHECK(input.submit(*touch, {3, action_kind::axis2, 3, 4}));
	CHECK(input.submit(*xr, {3, action_kind::axis2, 5, 6}));
	platform_event click{platform_event_type::pointer_button, window};
	click.button  = mouse_button::primary;
	click.pressed = true;
	CHECK(input.consume(click));
	click.pressed = false;
	CHECK(input.consume(click));
	CHECK(input.cancel_source(*touch));
	frame = input.consume_tick(14, .1);
	CHECK(frame && frame->cancelled && frame->find(look_delta)->x == 5 &&
	      frame->find(look_delta)->y == 6 && frame->find(interact)->pressed &&
	      frame->find(interact)->released && frame->find(jump)->down);
	CHECK(input.remove_source(*xr));
	frame = input.consume_tick(15, .1);
	CHECK(frame && frame->find(jump)->released && frame->find(look_rate)->x == 0);
	CHECK(!input.submit(*xr, {2, action_kind::axis2}) && !input.remove_source(*xr));
	auto replacement = input.add_source();
	CHECK(replacement && *replacement != *xr);
	auto other = configured();
	CHECK(other && !other->submit(*touch, {1, action_kind::axis2}));
	// Repeated inactive polling is idempotent and does not cancel unrelated input each frame.
	CHECK(input.submit(*touch, {1, action_kind::axis2, 0, 0, false}));
	frame = input.consume_tick(16, .01);
	CHECK(frame && !frame->cancelled); // cancel_source already made this control inactive.
	CHECK(input.submit(*touch, {1, action_kind::axis2, 0, 0, false}));
	frame = input.consume_tick(17, .01);
	CHECK(frame && !frame->cancelled);
	return 0;
}
int validation() {
	auto original = descriptors();
	auto invalid  = original;
	invalid[1].id = invalid[0].id;
	CHECK(!action_reducer::create(window, invalid, bindings()));
	invalid         = original;
	invalid[0].name = "";
	CHECK(!action_reducer::create(window, invalid, bindings()));
	invalid         = original;
	invalid[0].kind = static_cast<action_kind>(99);
	CHECK(!action_reducer::create(window, invalid, bindings()));
	CHECK(!action_reducer::create(0, original, bindings()));
	auto bad_bindings      = bindings();
	bad_bindings[0].action = 999;
	CHECK(!action_reducer::create(window, original, bad_bindings));
	bad_bindings             = bindings();
	bad_bindings[0].scale[0] = std::numeric_limits<double>::infinity();
	CHECK(!action_reducer::create(window, original, bad_bindings));
	bad_bindings              = bindings();
	bad_bindings[10].deadzone = 1;
	CHECK(!action_reducer::create(window, original, bad_bindings));
	bad_bindings = bindings();
	bad_bindings.push_back(bind_virtual(look_delta, 1, action_kind::axis2));
	CHECK(!action_reducer::create(window, original, bad_bindings));
	auto created = configured();
	CHECK(created);
	auto &input = *created;
	input.set_focused(true);
	CHECK(input.set_context(input_context::player) && input.consume(key(physical_key::w)));
	auto source = input.add_source();
	CHECK(source);
	CHECK(!input.submit(desktop_source, {1, action_kind::axis2}));
	CHECK(!input.submit(*source, {99, action_kind::axis2}));
	CHECK(!input.submit(*source, {1, action_kind::axis1}));
	CHECK(!input.submit(*source, {1, action_kind::axis2, 2, 0}));
	CHECK(!input.submit(*source, {4, action_kind::button, .5, 0}));
	CHECK(!input.submit(*source,
	                    {1, action_kind::axis2, std::numeric_limits<double>::quiet_NaN(), 0}));
	CHECK(!input.set_context(static_cast<input_context>(99)));
	auto bad = key(physical_key::unknown);
	CHECK(!input.consume(bad));
	bad = key(physical_key::w, false, true);
	CHECK(!input.consume(bad));
	bad         = key(physical_key::w);
	bad.delta_x = std::numeric_limits<double>::infinity();
	CHECK(!input.consume(bad));
	auto frame = input.consume_tick(0, .01);
	CHECK(frame && frame->find(move)->y == 1 && input.context() == input_context::player);
	input.cancel();
	CHECK(!input.consume_tick(0, .01));
	frame = input.consume_tick(1, .01);
	CHECK(frame && frame->cancelled && frame->find(move)->y == 0);
	CHECK(input.set_context(input_context::player));
	frame = input.consume_tick(2, .01);
	CHECK(frame && !frame->cancelled); // Selecting the same profile is not cancellation.
	action_limits limits;
	limits.sources       = 2;
	limits.contributions = 1;
	auto small           = action_reducer::create(0, {{move, "move", action_kind::axis2}},
	                                              {bind_virtual(move, 1, action_kind::axis2)}, limits);
	CHECK(small);
	small->set_focused(true);
	CHECK(small->set_context(input_context::player));
	auto a = small->add_source(), b = small->add_source();
	CHECK(a && b && !small->add_source());
	CHECK(small->submit(*a, {1, action_kind::axis2}));
	CHECK(small->submit(*a, {1, action_kind::axis2, .25, 0}));
	CHECK(!small->submit(*b, {1, action_kind::axis2}));
	frame = small->consume_tick(0, .01);
	CHECK(frame && frame->find(move)->x == .25);
	CHECK(small->remove_source(*a));
	CHECK(small->submit(*b, {1, action_kind::axis2}));
	CHECK(small->submit(*b, {1, action_kind::axis2, .5, 0}));
	frame = small->consume_tick(1, .01);
	CHECK(frame && frame->find(move)->x == .5);
	auto moved = std::move(*small);
	CHECK(!small->add_source() && !small->consume_tick(2, .01) && small->discover().empty());
	CHECK(moved.submit(*b, {1, action_kind::axis2, -.5, 0}));
	frame = moved.consume_tick(2, .01);
	CHECK(frame && frame->find(move)->x == -.5);
	return 0;
}
int lifecycle_virtual() {
	auto created = configured();
	CHECK(created);
	auto &input = *created;
	input.set_focused(true);
	CHECK(input.set_context(input_context::player));
	auto a = input.add_source(), b = input.add_source();
	CHECK(a && b);
	CHECK(input.submit(*a, {4, action_kind::button, 0}));
	CHECK(input.submit(*a, {4, action_kind::button, 1}));
	CHECK(input.submit(*b, {1, action_kind::axis2}));
	CHECK(input.submit(*b, {1, action_kind::axis2, 1, 0}));
	auto frame = input.consume_tick(0, .01);
	CHECK(frame && frame->find(jump)->pressed && frame->find(move)->x == 1);
	platform_event cancel{platform_event_type::input_cancelled, application_scope};
	cancel.cancellation = input_cancellation::text;
	CHECK(input.consume(cancel));
	frame = input.consume_tick(1, .01);
	CHECK(frame && !frame->cancelled && frame->find(jump)->down && frame->find(move)->x == 1);
	cancel.cancellation = input_cancellation::all;
	CHECK(input.consume(cancel));
	frame = input.consume_tick(2, .01);
	CHECK(frame && frame->cancelled && frame->find(jump)->released && frame->find(move)->x == 0);
	CHECK(input.submit(*a, {4, action_kind::button, 1}));
	CHECK(input.submit(*b, {1, action_kind::axis2, 1, 0}));
	frame = input.consume_tick(3, .01);
	CHECK(frame && !frame->find(jump)->down && frame->find(move)->x == 0);
	CHECK(input.submit(*a, {4, action_kind::button, 0}));
	CHECK(input.submit(*a, {4, action_kind::button, 1}));
	CHECK(input.submit(*b, {1, action_kind::axis2}));
	CHECK(input.submit(*b, {1, action_kind::axis2, 1, 0}));
	frame = input.consume_tick(4, .01);
	CHECK(frame && frame->find(jump)->pressed && frame->find(move)->x == 1);
	input.set_focused(false);
	CHECK(input.submit(*a, {4, action_kind::button, 0}));
	CHECK(input.submit(*a, {4, action_kind::button, 1}));
	input.set_focused(true);
	frame = input.consume_tick(5, .01);
	CHECK(frame && !frame->find(jump)->down && frame->find(move)->x == 0);
	CHECK(!input.remove_source(desktop_source));
	return 0;
}
int shared_pending_edge() {
	auto created =
	    action_reducer::create(0, {{jump, "jump"}}, {bind_virtual(jump, 4, action_kind::button)});
	CHECK(created);
	auto &input = *created;
	input.set_focused(true);
	CHECK(input.set_context(input_context::player));
	auto a = input.add_source(), b = input.add_source();
	CHECK(a && b);
	CHECK(input.submit(*a, {4, action_kind::button, 0}));
	CHECK(input.submit(*b, {4, action_kind::button, 0}));
	CHECK(input.submit(*a, {4, action_kind::button, 1}));
	CHECK(input.submit(*b, {4, action_kind::button, 1}));
	CHECK(input.cancel_source(*a));
	auto frame = input.consume_tick(0, .01);
	CHECK(frame && frame->find(jump)->pressed && frame->find(jump)->down);
	frame = input.consume_tick(1, .01);
	CHECK(frame && !frame->find(jump)->pressed && frame->find(jump)->down);
	CHECK(input.submit(*a, {4, action_kind::button, 0}));
	CHECK(input.submit(*a, {4, action_kind::button, 1}));
	frame = input.consume_tick(2, .01);
	CHECK(frame && !frame->find(jump)->pressed && frame->find(jump)->down);
	CHECK(input.remove_source(*b));
	frame = input.consume_tick(3, .01);
	CHECK(frame && !frame->find(jump)->released && frame->find(jump)->down);
	CHECK(input.submit(*a, {4, action_kind::button, 0}));
	frame = input.consume_tick(4, .01);
	CHECK(frame && frame->find(jump)->released && !frame->find(jump)->down);
	return 0;
}
} // namespace
int main() {
	CHECK(desktop() == 0);
	CHECK(virtual_controls() == 0);
	CHECK(validation() == 0);
	CHECK(lifecycle_virtual() == 0);
	CHECK(shared_pending_edge() == 0);
	std::puts(
	    "Input actions passed: contexts/text separation, typed multi-source controls, neutral "
	    "rearming, edge/delta/rate ticks, cancellation, bounds and source lifetimes");
	return 0;
}
