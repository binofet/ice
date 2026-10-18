#include "../desktop_window_internal.h"
#include "../physical_key_internal.h"
#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#include <algorithm>
#include <cstring>
#include <vector>

namespace {
std::vector<ice::platform::desktop_window *> windows;
ice::platform::desktop_window               *relative_owner = nullptr;
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
@interface ICEDesktopTextView : NSView <NSTextInputClient> {
	std::array<bool, 256> physicalPressed_;
	NSPoint               pointerPosition_;
	BOOL                  pointerValid_;
}
@property(nonatomic, assign) ice::platform::desktop_window *owner;
@property(nonatomic, copy) NSString                        *marked;
@property(nonatomic) NSRange                                selection;
@property(nonatomic) ice::platform::key_modifiers           modifiers;
@property(nonatomic) BOOL                                   repeated;
@property(nonatomic, strong) NSTextInputContext            *context;
@property(nonatomic, strong) NSMenu                        *editEquivalents;
@property(nonatomic, strong) NSEvent                       *equivalentEvent;
- (void)cancelPreedit;
- (void)resetPointer;
- (void)cancelPhysical;
- (void)releasePhysicalKeyIfHeld:(NSEvent *)event;
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
	[self.window makeFirstResponder:self];
	[self pointerButton:event pressed:YES];
}
- (BOOL)acceptsFirstMouse:(NSEvent *)event {
	(void)event;
	return YES;
}
- (void)mouseUp:(NSEvent *)event {
	[self pointerButton:event pressed:NO];
}
- (void)rightMouseDown:(NSEvent *)event {
	[self pointerButton:event pressed:YES];
}
- (void)rightMouseUp:(NSEvent *)event {
	[self pointerButton:event pressed:NO];
}
- (void)otherMouseDown:(NSEvent *)event {
	[self pointerButton:event pressed:YES];
}
- (void)otherMouseUp:(NSEvent *)event {
	[self pointerButton:event pressed:NO];
}
- (void)pointerButton:(NSEvent *)native pressed:(BOOL)pressed {
	if (!self.owner || !self.window.isKeyWindow)
		return;
	using namespace ice::platform;
	const mouse_button buttons[]{mouse_button::primary, mouse_button::secondary,
	                             mouse_button::middle, mouse_button::back, mouse_button::forward};
	mouse_button       button = mouse_button::none;
	// Left/right event types carry their identity explicitly. Factory-created
	// NSEvents may leave buttonNumber at zero even for a right-button event.
	if (native.type == NSEventTypeLeftMouseDown || native.type == NSEventTypeLeftMouseUp)
		button = mouse_button::primary;
	else if (native.type == NSEventTypeRightMouseDown || native.type == NSEventTypeRightMouseUp)
		button = mouse_button::secondary;
	else if (native.buttonNumber >= 0 && native.buttonNumber < 5)
		button = buttons[native.buttonNumber];
	if (button == mouse_button::none)
		return;
	NSPoint        point = [self convertPoint:native.locationInWindow fromView:nil];
	platform_event event{platform_event_type::pointer_button, self.owner->id(), 0};
	event.button    = button;
	event.pressed   = pressed;
	event.x         = point.x;
	event.y         = self.bounds.size.height - point.y;
	event.modifiers = modifiers(native.modifierFlags);
	if (!platform_events::post(std::move(event)))
		self.owner->cancel_input();
}
- (void)resetPointer {
	pointerValid_ = NO;
}
- (void)cancelPhysical {
	[self resetPointer];
	physicalPressed_.fill(false);
}
- (void)mouseMoved:(NSEvent *)native {
	if (!self.owner || !self.window.isKeyWindow)
		return;
	using namespace ice::platform;
	NSPoint point = [self convertPoint:native.locationInWindow fromView:nil];
	point.y       = self.bounds.size.height - point.y;
	platform_event event{platform_event_type::pointer_move, self.owner->id(), 0};
	if (self.owner->pointer_mode() == pointer_mode::relative) {
		if (!pointerValid_) {
			pointerPosition_ = point;
			pointerValid_    = YES;
		}
		event.x       = pointerValid_ ? pointerPosition_.x : point.x;
		event.y       = pointerValid_ ? pointerPosition_.y : point.y;
		event.delta_x = native.deltaX;
		event.delta_y = native.deltaY;
	} else {
		event.x = point.x;
		event.y = point.y;
		if (pointerValid_) {
			event.delta_x = point.x - pointerPosition_.x;
			event.delta_y = point.y - pointerPosition_.y;
		}
		pointerPosition_ = point;
		pointerValid_    = YES;
	}
	event.modifiers = modifiers(native.modifierFlags);
	if (!platform_events::post(std::move(event)))
		self.owner->cancel_input();
}
- (void)mouseDragged:(NSEvent *)event {
	[self mouseMoved:event];
}
- (void)rightMouseDragged:(NSEvent *)event {
	[self mouseMoved:event];
}
- (void)otherMouseDragged:(NSEvent *)event {
	[self mouseMoved:event];
}
- (void)scrollWheel:(NSEvent *)native {
	if (!self.owner || !self.window.isKeyWindow)
		return;
	ice::platform::platform_event event{ice::platform::platform_event_type::scroll,
	                                    self.owner->id(), 0};
	// AppKit's positive horizontal wheel delta is leftward; the shared intent is rightward.
	event.scroll_x       = -native.scrollingDeltaX;
	event.scroll_y       = native.scrollingDeltaY;
	event.precise_scroll = native.hasPreciseScrollingDeltas;
	event.modifiers      = modifiers(native.modifierFlags);
	if (!ice::platform::platform_events::post(std::move(event)))
		self.owner->cancel_input();
}
- (void)physicalKey:(NSEvent *)native pressed:(BOOL)pressed {
	if (!self.owner || !self.window.isKeyWindow)
		return;
	auto key = ice::platform::detail::mac_physical_key(native.keyCode);
	if (key == ice::platform::physical_key::unknown)
		return;
	if (native.keyCode < physicalPressed_.size())
		physicalPressed_[native.keyCode] = pressed;
	ice::platform::platform_event event{ice::platform::platform_event_type::physical_key,
	                                    self.owner->id(), 0};
	event.key       = key;
	event.pressed   = pressed;
	event.repeat    = pressed && native.type == NSEventTypeKeyDown && native.isARepeat;
	event.modifiers = modifiers(native.modifierFlags);
	if (!ice::platform::platform_events::post(std::move(event)))
		self.owner->cancel_input();
}
- (void)keyUp:(NSEvent *)native {
	[self physicalKey:native pressed:NO];
}
- (void)releasePhysicalKeyIfHeld:(NSEvent *)native {
	if (native.keyCode < physicalPressed_.size() && physicalPressed_[native.keyCode])
		[self keyUp:native];
}
- (void)flagsChanged:(NSEvent *)native {
	using enum ice::platform::physical_key;
	auto                 key  = ice::platform::detail::mac_physical_key(native.keyCode);
	NSEventModifierFlags mask = 0;
	if (key == left_shift || key == right_shift)
		mask = NSEventModifierFlagShift;
	if (key == left_control || key == right_control)
		mask = NSEventModifierFlagControl;
	if (key == left_alt || key == right_alt)
		mask = NSEventModifierFlagOption;
	if (key == left_super || key == right_super)
		mask = NSEventModifierFlagCommand;
	if (key == caps_lock)
		mask = NSEventModifierFlagCapsLock;
	if (!mask || native.keyCode >= physicalPressed_.size())
		return;
	// AppKit reports Caps Lock's toggle, not a held-key release. Publish a complete
	// physical action; the actual latched state remains in event.modifiers.
	if (key == caps_lock) {
		[self physicalKey:native pressed:YES];
		[self physicalKey:native pressed:NO];
		return;
	}
	BOOL pressed = (native.modifierFlags & mask) != 0;
	if (key != caps_lock && pressed)
		pressed = !physicalPressed_[native.keyCode];
	[self physicalKey:native pressed:pressed];
}
- (void)recordKeyDown:(NSEvent *)event {
	[self physicalKey:event pressed:YES];
	self.modifiers = modifiers(event.modifierFlags);
	self.repeated  = event.isARepeat;
	ice::platform::platform_event activity{ice::platform::platform_event_type::input_activity,
	                                       self.owner->id(), 0};
	activity.modifiers = self.modifiers;
	activity.repeat    = self.repeated;
	if (!ice::platform::platform_events::post(std::move(activity)))
		[self cancelPreedit];
}
- (void)keyDown:(NSEvent *)event {
	if (!self.owner || !self.window.isKeyWindow)
		return;
	[self recordKeyDown:event];
	if (self.owner->text_input_enabled())
		[self interpretKeyEvents:@[ event ]];
}
- (BOOL)performKeyEquivalent:(NSEvent *)event {
	if (event.type != NSEventTypeKeyDown || !self.owner || !self.window.isKeyWindow ||
	    self.window.firstResponder != self || !self.owner->text_input_enabled())
		return [super performKeyEquivalent:event];
	if (!self.editEquivalents) {
		NSMenu *menu                     = [[NSMenu alloc] initWithTitle:@"ICE text editing"];
		menu.autoenablesItems            = NO;
		NSArray<NSString *> *titles      = @[ @"Cut", @"Copy", @"Paste", @"Select All" ];
		NSArray<NSString *> *keys        = @[ @"x", @"c", @"v", @"a" ];
		const SEL            selectors[] = {@selector(cut:), @selector(copy:), @selector(paste:),
		                                    @selector(selectAll:)};
		for (NSUInteger index = 0; index < titles.count; ++index) {
			NSMenuItem *item               = [menu addItemWithTitle:titles[index]
                                               action:selectors[index]
                                        keyEquivalent:keys[index]];
			item.target                    = self;
			item.keyEquivalentModifierMask = NSEventModifierFlagCommand;
		}
		self.editEquivalents = menu;
	}
	// AppKit performs character/layout/modifier matching; physical key codes do not
	// choose text actions. A handled equivalent bypasses keyDown:, so its action
	// records the same physical/activity stream before posting exactly one edit.
	NSEvent *previous    = self.equivalentEvent;
	self.equivalentEvent = event;
	BOOL handled         = NO;
	@try {
		handled = [self.editEquivalents performKeyEquivalent:event];
	} @finally {
		self.equivalentEvent = previous;
	}
	return handled ? YES : [super performKeyEquivalent:event];
}
- (void)editAction:(SEL)selector {
	if (self.equivalentEvent)
		[self recordKeyDown:self.equivalentEvent];
	[self doCommandBySelector:selector];
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
	[self editAction:@selector(copy:)];
}
- (void)cut:(id)sender {
	(void)sender;
	[self editAction:@selector(cut:)];
}
- (void)paste:(id)sender {
	(void)sender;
	[self editAction:@selector(paste:)];
}
- (void)selectAll:(id)sender {
	(void)sender;
	[self editAction:@selector(selectAll:)];
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
	self.owner->cancel_input();
	[(__bridge ICEDesktopTextView *)self.owner->native_handle().view cancelPhysical];
	ice::platform::platform_events::post(ice::platform::platform_event_type::focus_lost,
	                                     self.owner->id());
}
- (void)windowDidMiniaturize:(NSNotification *)notification {
	(void)notification;
	self.owner->cancel_input();
}
- (void)invalidateTextCoordinates {
	ICEDesktopTextView *view = (__bridge ICEDesktopTextView *)self.owner->native_handle().view;
	[view.context invalidateCharacterCoordinates];
}
- (void)windowDidMove:(NSNotification *)notification {
	(void)notification;
	[self invalidateTextCoordinates];
}
- (void)windowDidResize:(NSNotification *)notification {
	(void)notification;
	[self invalidateTextCoordinates];
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
		view_.autoresizingMask         = NSViewWidthSizable | NSViewHeightSizable;
		view_.wantsLayer               = YES;
		native.contentView             = view_;
		native.acceptsMouseMovedEvents = YES;
		[native makeFirstResponder:view_];
		delegate_        = [ICEDesktopDelegate new];
		delegate_.owner  = this;
		native_.delegate = delegate_;
		windows.push_back(this);
	}
	~appkit_window() override {
		cancel_input();
		view_.owner = nullptr;
		std::erase(windows, this);
		native_.delegate = nil;
		[native_ close];
	}
	ice::expected<std::string, window_diagnostic>
	read_native_clipboard_text(std::size_t maximum_bytes) override {
		@autoreleasepool {
			NSPasteboard *board = NSPasteboard.generalPasteboard;
			NSString     *value = [board stringForType:NSPasteboardTypeString];
			if (!value)
				return std::string{};
			const NSUInteger count = [value lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
			if (count > maximum_bytes)
				return ice::unexpected(window_diagnostic{"Clipboard text exceeds its byte limit"});
			NSData *bytes = [value dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:NO];
			if (!bytes)
				return ice::unexpected(
				    window_diagnostic{"Clipboard text cannot be converted to UTF-8"});
			return bytes.length ? std::string(static_cast<const char *>(bytes.bytes), bytes.length)
			                    : std::string{};
		}
	}
	ice::expected<void, window_diagnostic>
	write_native_clipboard_text(std::string_view text) override {
		@autoreleasepool {
			NSString *value = [[NSString alloc] initWithBytes:text.data()
			                                           length:text.size()
			                                         encoding:NSUTF8StringEncoding];
			if (!value)
				return ice::unexpected(
				    window_diagnostic{"Clipboard text cannot be converted from UTF-8"});
			NSPasteboardItem *item = [NSPasteboardItem new];
			if (![item setString:value forType:NSPasteboardTypeString])
				return ice::unexpected(window_diagnostic{"Cannot prepare clipboard text"});
			NSPasteboard *board = NSPasteboard.generalPasteboard;
			[board clearContents];
			if (![board writeObjects:@[ item ]])
				return ice::unexpected(window_diagnostic{"Cannot publish clipboard text"});
			return {};
		}
	}
	void show(bool visible) override {
		if (visible)
			[native_ makeKeyAndOrderFront:nil];
		else {
			cancel_input();
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
	relative_pointer_kind relative_pointer_capability() const override {
		return relative_pointer_kind::native;
	}

private:
	ice::expected<void, window_diagnostic>
	change_pointer_mode(ice::platform::pointer_mode mode) override {
		if (mode == ice::platform::pointer_mode::normal) {
			if (relative_owner == this) {
				CGAssociateMouseAndMouseCursorPosition(true);
				[NSCursor unhide];
				relative_owner = nullptr;
				[view_ resetPointer];
			}
			return {};
		}
		if (!native_.isKeyWindow || !native_.isVisible || native_.isMiniaturized ||
		    (relative_owner && relative_owner != this))
			return ice::unexpected(window_diagnostic{
			    "Relative pointer capture requires the focused visible AppKit window"});
		if (CGAssociateMouseAndMouseCursorPosition(false) != kCGErrorSuccess)
			return ice::unexpected(
			    window_diagnostic{"AppKit could not enable native relative pointer input"});
		[NSCursor hide];
		relative_owner = this;
		[view_ resetPointer];
		return {};
	}
	void text_input_changed() override {
		if (text_input_enabled() && native_.isKeyWindow) {
			[native_ makeFirstResponder:view_];
			[view_.inputContext activate];
		} else
			[view_.context deactivate];
		text_input_rect_changed();
	}
	void text_input_rect_changed() override { [view_.context invalidateCharacterCoordinates]; }
	void cancel_native_text_input() override { [view_ cancelPreedit]; }
	void cancel_native_physical_input() override { [view_ cancelPhysical]; }
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
		                                             dequeue:YES]) {
			[NSApp sendEvent:event];
			// AppKit can consume Command-key releases before the view's keyUp:.
			// Release only a still-held key after normal dispatch, so a delivered
			// keyUp: is never duplicated. Focus loss/cancellation already clears it.
			if (event.type == NSEventTypeKeyUp &&
			    [event.window.contentView isKindOfClass:ICEDesktopTextView.class])
				[(ICEDesktopTextView *)event.window.contentView releasePhysicalKeyIfHeld:event];
		}
		[NSApp updateWindows];
	}
}
} // namespace ice::platform::detail
