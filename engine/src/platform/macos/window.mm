#include "ice/platform/macos/window.h"
#include "ice/input/input.h"
#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#import <OpenGL/OpenGL.h>
#include <algorithm>
#include <vector>

struct ice::platform::macos::window_state {
	NSWindow        *window   = nil;
	NSView          *view     = nil;
	NSObject        *delegate = nil;
	NSOpenGLContext *context  = nil;
	NSRect           windowed_frame;
};
static std::vector<ice::platform::macos::window *> windows;
static bool                                        capture_requested = false, cursor_hidden = false;
static ice::platform::macos::window               *focused_window() {
    for (auto window : windows)
        if ((NSWindow *)window->get_ref() == [NSApp keyWindow])
            return window;
    return nullptr;
}
static void sync_cursor() {
	bool capture = capture_requested && [NSApp isActive] && focused_window();
	if (capture == cursor_hidden)
		return;
	if (capture) {
		[NSCursor hide];
		CGAssociateMouseAndMouseCursorPosition(false);
	} else {
		CGAssociateMouseAndMouseCursorPosition(true);
		[NSCursor unhide];
	}
	cursor_hidden = capture;
}
static int key_for_event(NSEvent *event) {
	NSString *text = [event charactersIgnoringModifiers];
	if (![text length])
		return -1;
	unichar c = [text characterAtIndex:0];
	if (c >= 'a' && c <= 'z')
		return ice::input::key_a + c - 'a';
	if (c >= 'A' && c <= 'Z')
		return ice::input::key_a + c - 'A';
	if (c >= '0' && c <= '9')
		return ice::input::key_0 + c - '0';
	if (c >= NSF1FunctionKey && c <= NSF12FunctionKey)
		return ice::input::key_f1 + c - NSF1FunctionKey;
	switch (c) {
	case 27:
		return ice::input::key_escape;
	case '\r':
	case NSEnterCharacter:
		return ice::input::key_return;
	case ' ':
		return ice::input::key_space;
	case '\t':
		return ice::input::key_tab;
	case NSDeleteCharacter:
		return ice::input::key_back;
	case NSLeftArrowFunctionKey:
		return ice::input::key_left;
	case NSRightArrowFunctionKey:
		return ice::input::key_right;
	case NSUpArrowFunctionKey:
		return ice::input::key_up;
	case NSDownArrowFunctionKey:
		return ice::input::key_down;
	case NSHomeFunctionKey:
		return ice::input::key_home;
	case NSEndFunctionKey:
		return ice::input::key_end;
	case NSPageUpFunctionKey:
		return ice::input::key_pg_up;
	case NSPageDownFunctionKey:
		return ice::input::key_pg_dn;
	case NSDeleteFunctionKey:
		return ice::input::key_delete;
	default:
		return -1;
	}
}
@interface ICEView : NSView {
@public
	ice::platform::macos::window *owner;
}
@end
@implementation ICEView
- (BOOL)acceptsFirstResponder {
	return YES;
}
- (BOOL)acceptsFirstMouse:(NSEvent *)event {
	return YES;
}
- (void)keyDown:(NSEvent *)event {
	int key = key_for_event(event);
	if (key >= 0)
		ice::input::input::instance()->set_down(key, true);
}
- (void)keyUp:(NSEvent *)event {
	int key = key_for_event(event);
	if (key >= 0)
		ice::input::input::instance()->set_down(key, false);
}
- (void)flagsChanged:(NSEvent *)event {
	auto input = ice::input::input::instance();
	auto flags = [event modifierFlags];
	input->set_down(ice::input::key_shift, (flags & NSEventModifierFlagShift) != 0);
	input->set_down(ice::input::key_control, (flags & NSEventModifierFlagControl) != 0);
	input->set_down(ice::input::key_menu, (flags & NSEventModifierFlagOption) != 0);
}
- (void)move:(NSEvent *)event {
	if (!owner)
		return;
	NSPoint point  = [self convertPoint:[event locationInWindow] fromView:nil];
	int     height = (int)[self bounds].size.height;
	auto    input  = ice::input::input::instance();
	if (cursor_hidden)
		input->add_mouse_delta([event deltaX], [event deltaY], height);
	else
		input->update_mouse_position(point.x, height - point.y, height);
}
- (void)mouseMoved:(NSEvent *)event {
	[self move:event];
}
- (void)mouseDragged:(NSEvent *)event {
	[self move:event];
}
- (void)rightMouseDragged:(NSEvent *)event {
	[self move:event];
}
- (void)otherMouseDragged:(NSEvent *)event {
	[self move:event];
}
- (void)mouseDown:(NSEvent *)event {
	[self move:event];
	ice::input::input::instance()->set_down(ice::input::mouse_l, true);
}
- (void)mouseUp:(NSEvent *)event {
	[self move:event];
	ice::input::input::instance()->set_down(ice::input::mouse_l, false);
}
- (void)rightMouseDown:(NSEvent *)event {
	ice::input::input::instance()->set_down(ice::input::mouse_r, true);
}
- (void)rightMouseUp:(NSEvent *)event {
	ice::input::input::instance()->set_down(ice::input::mouse_r, false);
}
- (void)otherMouseDown:(NSEvent *)event {
	ice::input::input::instance()->set_down(ice::input::mouse_m, true);
}
- (void)otherMouseUp:(NSEvent *)event {
	ice::input::input::instance()->set_down(ice::input::mouse_m, false);
}
- (void)scrollWheel:(NSEvent *)event {
	ice::input::input::instance()->add_scroll([event scrollingDeltaY]);
}
@end
@interface ICEWindowDelegate : NSObject <NSWindowDelegate> {
@public
	ice::platform::macos::window *owner;
}
@end
@implementation ICEWindowDelegate
- (BOOL)windowShouldClose:(NSWindow *)sender {
	if (owner)
		owner->request_close();
	return NO;
}
- (void)windowDidResignKey:(NSNotification *)notification {
	if (owner)
		ice::platform::platform_events::post(ice::platform::platform_event_type::focus_lost,
		                                     owner->get_id());
	ice::platform::macos::window::focus_changed();
}
- (void)windowDidBecomeKey:(NSNotification *)notification {
	if (owner)
		ice::platform::platform_events::post(ice::platform::platform_event_type::focus_gained,
		                                     owner->get_id());
	ice::platform::macos::window::focus_changed();
}
@end
@interface ICEApplicationDelegate : NSObject <NSApplicationDelegate>
@end
@implementation ICEApplicationDelegate
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
	ice::platform::platform_events::post(
	    ice::platform::platform_event_type::application_quit_requested);
	for (auto window : windows)
		window->request_close();
	return NSTerminateCancel;
}
- (void)applicationDidResignActive:(NSNotification *)notification {
	ice::platform::macos::window::focus_changed();
}
- (void)applicationDidBecomeActive:(NSNotification *)notification {
	ice::platform::macos::window::focus_changed();
}
@end
static void initialize_application() {
	static bool initialized = false;
	if (initialized)
		return;
	[NSApplication sharedApplication];
	[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
	static ICEApplicationDelegate *delegate = [[ICEApplicationDelegate alloc] init];
	[NSApp setDelegate:delegate];
	NSMenu     *bar         = [[NSMenu alloc] initWithTitle:@""];
	NSMenuItem *application = [[NSMenuItem alloc] initWithTitle:@"ICE"
	                                                     action:nil
	                                              keyEquivalent:@""];
	NSMenu     *menu        = [[NSMenu alloc] initWithTitle:@"ICE"];
	[menu addItemWithTitle:@"Hide ICE" action:@selector(hide:) keyEquivalent:@"h"];
	[menu addItem:[NSMenuItem separatorItem]];
	[menu addItemWithTitle:@"Quit ICE" action:@selector(terminate:) keyEquivalent:@"q"];
	[application setSubmenu:menu];
	[bar addItem:application];
	[NSApp setMainMenu:bar];
	[menu release];
	[application release];
	[bar release];
	[NSApp finishLaunching];
	initialized = true;
}
ice::platform::macos::window::window(const ice::platform::window_def &def,
                                     ice::platform::window           *parent)
    : ice::platform::window(def, parent) {
	@autoreleasepool {
		if (![NSThread isMainThread] || def.width <= 0 || def.height <= 0)
			return;
		initialize_application();
		native_          = new ice::platform::macos::window_state;
		NSUInteger style = def.win_flags
		                       ? def.win_flags
		                       : (NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
		                          NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable);
		NSRect     rect  = NSMakeRect(def.pos_x, def.pos_y, def.width, def.height);
		native_->window  = [[NSWindow alloc] initWithContentRect:rect
                                                      styleMask:style
                                                        backing:NSBackingStoreBuffered
                                                          defer:NO];
		[native_->window setReleasedWhenClosed:NO];
		ICEView *view = [[ICEView alloc] initWithFrame:NSMakeRect(0, 0, def.width, def.height)];
		view->owner   = this;
		[view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
		[view setWantsBestResolutionOpenGLSurface:NO];
		native_->view = view;
		[native_->window setContentView:view];
		[view release];
		ICEWindowDelegate *delegate = [[ICEWindowDelegate alloc] init];
		delegate->owner             = this;
		native_->delegate           = delegate;
		[native_->window setDelegate:delegate];
		[native_->window setAcceptsMouseMovedEvents:YES];
		[native_->window makeFirstResponder:view];
		if (parent)
			if (auto native = dynamic_cast<ice::platform::macos::window *>(parent))
				[(NSWindow *)native->get_ref() addChildWindow:native_->window
				                                      ordered:NSWindowAbove];
		windows.push_back(this);
		update_params(def);
		if (!def.pos_x && !def.pos_y)
			[native_->window center];
	}
}
ice::platform::macos::window::~window() { cleanup(); }
ice::result ice::platform::macos::window::cleanup() {
	if (!native_)
		return IC_OK;
	@autoreleasepool {
		windows.erase(std::remove(windows.begin(), windows.end(), this), windows.end());
		sync_cursor();
		destroy_gl_context();
		((ICEView *)native_->view)->owner               = nullptr;
		((ICEWindowDelegate *)native_->delegate)->owner = nullptr;
		[native_->window setDelegate:nil];
		if ([native_->window parentWindow])
			[[native_->window parentWindow] removeChildWindow:native_->window];
		[native_->window orderOut:nil];
		[native_->window close];
		[native_->window release];
		[native_->delegate release];
		delete native_;
		native_ = nullptr;
	}
	return IC_OK;
}
void *ice::platform::macos::window::get_ref() const { return native_ ? native_->window : nil; }
void *ice::platform::macos::window::get_view() const { return native_ ? native_->view : nil; }
ice::result ice::platform::macos::window::show() {
	if (!native_)
		return IC_FAIL_CREATE_WIND;
	@autoreleasepool {
		[native_->window makeKeyAndOrderFront:nil];
		[NSApp activateIgnoringOtherApps:YES];
		sync_cursor();
	}
	return IC_OK;
}
ice::result ice::platform::macos::window::hide() {
	if (!native_)
		return IC_FAIL_CREATE_WIND;
	@autoreleasepool {
		[native_->window orderOut:nil];
		sync_cursor();
	}
	return IC_OK;
}
ice::result ice::platform::macos::window::update_params(const ice::platform::window_def &win_def) {
	if (!native_ || win_def.width <= 0 || win_def.height <= 0)
		return IC_FAIL_GEN;
	@autoreleasepool {
		NSString *title =
		    [NSString stringWithUTF8String:win_def.window_name ? win_def.window_name : "ICE"];
		[native_->window setTitle:title];
		if (win_def.full_screen) {
			if (!window_info_.full_screen || !native_->windowed_frame.size.width)
				native_->windowed_frame = [native_->window frame];
			[native_->window setStyleMask:NSWindowStyleMaskBorderless];
			[native_->window setFrame:[[native_->window screen] frame] display:YES];
		} else {
			[native_->window setStyleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
			                              NSWindowStyleMaskMiniaturizable |
			                              NSWindowStyleMaskResizable];
			if (window_info_.full_screen)
				[native_->window setFrame:native_->windowed_frame display:YES];
			[native_->window setContentSize:NSMakeSize(win_def.width, win_def.height)];
		}
		window_info_ = win_def;
		if (native_->context)
			[native_->context update];
	}
	return IC_OK;
}
ice::result ice::platform::macos::window::get_window_pos(ice::math::ivec2 *pos) {
	if (!native_ || !pos)
		return IC_FAIL_NPE;
	NSRect frame = [native_->window frame];
	pos->x       = (int)frame.origin.x;
	pos->y       = (int)(NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]) - NSMaxY(frame));
	return IC_OK;
}
void ice::platform::macos::window::get_client_size(int *width, int *height) const {
	NSSize size = native_ ? [native_->view bounds].size : NSZeroSize;
	if (width)
		*width = (int)size.width;
	if (height)
		*height = (int)size.height;
}
void ice::platform::macos::window::get_framebuffer_size(int *width, int *height) const {
	get_client_size(width, height);
}
void *ice::platform::macos::window::create_gl_context(void *shared_context) {
	if (!native_)
		return nullptr;
	if (native_->context)
		return native_->context;
	@autoreleasepool {
		NSOpenGLPixelFormatAttribute attributes[] = {NSOpenGLPFAOpenGLProfile,
		                                             NSOpenGLProfileVersionLegacy,
		                                             NSOpenGLPFADoubleBuffer,
		                                             NSOpenGLPFAColorSize,
		                                             24,
		                                             NSOpenGLPFAAlphaSize,
		                                             8,
		                                             NSOpenGLPFADepthSize,
		                                             24,
		                                             0};
		NSOpenGLPixelFormat *format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
		if (!format)
			return nullptr;
		native_->context =
		    [[NSOpenGLContext alloc] initWithFormat:format
		                               shareContext:(NSOpenGLContext *)shared_context];
		[format release];
		[native_->context setView:native_->view];
		[native_->context makeCurrentContext];
		GLint interval = 1;
		[native_->context setValues:&interval forParameter:NSOpenGLContextParameterSwapInterval];
		return native_->context;
	}
}
void ice::platform::macos::window::destroy_gl_context() {
	if (!native_ || !native_->context)
		return;
	if ([NSOpenGLContext currentContext] == native_->context)
		[NSOpenGLContext clearCurrentContext];
	[native_->context clearDrawable];
	[native_->context release];
	native_->context = nil;
}
void ice::platform::macos::window::make_current() {
	if (native_ && native_->context) {
		[native_->context makeCurrentContext];
		[native_->context update];
	}
}
void ice::platform::macos::window::swap_buffers() {
	if (native_ && native_->context)
		[native_->context flushBuffer];
}
void ice::platform::macos::window::pump_events() {
	if (!NSApp)
		return;
	@autoreleasepool {
		NSEvent *event;
		while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
		                                   untilDate:[NSDate distantPast]
		                                      inMode:NSDefaultRunLoopMode
		                                     dequeue:YES])) {
			bool handled = false;
			for (auto window : windows)
				if ((NSWindow *)window->get_ref() == [event window]) {
					handled = window->handle_native_event(event);
					break;
				}
			if (!handled)
				[NSApp sendEvent:event];
		}
		[NSApp updateWindows];
		sync_cursor();
	}
}
void ice::platform::macos::window::focus_changed() {
	auto input = ice::input::input::instance();
	input->cancel_held_input();
	sync_cursor();
}
void ice::platform::macos::window::set_cursor_captured(bool captured) {
	capture_requested = captured;
	sync_cursor();
}
void ice::platform::macos::window::warp_cursor(short x, short y) {
	auto window = focused_window();
	if (!window)
		return;
	NSWindow *native = (NSWindow *)window->get_ref();
	NSView   *view   = (NSView *)window->get_view();
	NSPoint   point  = [view convertPoint:NSMakePoint(x, [view bounds].size.height - y) toView:nil];
	point            = [native convertPointToScreen:point];
	CGFloat top      = NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]);
	CGWarpMouseCursorPosition(CGPointMake(point.x, top - point.y));
	ice::input::input::instance()->reset_mouse_tracking();
}
