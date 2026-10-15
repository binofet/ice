#include "ice/input/input.h"
#include "ice/platform/macos/window.h"
#include "ice/render/gl/api.h"
#import <AppKit/AppKit.h>
#include <cstdio>
#define CHECK(x)                                                                                   \
	do {                                                                                           \
		if (!(x)) {                                                                                \
			std::fprintf(stderr, "Platform smoke failed line %d: %s\n", __LINE__, #x);             \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)
static bool draw(ice::platform::macos::window &window, float red, float green, float blue) {
	window.make_current();
	int width, height;
	window.get_framebuffer_size(&width, &height);
	glViewport(0, 0, width, height);
	glClearColor(red, green, blue, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	unsigned char pixel[4] = {};
	glReadBuffer(GL_BACK);
	glReadPixels(width / 2, height / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
	bool correct = abs(pixel[0] - (int)(red * 255)) < 3 && abs(pixel[1] - (int)(green * 255)) < 3 &&
	               abs(pixel[2] - (int)(blue * 255)) < 3;
	window.swap_buffers();
	return correct && glGetError() == GL_NO_ERROR;
}
int main() {
	@autoreleasepool {
		ice::platform::window_def def = {};
		def.width                     = 320;
		def.height                    = 240;
		def.window_name               = "ICE native main";
		ice::platform::macos::window main(def);
		CHECK(main.get_ref());
		CHECK(main.create_gl_context());
		CHECK(!ICEFAIL(main.show()));
		def.width       = 200;
		def.height      = 160;
		def.window_name = "ICE native preview";
		ice::platform::macos::window preview(def, &main);
		CHECK(preview.get_ref());
		CHECK(preview.create_gl_context());
		CHECK(!ICEFAIL(preview.show()));
		CHECK(main.get_id() != preview.get_id() && main.get_id() != 0);
		ice::platform::window::pump_events();
		CHECK([(NSWindow *)preview.get_ref() parentWindow] == (NSWindow *)main.get_ref());
		CHECK(draw(main, 1, 0, 0));
		CHECK(draw(preview, 0, 1, 0));
		CHECK(draw(main, 0, 0, 1));
		def.width       = 260;
		def.height      = 180;
		def.window_name = "ICE resized preview";
		CHECK(!ICEFAIL(preview.update_params(def)));
		ice::platform::window::pump_events();
		int width, height;
		preview.get_client_size(&width, &height);
		CHECK(width == 260 && height == 180);
		CHECK([[(NSWindow *)preview.get_ref() title] isEqualToString:@"ICE resized preview"]);
		CHECK(draw(preview, 1, 1, 0));
		def.full_screen = true;
		CHECK(!ICEFAIL(preview.update_params(def)));
		ice::platform::window::pump_events();
		preview.get_client_size(&width, &height);
		CHECK(width > 0 && height > 0);
		def.full_screen = false;
		CHECK(!ICEFAIL(preview.update_params(def)));
		ice::platform::window::pump_events();
		preview.get_client_size(&width, &height);
		CHECK(width == 260 && height == 180);
		// Exercise the view's native event handlers using local test events.
		NSView  *view = (NSView *)preview.get_view();
		NSEvent *key  = [NSEvent keyEventWithType:NSEventTypeKeyDown
                                        location:NSZeroPoint
                                   modifierFlags:0
                                       timestamp:0
                                    windowNumber:[(NSWindow *)preview.get_ref() windowNumber]
                                         context:nil
                                      characters:@"a"
                     charactersIgnoringModifiers:@"a"
                                       isARepeat:NO
                                         keyCode:0];
		[view keyDown:key];
		CHECK(ice::input::input::instance()->is_down(ice::input::key_a));
		ice::input::input::instance()->update();
		key = [NSEvent keyEventWithType:NSEventTypeKeyUp
		                       location:NSZeroPoint
		                  modifierFlags:0
		                      timestamp:0
		                   windowNumber:[(NSWindow *)preview.get_ref() windowNumber]
		                        context:nil
		                     characters:@"a"
		    charactersIgnoringModifiers:@"a"
		                      isARepeat:NO
		                        keyCode:0];
		[view keyUp:key];
		CHECK(ice::input::input::instance()->is_pressed(ice::input::key_a));
		ice::input::input::instance()->set_down(ice::input::key_left, true);
		ice::platform::macos::window::focus_changed();
		CHECK(!ice::input::input::instance()->is_down(ice::input::key_left));
		[(NSWindow *)preview.get_ref() performClose:nil];
		CHECK(preview.should_close());
		CHECK(!main.should_close());
		preview.cleanup();
		preview.cleanup();
		CHECK(draw(main, 0, 1, 1));
		main.request_close();
		CHECK(main.should_close());
		main.cleanup();
		main.cleanup();
		std::puts("Native platform checks passed: independent windows/contexts, parent, "
		          "resize/title, key/focus events, close and cleanup");
	}
	return 0;
}
