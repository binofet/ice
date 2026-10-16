#include "ice/platform/desktop_window.h"
#import <AppKit/AppKit.h>
#include <cstdio>
#include <vector>
#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::fprintf(stderr, "AppKit physical input failed at %d: %s\n", __LINE__,             \
			             #condition);                                                              \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)
namespace {
using namespace ice::platform;
std::vector<platform_event> take() {
	std::vector<platform_event> events;
	platform_event              event;
	while (platform_events::poll(event))
		events.push_back(std::move(event));
	return events;
}
NSEvent *key(NSWindow *window, NSEventType type) {
	return [NSEvent keyEventWithType:type
	                        location:NSZeroPoint
	                   modifierFlags:0
	                       timestamp:NSProcessInfo.processInfo.systemUptime
	                    windowNumber:window.windowNumber
	                         context:nil
	                      characters:@"z"
	     charactersIgnoringModifiers:@"z"
	                       isARepeat:NO
	                         keyCode:13];
}
bool has(const std::vector<platform_event> &events, platform_event_type type, window_id window) {
	for (const auto &event : events)
		if (event.type == type && event.window == window)
			return true;
	return false;
}
bool focus(NSWindow *window) {
	[window makeKeyWindow];
	// Activation requested by native creation completes asynchronously. Give this
	// test's own application a bounded chance to receive it; never force or unlock.
	NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:.5];
	bool    stable   = false;
	do {
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, .01, false);
		desktop_window::pump_events();
		if (NSApp.isActive && window.isKeyWindow) {
			if (stable)
				return true;
			stable = true;
		} else
			stable = false;
	} while (deadline.timeIntervalSinceNow > 0);
	return false;
}
} // namespace
int main() {
	@autoreleasepool {
		using namespace ice::platform;
		auto first  = desktop_window::create({"ICE physical input regression", 640, 400, true});
		auto second = desktop_window::create({"ICE physical routing regression", 640, 400, false});
		CHECK(first && second);
		auto native =
		    (__bridge NSWindow *)reinterpret_cast<void *>((*first)->native_handle().window);
		auto view = (__bridge NSView *)(*first)->native_handle().view;
		if (!focus(native)) {
			std::fputs(
			    "AppKit physical input unavailable: desktop locked/inactive; skipped, not passed\n",
			    stderr);
			return 77;
		}
		(*first)->enable_text_input();
		take();
		NSEvent *down_event = key(native, NSEventTypeKeyDown);
		NSEvent *up_event   = key(native, NSEventTypeKeyUp);
		CHECK(down_event && up_event && down_event.window == native && up_event.window == native);
		[NSApp postEvent:down_event atStart:NO];
		[NSApp postEvent:up_event atStart:NO];
		desktop_window::pump_events();
		auto events = take();
		bool down = false, up = false, text = false;
		for (const auto &event : events) {
			if (event.type == platform_event_type::physical_key && event.window == (*first)->id()) {
				CHECK(event.key == physical_key::w);
				if (event.pressed)
					down = true;
				else
					up = true;
			}
			if (event.type == platform_event_type::text_committed && event.text == "z")
				text = true;
		}
		if (!(down && up && text)) {
			std::fprintf(stderr, "AppKit active=%d key=%d visible=%d responder=%s events=%zu\n",
			             NSApp.isActive, native.isKeyWindow, native.isVisible,
			             NSStringFromClass(native.firstResponder.class).UTF8String, events.size());
			for (const auto &event : events)
				std::fprintf(stderr,
				             "Observed AppKit event type=%d window=%llu key=%u down=%d "
				             "text_bytes=%zu matches_z=%d\n",
				             static_cast<int>(event.type),
				             static_cast<unsigned long long>(event.window),
				             static_cast<unsigned>(event.key), event.pressed, event.text.size(),
				             event.text == "z");
		}
		CHECK(down && up && text);
		(*first)->enable_text_input(false);
		take();
		[NSApp postEvent:key(native, NSEventTypeKeyDown) atStart:NO];
		desktop_window::pump_events();
		events = take();
		CHECK(has(events, platform_event_type::physical_key, (*first)->id()));
		CHECK(!has(events, platform_event_type::text_committed, (*first)->id()));
		NSEvent *button = [NSEvent mouseEventWithType:NSEventTypeRightMouseDown
		                                     location:NSMakePoint(17, 23)
		                                modifierFlags:0
		                                    timestamp:0
		                                 windowNumber:native.windowNumber
		                                      context:nil
		                                  eventNumber:0
		                                   clickCount:1
		                                     pressure:1];
		[view rightMouseDown:button];
		events = take();
		CHECK(events.size() == 1 && events[0].type == platform_event_type::pointer_button &&
		      events[0].button == mouse_button::secondary && events[0].pressed);
		CHECK(events[0].x == 17 && events[0].y == view.bounds.size.height - 23);
		(*first)->cancel_input();
		take();
		NSEvent *motion = [NSEvent mouseEventWithType:NSEventTypeMouseMoved
		                                     location:NSMakePoint(27, 28)
		                                modifierFlags:0
		                                    timestamp:0
		                                 windowNumber:native.windowNumber
		                                      context:nil
		                                  eventNumber:0
		                                   clickCount:0
		                                     pressure:0];
		[view mouseMoved:button];
		[view mouseMoved:motion];
		events = take();
		CHECK(events.size() == 2 && events[0].delta_x == 0 && events[0].delta_y == 0);
		CHECK(events[1].x == 27 && events[1].y == view.bounds.size.height - 28 &&
		      events[1].delta_x == 10 && events[1].delta_y == -5);
		CHECK(!(*second)->set_pointer_mode(pointer_mode::relative));
		auto captured = (*first)->set_pointer_mode(pointer_mode::relative);
		if (!captured) {
			std::fprintf(stderr, "AppKit relative pointer unavailable: %s; skipped, not passed\n",
			             captured.error().message.c_str());
			return 77;
		}
		CHECK((*first)->pointer_mode() == pointer_mode::relative);
		(*first)->cancel_text_input();
		CHECK((*first)->pointer_mode() == pointer_mode::relative);
		events = take();
		CHECK(events.size() == 1 && events[0].cancellation == input_cancellation::text);
		(*second)->show();
		CHECK(focus(
		    (__bridge NSWindow *)reinterpret_cast<void *>((*second)->native_handle().window)));
		CHECK((*first)->pointer_mode() == pointer_mode::normal);
		events = take();
		CHECK(has(events, platform_event_type::input_cancelled, (*first)->id()));
		(*first)->show();
		CHECK(focus(native));
		CHECK((*first)->set_pointer_mode(pointer_mode::relative));
		take();
		platform_event move{platform_event_type::pointer_move, (*first)->id(), 0};
		for (unsigned index = 0; index < 1024; ++index)
			CHECK(platform_events::post(move));
		CHECK(!platform_events::post(move));
		CHECK((*first)->pointer_mode() == pointer_mode::normal);
		take();
		CHECK((*first)->set_pointer_mode(pointer_mode::relative));
		(*first)->show(false);
		CHECK((*first)->pointer_mode() == pointer_mode::normal);
		(*first)->show();
		CHECK(focus(native));
		CHECK((*first)->set_pointer_mode(pointer_mode::relative));
		(*first)->request_close();
		CHECK((*first)->pointer_mode() == pointer_mode::normal);
		std::puts(
		    "AppKit physical input passed: key/text independence, right button/logical motion, "
		    "focus/overflow/hide/close capture release");
		return 0;
	}
}
