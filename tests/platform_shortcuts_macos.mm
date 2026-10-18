#include "ice/platform/desktop_window.h"
#import <AppKit/AppKit.h>
#include <cstdio>
#include <vector>

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::fprintf(stderr, "AppKit shortcut failed at %d: %s\n", __LINE__, #condition);      \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

@interface ICEShortcutOtherResponder : NSView
@end
@implementation ICEShortcutOtherResponder
- (BOOL)acceptsFirstResponder {
	return YES;
}
@end

namespace {
using namespace ice::platform;
std::vector<platform_event> take() {
	std::vector<platform_event> result;
	platform_event              event;
	while (platform_events::poll(event))
		result.push_back(std::move(event));
	return result;
}
NSEvent *key(NSWindow *window, NSEventType type, NSString *characters,
             NSEventModifierFlags flags = NSEventModifierFlagCommand, BOOL repeated = NO,
             unsigned short code = 9) {
	return [NSEvent keyEventWithType:type
	                        location:NSZeroPoint
	                   modifierFlags:flags
	                       timestamp:NSProcessInfo.processInfo.systemUptime
	                    windowNumber:window.windowNumber
	                         context:nil
	                      characters:characters
	     charactersIgnoringModifiers:characters
	                       isARepeat:repeated
	                         keyCode:code];
}
bool focus(NSWindow *window) {
	[window makeKeyWindow];
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
bool shortcut(const std::vector<platform_event> &events, window_id window, edit_command command,
              physical_key physical, bool repeated, bool released) {
	unsigned downs = 0, ups = 0, activities = 0, edits = 0;
	for (const auto &event : events) {
		if (event.window != window)
			continue;
		if (event.type == platform_event_type::text_committed ||
		    event.type == platform_event_type::text_composition ||
		    event.type == platform_event_type::input_cancelled)
			return false;
		if (event.type == platform_event_type::physical_key) {
			if (event.key != physical || !has_modifier(event.modifiers, key_modifier::super) ||
			    event.repeat != (event.pressed && repeated))
				return false;
			event.pressed ? ++downs : ++ups;
		} else if (event.type == platform_event_type::input_activity) {
			if (!has_modifier(event.modifiers, key_modifier::super) || event.repeat != repeated)
				return false;
			++activities;
		} else if (event.type == platform_event_type::edit_command) {
			if (event.command != command || !has_modifier(event.modifiers, key_modifier::super) ||
			    event.repeat != repeated)
				return false;
			++edits;
		}
	}
	if (downs != 1 || ups != unsigned(released) || activities != 1 || edits != 1) {
		std::fprintf(stderr, "shortcut counts: down=%u up=%u activity=%u edit=%u\n", downs, ups,
		             activities, edits);
		return false;
	}
	return true;
}
bool no_text_edits(const std::vector<platform_event> &events) {
	for (const auto &event : events)
		if (event.type == platform_event_type::edit_command ||
		    event.type == platform_event_type::text_committed ||
		    event.type == platform_event_type::text_composition)
			return false;
	return true;
}
} // namespace

int main() {
	@autoreleasepool {
		using namespace ice::platform;
		auto first  = desktop_window::create({"ICE command shortcut regression", 480, 260, true});
		auto second = desktop_window::create({"ICE inactive shortcut target", 480, 260, false});
		CHECK(first && second);
		auto native =
		    (__bridge NSWindow *)reinterpret_cast<void *>((*first)->native_handle().window);
		auto view     = (__bridge NSView *)(*first)->native_handle().view;
		auto inactive = (__bridge NSView *)(*second)->native_handle().view;
		if (!focus(native)) {
			std::fputs(
			    "AppKit shortcut unavailable: desktop locked/inactive; skipped, not passed\n",
			    stderr);
			return 77;
		}
		CHECK([native makeFirstResponder:view]);
		(*first)->enable_text_input();
		(*second)->enable_text_input();
		take();
		// Exercise the real NSApplication queue first. Direct paste: alone would pass
		// the old implementation and hide the missing command-key routing.
		[NSApp postEvent:key(native, NSEventTypeKeyDown, @"v") atStart:NO];
		[NSApp postEvent:key(native, NSEventTypeKeyUp, @"v") atStart:NO];
		desktop_window::pump_events();
		CHECK(shortcut(take(), (*first)->id(), edit_command::paste, physical_key::v, false, true));

		struct binding {
			NSString    *characters;
			edit_command command;
		};
		const binding bindings[] = {{@"x", edit_command::cut},
		                            {@"c", edit_command::copy},
		                            {@"v", edit_command::paste},
		                            {@"a", edit_command::select_all}};
		for (const auto &binding : bindings) {
			for (BOOL repeated : {NO, YES}) {
				// Deliberately use physical W with each logical shortcut character.
				// This checks layout-aware matching independently of the scan code.
				CHECK([view performKeyEquivalent:key(native, NSEventTypeKeyDown, binding.characters,
				                                     NSEventModifierFlagCommand, repeated, 13)]);
				[view keyUp:key(native, NSEventTypeKeyUp, binding.characters,
				                NSEventModifierFlagCommand, NO, 13)];
				CHECK(shortcut(take(), (*first)->id(), binding.command, physical_key::w, repeated,
				               true));
			}
		}
		CHECK(![view performKeyEquivalent:key(native, NSEventTypeKeyDown, @"j")]);
		CHECK(![view performKeyEquivalent:key(native, NSEventTypeKeyDown, @"v", 0)]);
		CHECK(![view performKeyEquivalent:key(native, NSEventTypeKeyDown, @"v",
		                                      NSEventModifierFlagControl)]);
		CHECK(take().empty());
		CHECK(![inactive performKeyEquivalent:key(native, NSEventTypeKeyDown, @"v")]);
		CHECK(take().empty());

		ICEShortcutOtherResponder *other = [ICEShortcutOtherResponder new];
		[view addSubview:other];
		CHECK([native makeFirstResponder:other]);
		CHECK(![view performKeyEquivalent:key(native, NSEventTypeKeyDown, @"v")]);
		CHECK(take().empty());
		CHECK([native makeFirstResponder:view]);
		[other removeFromSuperview];

		(*first)->enable_text_input(false);
		take();
		CHECK(![view performKeyEquivalent:key(native, NSEventTypeKeyDown, @"v")]);
		CHECK(take().empty());
		[NSApp postEvent:key(native, NSEventTypeKeyDown, @"v") atStart:NO];
		desktop_window::pump_events();
		CHECK(no_text_edits(take()));
		// Raw input remains independent of text activation on its existing path.
		[view keyDown:key(native, NSEventTypeKeyDown, @"z", 0, NO, 13)];
		[view keyUp:key(native, NSEventTypeKeyUp, @"z", 0, NO, 13)];
		auto events = take();
		CHECK(no_text_edits(events));
		unsigned physical = 0;
		for (const auto &event : events)
			if (event.type == platform_event_type::physical_key && event.key == physical_key::w)
				++physical;
		CHECK(physical == 2);

		(*first)->enable_text_input();
		take();
		id<NSTextInputClient> client = (id<NSTextInputClient>)view;
		[client setMarkedText:@"日本"
		        selectedRange:NSMakeRange(2, 0)
		     replacementRange:NSMakeRange(NSNotFound, 0)];
		CHECK(client.hasMarkedText);
		[client insertText:@"日本🦊" replacementRange:NSMakeRange(NSNotFound, 0)];
		CHECK(!client.hasMarkedText);
		events                = take();
		unsigned compositions = 0, commits = 0;
		for (const auto &event : events) {
			if (event.type == platform_event_type::text_composition && event.text == "日本")
				++compositions;
			if (event.type == platform_event_type::text_committed && event.text == "日本🦊")
				++commits;
			CHECK(event.type != platform_event_type::edit_command);
		}
		CHECK(compositions == 1 && commits == 1);
		std::puts(
		    "AppKit shortcuts passed: queued Cmd+V, exactly one edit/raw stream, logical "
		    "characters, repeat, inactive/other responder/text-disabled guards and UTF-8 IME");
		return 0;
	}
}
