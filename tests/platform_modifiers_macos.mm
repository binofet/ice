#include "ice/input/actions.h"
#include "ice/platform/desktop_window.h"
#import <AppKit/AppKit.h>
#import <IOKit/hidsystem/IOLLEvent.h>
#include <array>
#include <cstdio>
#import <objc/runtime.h>
#include <stdexcept>
#include <vector>

namespace {
using namespace ice::platform;
unsigned checks  = 0;
bool     focused = true, dispatching = false;
id       focus_gate;
void     check(bool condition, const char *expression, int line) {
    ++checks;
    if (!condition)
        throw std::runtime_error("Mac modifier check " + std::to_string(line) + ": " + expression);
}
#define CHECK(x) check(bool(x), #x, __LINE__)
} // namespace
@interface ICETestKeyWindowGate : NSObject
- (BOOL)isKeyWindow;
@end
@implementation ICETestKeyWindowGate
- (BOOL)isKeyWindow {
	return focused;
}
@end
@protocol ICETestPhysicalView
@property(nonatomic, assign) ice::platform::desktop_window *owner;
- (void)flagsChanged:(NSEvent *)event;
- (void)cancelPhysical;
@end
namespace {
// Expose the test focus gate only during producer dispatch, never during NSView lifetime.
id key_window(id, SEL) { return dispatching ? focus_gate : nil; }
using test_view = id<ICETestPhysicalView>;
class memory_window final : public desktop_window {
public:
	test_view __unsafe_unretained view = nil;
	~memory_window() override { view.owner = nullptr; }
	void                 show(bool) override {}
	bool                 resize(int, int) override { return false; }
	void                 fullscreen(bool) override {}
	window_extent        extent() const override { return {}; }
	native_window_handle native_handle() const override { return {native_window_kind::appkit}; }

private:
	void cancel_native_physical_input() override { [view cancelPhysical]; }
};
std::vector<platform_event> drain() {
	std::vector<platform_event> events;
	platform_event              event;
	while (platform_events::poll(event))
		events.push_back(event);
	return events;
}
struct family {
	unsigned             left_code, right_code;
	physical_key         left, right;
	NSEventModifierFlags combined, left_mask, right_mask;
	key_modifier         modifier;
};
const std::array families{
    family{56, 60, physical_key::left_shift, physical_key::right_shift, NSEventModifierFlagShift,
           NX_DEVICELSHIFTKEYMASK, NX_DEVICERSHIFTKEYMASK, key_modifier::shift},
    family{59, 62, physical_key::left_control, physical_key::right_control,
           NSEventModifierFlagControl, NX_DEVICELCTLKEYMASK, NX_DEVICERCTLKEYMASK,
           key_modifier::control},
    family{58, 61, physical_key::left_alt, physical_key::right_alt, NSEventModifierFlagOption,
           NX_DEVICELALTKEYMASK, NX_DEVICERALTKEYMASK, key_modifier::alt},
    family{55, 54, physical_key::left_super, physical_key::right_super, NSEventModifierFlagCommand,
           NX_DEVICELCMDKEYMASK, NX_DEVICERCMDKEYMASK, key_modifier::super}};
NSEvent *native_event(unsigned code, NSEventModifierFlags flags) {
	auto *event = [NSEvent keyEventWithType:NSEventTypeFlagsChanged
	                               location:NSZeroPoint
	                          modifierFlags:flags
	                              timestamp:0
	                           windowNumber:0
	                                context:nil
	                             characters:@""
	            charactersIgnoringModifiers:@""
	                              isARepeat:NO
	                                keyCode:code];
	CHECK(event && event.type == NSEventTypeFlagsChanged && event.keyCode == code &&
	      event.modifierFlags == flags);
	return event;
}
NSEvent *transition(const family &kind, unsigned side, bool left, bool right) {
	NSEventModifierFlags flags = (left || right) ? kind.combined : 0;
	if (left)
		flags |= kind.left_mask;
	if (right)
		flags |= kind.right_mask;
	return native_event(side ? kind.right_code : kind.left_code, flags);
}
std::vector<platform_event> dispatch(id<ICETestPhysicalView> view, NSEvent *event) {
	dispatching = true;
	[view flagsChanged:event];
	dispatching = false;
	return drain();
}
void producer(id<ICETestPhysicalView> view, memory_window &owner) {
	// 256 paths of eight transitions for each pair, with and without history resets.
	for (const auto &kind : families)
		for (bool reset_each : {false, true}) {
			for (unsigned path = 0; path < 256; ++path) {
				[view cancelPhysical];
				bool state[2]{};
				for (unsigned step = 0; step < 8; ++step) {
					unsigned side = (path >> step) & 1u;
					state[side]   = !state[side];
					if (reset_each)
						[view cancelPhysical];
					const auto events = dispatch(view, transition(kind, side, state[0], state[1]));
					CHECK(events.size() == 1);
					const auto &event = events.front();
					CHECK(event.type == platform_event_type::physical_key &&
					      event.window == owner.id());
					CHECK(event.key == (side ? kind.right : kind.left));
					CHECK(event.pressed == state[side] && !event.repeat);
					CHECK(bool(event.modifiers & modifier_flag(kind.modifier)) ==
					      (state[0] || state[1]));
				}
			}
		}
	for (const auto &kind : families) {
		// Missing or contradictory sided state cannot create an authoritative press.
		for (auto flags : {kind.combined, kind.left_mask, kind.right_mask}) {
			owner.cancel_input();
			drain();
			const auto held = dispatch(view, transition(kind, 0, true, false));
			CHECK(held.size() == 1 && held.front().pressed);
			const auto events = dispatch(view, native_event(kind.left_code, flags));
			CHECK(events.size() == 1 && events[0].type == platform_event_type::input_cancelled &&
			      events[0].cancellation == input_cancellation::all);
			const auto fresh = dispatch(view, transition(kind, 0, true, false));
			CHECK(fresh.size() == 1 && fresh[0].pressed);
		}
		focused = false;
		CHECK(dispatch(view, native_event(kind.left_code, kind.combined)).empty());
		focused    = true;
		view.owner = nullptr;
		CHECK(dispatch(view, native_event(kind.left_code, kind.combined)).empty());
		view.owner = &owner;
	}
	CHECK(dispatch(view, native_event(0, NSEventModifierFlagShift)).empty());
	CHECK(dispatch(view, native_event(255, NSEventModifierFlagShift)).empty());
	for (auto flags :
	     {NSEventModifierFlags(0), NSEventModifierFlags(NSEventModifierFlagCapsLock)}) {
		const auto events = dispatch(view, native_event(57, flags));
		CHECK(events.size() == 2 && events[0].key == physical_key::caps_lock && events[0].pressed &&
		      events[1].key == physical_key::caps_lock && !events[1].pressed);
	}
}
void reducer(id<ICETestPhysicalView> view, memory_window &owner) {
	using namespace ice::input;
	action_binding left;
	left.action  = 1;
	left.key     = physical_key::left_shift;
	auto right   = left;
	right.key    = physical_key::right_shift;
	auto created = action_reducer::create(owner.id(), {{1, "hold"}}, {left, right});
	CHECK(created);
	auto &input = *created;
	input.set_focused(true);
	CHECK(input.set_context(input_context::player));
	std::uint64_t tick  = 0;
	auto          route = [&](const std::vector<platform_event> &events) {
        for (const auto &event : events)
            CHECK(input.consume(event));
        auto frame = input.consume_tick(++tick, .01);
        CHECK(frame);
        return *frame->find(1);
	};
	owner.cancel_input();
	route(drain());
	auto sample = route(dispatch(view, transition(families[0], 0, true, false)));
	CHECK(sample.down && sample.pressed);
	sample = route(dispatch(view, transition(families[0], 1, true, true)));
	CHECK(sample.down && !sample.pressed && !sample.released);
	sample = route(dispatch(view, transition(families[0], 0, false, true)));
	CHECK(sample.down && !sample.released);
	owner.cancel_input();
	sample = route(drain());
	CHECK(!sample.down && !sample.pressed);
	sample = route(dispatch(view, transition(families[0], 1, false, false)));
	CHECK(!sample.down && !sample.pressed);
	sample = route(dispatch(view, transition(families[0], 0, true, false)));
	CHECK(sample.down && sample.pressed);
	owner.cancel_input();
	route(drain());
	dispatch(view, transition(families[0], 1, true, true));
	owner.cancel_input();
	route(drain());
	sample = route(dispatch(view, transition(families[0], 0, false, true)));
	CHECK(!sample.down && !sample.pressed);
	sample = route(dispatch(view, transition(families[0], 1, false, false)));
	CHECK(!sample.down && !sample.pressed);
	sample = route(dispatch(view, transition(families[0], 0, true, false)));
	CHECK(sample.down && sample.pressed);
	sample = route(dispatch(view, native_event(56, NSEventModifierFlagShift)));
	CHECK(!sample.down && !sample.pressed);
}
} // namespace
int main() {
	@autoreleasepool {
		try {
			CHECK(NSApp == nil);
			Class production = objc_getClass("ICEDesktopTextView");
			CHECK(production);
			Class test_class =
			    objc_allocateClassPair(production, "ICETestOffscreenModifierView", 0);
			CHECK(test_class);
			CHECK(class_addMethod(test_class, @selector(window), (IMP)key_window, "@@:"));
			objc_registerClassPair(test_class);
			for (SEL selector : { @selector(flagsChanged:), @selector(physicalKey:pressed:) })
				CHECK(method_getImplementation(class_getInstanceMethod(test_class, selector)) ==
				      method_getImplementation(class_getInstanceMethod(production, selector)));
			focus_gate     = [ICETestKeyWindowGate new];
			NSView *object = [(id)test_class alloc];
			object         = [object initWithFrame:NSZeroRect];
			CHECK(object);
			id<ICETestPhysicalView> view = (id<ICETestPhysicalView>)object;
			memory_window           owner;
			owner.view = view;
			view.owner = &owner;
			drain();
			producer(view, owner);
			reducer(view, owner);
			CHECK(NSApp == nil);
			std::printf("Mac modifier producer passed %u checks: 16384 actual-method transitions, "
			            "four sided families, cancellation recovery, malformed-mask cancellation, "
			            "focus/owner guards, Caps Lock and real reducer aggregation; no "
			            "NSApp/NSWindow/pump/device acceptance.\n",
			            checks);
			return 0;
		} catch (const std::exception &error) {
			std::fprintf(stderr, "%s\n", error.what());
			return 1;
		}
	}
}
