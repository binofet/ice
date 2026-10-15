#include "../desktop_window_internal.h"
#import <AppKit/AppKit.h>
#include <algorithm>
#include <vector>

namespace {
std::vector<ice::platform::desktop_window *> windows;
}
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
	ice::platform::platform_events::post(ice::platform::platform_event_type::focus_gained,
	                                     self.owner->id());
}
- (void)windowDidResignKey:(NSNotification *)notification {
	(void)notification;
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
		delegate_        = [ICEDesktopDelegate new];
		delegate_.owner  = this;
		native_.delegate = delegate_;
		windows.push_back(this);
	}
	~appkit_window() override {
		std::erase(windows, this);
		native_.delegate = nil;
		[native_ close];
	}
	void show(bool visible) override {
		if (visible)
			[native_ makeKeyAndOrderFront:nil];
		else
			[native_ orderOut:nil];
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
		while (NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
		                                           untilDate:NSDate.distantPast
		                                              inMode:NSDefaultRunLoopMode
		                                             dequeue:YES])
			[NSApp sendEvent:event];
		[NSApp updateWindows];
	}
}
} // namespace ice::platform::detail
