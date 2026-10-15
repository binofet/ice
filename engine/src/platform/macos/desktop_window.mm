#include "../desktop_window_internal.h"
#import <AppKit/AppKit.h>
#include <algorithm>
#include <cstring>
#include <vector>

namespace {
std::vector<ice::platform::desktop_window *> windows;
ice::platform::key_modifiers                 modifiers(NSEventModifierFlags flags) {
    using namespace ice::platform;
    key_modifiers result = 0;
    if (flags & NSEventModifierFlagShift)
        result |= modifier_flag(key_modifier::shift);
    if (flags & NSEventModifierFlagControl)
        result |= modifier_flag(key_modifier::control);
    if (flags & NSEventModifierFlagOption)
        result |= modifier_flag(key_modifier::alt);
    if (flags & NSEventModifierFlagCommand)
        result |= modifier_flag(key_modifier::super);
    if (flags & NSEventModifierFlagCapsLock)
        result |= modifier_flag(key_modifier::caps_lock);
    return result;
}
std::size_t utf8_offset(NSString *text, NSUInteger offset) {
	offset = std::min(offset, text.length);
	if (offset && offset < text.length &&
	    CFStringIsSurrogateHighCharacter([text characterAtIndex:offset - 1]) &&
	    CFStringIsSurrogateLowCharacter([text characterAtIndex:offset]))
		--offset;
	return [[text substringToIndex:offset] lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
}
} // namespace
@interface ICEDesktopTextView : NSView <NSTextInputClient>
@property(nonatomic, assign) ice::platform::desktop_window *owner;
@property(nonatomic, copy) NSString                        *marked;
@property(nonatomic) NSRange                                selection;
@property(nonatomic) ice::platform::key_modifiers           modifiers;
@property(nonatomic) BOOL                                   repeated;
@property(nonatomic, strong) NSTextInputContext            *context;
- (void)cancelPreedit;
@end
@implementation ICEDesktopTextView
- (BOOL)acceptsFirstResponder {
	return YES;
}
- (NSTextInputContext *)inputContext {
	if (!self.owner || !self.owner->text_input_enabled() || !self.window.isKeyWindow)
		return nil;
	if (!self.context)
		self.context = [[NSTextInputContext alloc] initWithClient:self];
	return self.context;
}
- (void)mouseDown:(NSEvent *)event {
	(void)event;
	[self.window makeFirstResponder:self];
}
- (void)keyDown:(NSEvent *)event {
	if (!self.owner || !self.window.isKeyWindow)
		return;
	self.modifiers = modifiers(event.modifierFlags);
	self.repeated  = event.isARepeat;
	ice::platform::platform_event activity{ice::platform::platform_event_type::input_activity,
	                                       self.owner->id(), 0};
	activity.modifiers = self.modifiers;
	activity.repeat    = self.repeated;
	if (!ice::platform::platform_events::post(std::move(activity)))
		[self cancelPreedit];
	if (self.owner->text_input_enabled())
		[self interpretKeyEvents:@[ event ]];
}
- (void)insertText:(id)value replacementRange:(NSRange)replacement {
	(void)replacement;
	if (!self.owner || !self.owner->text_input_enabled() || !self.window.isKeyWindow)
		return;
	NSString *text = [value isKindOfClass:NSAttributedString.class] ? [value string] : value;
	if (![text isKindOfClass:NSString.class] ||
	    [text lengthOfBytesUsingEncoding:NSUTF8StringEncoding] > 4096) {
		self.owner->cancel_text_input();
		return;
	}
	const char                   *bytes = text.UTF8String;
	ice::platform::platform_event event{ice::platform::platform_event_type::text_committed,
	                                    self.owner->id(), 0};
	event.text = bytes ? std::string(bytes, [text lengthOfBytesUsingEncoding:NSUTF8StringEncoding])
	                   : std::string("\xFF");
	event.modifiers = self.modifiers;
	event.repeat    = self.repeated;
	if (!ice::platform::platform_events::post(std::move(event)))
		[self cancelPreedit];
	self.marked    = @"";
	self.selection = NSMakeRange(0, 0);
}
- (void)setMarkedText:(id)value
        selectedRange:(NSRange)selection
     replacementRange:(NSRange)replacement {
	(void)replacement;
	if (!self.owner || !self.owner->text_input_enabled() || !self.window.isKeyWindow)
		return;
	NSString *text = [value isKindOfClass:NSAttributedString.class] ? [value string] : value;
	if (![text isKindOfClass:NSString.class] ||
	    [text lengthOfBytesUsingEncoding:NSUTF8StringEncoding] > 4096 ||
	    selection.location > text.length || selection.length > text.length - selection.location) {
		self.owner->cancel_text_input();
		return;
	}
	self.marked    = text;
	self.selection = selection;
	ice::platform::platform_event event{ice::platform::platform_event_type::text_composition,
	                                    self.owner->id(), 0};
	const char                   *bytes = text.UTF8String;
	event.text = bytes ? std::string(bytes, [text lengthOfBytesUsingEncoding:NSUTF8StringEncoding])
	                   : std::string("\xFF");
	event.selection_start  = utf8_offset(text, selection.location);
	event.selection_length = utf8_offset(text, NSMaxRange(selection)) - event.selection_start;
	event.modifiers        = self.modifiers;
	if (!ice::platform::platform_events::post(std::move(event)))
		[self cancelPreedit];
}
- (void)cancelPreedit {
	self.marked    = @"";
	self.selection = NSMakeRange(0, 0);
	[self.context discardMarkedText];
}
- (void)unmarkText {
	if (self.marked.length)
		[self insertText:self.marked replacementRange:NSMakeRange(NSNotFound, 0)];
}
- (BOOL)hasMarkedText {
	return self.marked.length != 0;
}
- (NSRange)markedRange {
	return self.marked.length ? NSMakeRange(0, self.marked.length) : NSMakeRange(NSNotFound, 0);
}
- (NSRange)selectedRange {
	return self.selection;
}
- (NSArray<NSAttributedStringKey> *)validAttributesForMarkedText {
	return @[];
}
- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range
                                                actualRange:(NSRangePointer)actual {
	if (range.location > self.marked.length || range.length > self.marked.length - range.location)
		return nil;
	if (actual)
		*actual = range;
	return [[NSAttributedString alloc] initWithString:[self.marked substringWithRange:range]];
}
- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actual {
	if (actual)
		*actual = range;
	const auto rect = self.owner ? self.owner->text_input_area() : ice::platform::text_input_rect{};
	NSRect     local =
	    NSMakeRect(rect.x, self.bounds.size.height - rect.y - rect.height, rect.width, rect.height);
	return [self.window convertRectToScreen:[self convertRect:local toView:nil]];
}
- (NSUInteger)characterIndexForPoint:(NSPoint)point {
	(void)point;
	return NSNotFound;
}
- (void)doCommandBySelector:(SEL)selector {
	if (!self.owner || !self.owner->text_input_enabled() || !self.window.isKeyWindow)
		return;
	using ice::platform::edit_command;
	struct binding {
		const char  *name;
		edit_command command;
	};
	static constexpr binding bindings[] = {
	    {"insertNewline:", edit_command::accept_line},
	    {"insertLineBreak:", edit_command::accept_line},
	    {"cancelOperation:", edit_command::cancel},
	    {"insertTab:", edit_command::tab},
	    {"insertBacktab:", edit_command::tab},
	    {"deleteBackward:", edit_command::backspace},
	    {"deleteForward:", edit_command::delete_forward},
	    {"moveLeft:", edit_command::move_left},
	    {"moveRight:", edit_command::move_right},
	    {"moveUp:", edit_command::move_up},
	    {"moveDown:", edit_command::move_down},
	    {"moveToBeginningOfLine:", edit_command::line_start},
	    {"moveToEndOfLine:", edit_command::line_end},
	    {"moveToLeftEndOfLine:", edit_command::line_start},
	    {"moveToRightEndOfLine:", edit_command::line_end},
	    {"scrollPageUp:", edit_command::page_up},
	    {"scrollPageDown:", edit_command::page_down},
	    {"pageUp:", edit_command::page_up},
	    {"pageDown:", edit_command::page_down},
	    {"moveWordLeft:", edit_command::word_left},
	    {"moveWordRight:", edit_command::word_right},
	    {"deleteWordBackward:", edit_command::delete_word_backward},
	    {"deleteWordForward:", edit_command::delete_word_forward},
	    {"deleteToBeginningOfLine:", edit_command::delete_to_line_start},
	    {"deleteToEndOfLine:", edit_command::delete_to_line_end},
	    {"selectAll:", edit_command::select_all},
	    {"copy:", edit_command::copy},
	    {"cut:", edit_command::cut},
	    {"paste:", edit_command::paste}};
	NSString *name = NSStringFromSelector(selector);
	if ([name containsString:@"AndModifySelection:"]) {
		self.modifiers |= ice::platform::modifier_flag(ice::platform::key_modifier::shift);
		name = [name stringByReplacingOccurrencesOfString:@"AndModifySelection:" withString:@":"];
	}
	for (const auto &binding : bindings)
		if (std::strcmp(name.UTF8String, binding.name) == 0) {
			ice::platform::platform_event event{ice::platform::platform_event_type::edit_command,
			                                    self.owner->id(), 0};
			event.command   = binding.command;
			event.modifiers = self.modifiers;
			event.repeat    = self.repeated;
			if (!ice::platform::platform_events::post(std::move(event)))
				[self cancelPreedit];
			return;
		}
}
- (void)copy:(id)sender {
	(void)sender;
	[self doCommandBySelector:@selector(copy:)];
}
- (void)cut:(id)sender {
	(void)sender;
	[self doCommandBySelector:@selector(cut:)];
}
- (void)paste:(id)sender {
	(void)sender;
	[self doCommandBySelector:@selector(paste:)];
}
- (void)selectAll:(id)sender {
	(void)sender;
	[self doCommandBySelector:@selector(selectAll:)];
}
@end
@interface ICEDesktopDelegate : NSObject <NSWindowDelegate>
@property(nonatomic, assign) ice::platform::desktop_window *owner;
@end
@implementation ICEDesktopDelegate
- (BOOL)windowShouldClose:(NSWindow *)sender {
	(void)sender;
	self.owner->request_close();
	return NO;
}
- (void)windowDidBecomeKey:(NSNotification *)notification {
	(void)notification;
	NSWindow *window =
	    (__bridge NSWindow *)reinterpret_cast<void *>(self.owner->native_handle().window);
	[window makeFirstResponder:window.contentView];
	if (self.owner->text_input_enabled())
		[window.contentView.inputContext activate];
	ice::platform::platform_events::post(ice::platform::platform_event_type::focus_gained,
	                                     self.owner->id());
}
- (void)windowDidResignKey:(NSNotification *)notification {
	(void)notification;
	[self.owner->native_handle().view
	        ? (__bridge ICEDesktopTextView *)self.owner->native_handle().view
	        : nil cancelPreedit];
	ice::platform::platform_events::post(ice::platform::platform_event_type::focus_lost,
	                                     self.owner->id());
}
- (void)windowDidResize:(NSNotification *)notification {
	(void)notification;
	ice::platform::platform_events::post(ice::platform::platform_event_type::window_resized,
	                                     self.owner->id());
}
- (void)windowDidChangeBackingProperties:(NSNotification *)notification {
	[self windowDidResize:notification];
}
@end
@interface ICEDesktopApplication : NSObject <NSApplicationDelegate>
@end
@implementation ICEDesktopApplication
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
	(void)sender;
	for (auto *window : windows)
		window->request_close();
	ice::platform::platform_events::post(
	    ice::platform::platform_event_type::application_quit_requested);
	return NSTerminateCancel;
}
@end
namespace ice::platform::detail {
namespace {
class appkit_window final : public desktop_window {
public:
	explicit appkit_window(NSWindow *native) : native_(native) {
		view_        = [[ICEDesktopTextView alloc] initWithFrame:native.contentView.bounds];
		view_.owner  = this;
		view_.marked = @"";
		view_.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
		view_.wantsLayer       = YES;
		native.contentView     = view_;
		[native makeFirstResponder:view_];
		delegate_        = [ICEDesktopDelegate new];
		delegate_.owner  = this;
		native_.delegate = delegate_;
		windows.push_back(this);
	}
	~appkit_window() override {
		cancel_text_input();
		view_.owner = nullptr;
		std::erase(windows, this);
		native_.delegate = nil;
		[native_ close];
	}
	void show(bool visible) override {
		if (visible)
			[native_ makeKeyAndOrderFront:nil];
		else {
			cancel_text_input();
			[native_ orderOut:nil];
		}
	}
	bool resize(int width, int height) override {
		if (!valid_extent(width, height))
			return false;
		[native_ setContentSize:NSMakeSize(width, height)];
		return true;
	}
	void fullscreen(bool enabled) override {
		if (((native_.styleMask & NSWindowStyleMaskFullScreen) != 0) != enabled)
			[native_ toggleFullScreen:nil];
	}
	window_extent extent() const override {
		NSRect logical = native_.contentView.bounds;
		NSRect pixels  = [native_.contentView convertRectToBacking:logical];
		return {int(logical.size.width), int(logical.size.height), int(pixels.size.width),
		        int(pixels.size.height), float(native_.backingScaleFactor)};
	}
	native_window_handle native_handle() const override {
		return {native_window_kind::appkit,
		        reinterpret_cast<std::uintptr_t>((__bridge void *)native_),
		        (__bridge void *)native_.contentView, nullptr};
	}

private:
	void text_input_changed() override {
		if (text_input_enabled() && native_.isKeyWindow) {
			[native_ makeFirstResponder:view_];
			[view_.inputContext activate];
		} else
			[view_.context deactivate];
		[view_.context invalidateCharacterCoordinates];
	}
	void                         cancel_native_text_input() override { [view_ cancelPreedit]; }
	ICEDesktopTextView *__strong view_;
	NSWindow *__strong           native_;
	ICEDesktopDelegate *__strong delegate_;
};
} // namespace
ice::expected<std::unique_ptr<desktop_window>, window_diagnostic>
create_desktop_window(const desktop_window_options &options) {
	if (![NSThread isMainThread])
		return ice::unexpected(
		    window_diagnostic{"AppKit window creation requires the main thread"});
	@autoreleasepool {
		[NSApplication sharedApplication];
		static ICEDesktopApplication *application_delegate = [ICEDesktopApplication new];
		if (!NSApp.delegate)
			NSApp.delegate = application_delegate;
		[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
		// A manually pumped application still needs AppKit's launch phase for services,
		// responder setup and accessibility registration, without entering a blocking run().
		static bool launch_finished = false;
		if (!launch_finished) {
			if (!NSApp.isRunning)
				[NSApp finishLaunching];
			launch_finished = true;
		}
		if (!NSApp.mainMenu) {
			NSMenu     *menu = [NSMenu new];
			NSMenuItem *item = [NSMenuItem new];
			[menu addItem:item];
			NSMenu *application_menu = [NSMenu new];
			[application_menu addItemWithTitle:@"Quit"
			                            action:@selector(terminate:)
			                     keyEquivalent:@"q"];
			item.submenu   = application_menu;
			NSApp.mainMenu = menu;
		}
		NSString *title = [[NSString alloc] initWithBytes:options.title.data()
		                                           length:options.title.size()
		                                         encoding:NSUTF8StringEncoding];
		if (!title)
			return ice::unexpected(window_diagnostic{"Window title is not UTF-8"});
		NSWindow *native = [[NSWindow alloc]
		    initWithContentRect:NSMakeRect(0, 0, options.width, options.height)
		              styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
		                        NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable
		                backing:NSBackingStoreBuffered
		                  defer:NO];
		if (!native)
			return ice::unexpected(window_diagnostic{"AppKit window creation failed"});
		native.releasedWhenClosed = NO;
		native.title              = title;
		native.backgroundColor    = NSColor.blackColor;
		[native center];
		native.contentView.wantsLayer = YES;
		auto result                   = std::make_unique<appkit_window>(native);
		if (options.visible) {
			result->show(true);
			if (@available(macOS 14.0, *))
				[NSApp activate];
		}
		return std::unique_ptr<desktop_window>(std::move(result));
	}
}
void pump_desktop_events() {
	@autoreleasepool {
		// Cocoa services (including activation, input methods and AX requests) also
		// arrive on run-loop sources rather than NSEvent's queue. Service ready work
		// without taking over the embedding application's frame loop.
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true);
		while (NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
		                                           untilDate:NSDate.distantPast
		                                              inMode:NSDefaultRunLoopMode
		                                             dequeue:YES])
			[NSApp sendEvent:event];
		[NSApp updateWindows];
	}
}
} // namespace ice::platform::detail
