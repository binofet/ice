#include "ice/platform/desktop_window.h"
#import <AppKit/AppKit.h>
#include <cmath>
#include <cstdio>

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::fprintf(stderr, "AppKit input failed at line %d: %s\n", __LINE__, #condition);    \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

static void drain() {
	ice::platform::platform_event event;
	while (ice::platform::platform_events::poll(event)) {
	}
}
static bool find(ice::platform::platform_event_type type, ice::platform::window_id window,
                 ice::platform::platform_event &found) {
	ice::platform::platform_event event;
	while (ice::platform::platform_events::poll(event))
		if (event.type == type && event.window == window) {
			found = std::move(event);
			return true;
		}
	return false;
}
int main() {
	@autoreleasepool {
		using namespace ice::platform;
		auto first  = desktop_window::create({"ICE native text input check", 640, 400, true});
		auto second = desktop_window::create({"ICE independent text routing", 640, 400, false});
		CHECK(first && second);
		(*first)->enable_text_input();
		(*second)->enable_text_input();
		const auto            handle = (*first)->native_handle();
		NSWindow             *native = (__bridge NSWindow *)reinterpret_cast<void *>(handle.window);
		NSView               *view   = (__bridge NSView *)handle.view;
		id<NSTextInputClient> client = (id<NSTextInputClient>)view;
		CHECK([view conformsToProtocol:@protocol(NSTextInputClient)]);
		[native makeKeyWindow];
		desktop_window::pump_events();
		if (!native.isKeyWindow) {
			std::fputs("AppKit input unavailable: no native key window (desktop may be locked or "
			           "inactive); skipped, not passed\n",
			           stderr);
			return 77;
		}
		desktop_window::pump_events();
		drain();
		[client setMarkedText:@"a🦊z"
		        selectedRange:NSMakeRange(1, 2)
		     replacementRange:NSMakeRange(NSNotFound, 0)];
		platform_event event;
		CHECK(find(platform_event_type::text_composition, (*first)->id(), event));
		CHECK(event.text == "a\xF0\x9F\xA6\x8Az" && event.selection_start == 1 &&
		      event.selection_length == 4);
		CHECK(client.hasMarkedText);
		[client insertText:@"日本🦊" replacementRange:NSMakeRange(NSNotFound, 0)];
		CHECK(find(platform_event_type::text_committed, (*first)->id(), event));
		CHECK(event.text == "\xE6\x97\xA5\xE6\x9C\xAC\xF0\x9F\xA6\x8A" && !client.hasMarkedText);
		const unichar embedded_nul[] = {'a', 0, 'b'};
		NSString     *invalid        = [NSString stringWithCharacters:embedded_nul length:3];
		[client insertText:invalid replacementRange:NSMakeRange(NSNotFound, 0)];
		CHECK(find(platform_event_type::input_cancelled, (*first)->id(), event));
		while (platform_events::poll(event))
			CHECK(event.type != platform_event_type::text_committed);
		[client setMarkedText:invalid
		        selectedRange:NSMakeRange(0, 0)
		     replacementRange:NSMakeRange(NSNotFound, 0)];
		CHECK(find(platform_event_type::input_cancelled, (*first)->id(), event));
		CHECK(!client.hasMarkedText);
		CHECK((*first)->set_text_input_rect({17, 29, 2, 20}));
		NSRange      actual{};
		const NSRect candidate = [client firstRectForCharacterRange:NSMakeRange(0, 0)
		                                                actualRange:&actual];
		const NSRect expected  = [native
            convertRectToScreen:[view
                                    convertRect:NSMakeRect(17, view.bounds.size.height - 49, 2, 20)
                                         toView:nil]];
		CHECK(std::fabs(candidate.origin.x - expected.origin.x) < .01 &&
		      std::fabs(candidate.origin.y - expected.origin.y) < .01);
		[client doCommandBySelector:@selector(moveLeft:)];
		CHECK(find(platform_event_type::edit_command, (*first)->id(), event) &&
		      event.command == edit_command::move_left);
		[client doCommandBySelector:@selector(paste:)];
		CHECK(find(platform_event_type::edit_command, (*first)->id(), event) &&
		      event.command == edit_command::paste);
		// Route an actual queued AppKit key event through NSApplication and the window's
		// first responder, independently of direct protocol-method coverage above.
		NSEvent *key = [NSEvent keyEventWithType:NSEventTypeKeyDown
		                                location:NSZeroPoint
		                           modifierFlags:0
		                               timestamp:0
		                            windowNumber:native.windowNumber
		                                 context:nil
		                              characters:@"x"
		             charactersIgnoringModifiers:@"x"
		                               isARepeat:NO
		                                 keyCode:7];
		[NSApp postEvent:key atStart:NO];
		desktop_window::pump_events();
		CHECK(find(platform_event_type::input_activity, (*first)->id(), event));
		drain();
		[client setMarkedText:@"cancel me"
		        selectedRange:NSMakeRange(9, 0)
		     replacementRange:NSMakeRange(NSNotFound, 0)];
		drain();
		(*second)->show();
		NSWindow *second_native =
		    (__bridge NSWindow *)reinterpret_cast<void *>((*second)->native_handle().window);
		[second_native makeKeyWindow];
		desktop_window::pump_events();
		CHECK(!client.hasMarkedText);
		CHECK(find(platform_event_type::input_cancelled, (*first)->id(), event));
		while (platform_events::poll(event))
			CHECK(event.type != platform_event_type::text_committed);
		// Native input systems may deliver a late callback after focus changes.
		// It must not create text or restore discarded marked state in the old client.
		[client insertText:@"late commit" replacementRange:NSMakeRange(NSNotFound, 0)];
		[client setMarkedText:@"late preedit"
		        selectedRange:NSMakeRange(0, 0)
		     replacementRange:NSMakeRange(NSNotFound, 0)];
		[client doCommandBySelector:@selector(insertNewline:)];
		CHECK(!client.hasMarkedText);
		while (platform_events::poll(event))
			CHECK(event.type != platform_event_type::text_committed &&
			      event.type != platform_event_type::text_composition &&
			      event.type != platform_event_type::edit_command);
		(*first)->show();
		[native makeKeyWindow];
		desktop_window::pump_events();
		drain();
		for (int index = 0; index < 1024; ++index)
			CHECK(platform_events::post(
			    platform_event{platform_event_type::input_activity, (*first)->id(), 0}));
		[client setMarkedText:@"overflow"
		        selectedRange:NSMakeRange(8, 0)
		     replacementRange:NSMakeRange(NSNotFound, 0)];
		CHECK(!client.hasMarkedText);
		CHECK(find(platform_event_type::input_cancelled, application_scope, event));
		std::puts("AppKit input passed: native key-event routing, UTF-8 commit/preedit, candidate "
		          "rectangle, interpreted commands, focus cancellation and overflow");
	}
}
