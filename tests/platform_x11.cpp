#include "ice/input/input.h"
#include "ice/platform/x11/window.h"
#include "ice/render/gl/api.h"
#include <X11/keysym.h>
#include <cstdio>
#define CHECK(x)                                                                                   \
	do {                                                                                           \
		if (!(x)) {                                                                                \
			std::fprintf(stderr, "X11 smoke failed line %d: %s\n", __LINE__, #x);                  \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)
static bool draw(ice::platform::x11::window &window, float red, float green, float blue) {
	window.make_current();
	int width_, height_;
	window.get_framebuffer_size(&width_, &height_);
	glViewport(0, 0, width_, height_);
	glClearColor(red, green, blue, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	unsigned char pixel[4] = {};
	glReadBuffer(GL_BACK);
	glReadPixels(width_ / 2, height_ / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
	bool correct = abs(pixel[0] - (int)(red * 255)) < 3 && abs(pixel[1] - (int)(green * 255)) < 3 &&
	               abs(pixel[2] - (int)(blue * 255)) < 3;
	window.swap_buffers();
	return correct && glGetError() == GL_NO_ERROR;
}
int main() {
	ice::platform::window_def def = {};
	def.width                     = 320;
	def.height                    = 240;
	def.window_name               = "ICE native main";
	ice::platform::x11::window main(def);
	CHECK(main.get_ref());
	CHECK(main.create_gl_context());
	CHECK(!ICEFAIL(main.show()));
	def.width       = 200;
	def.height      = 160;
	def.window_name = "ICE native preview";
	ice::platform::x11::window preview(def, &main);
	CHECK(preview.get_ref());
	CHECK(preview.create_gl_context());
	CHECK(!ICEFAIL(preview.show()));
	CHECK(main.get_id() != preview.get_id() && main.get_id() != 0);
	XSync(main.get_display(), False);
	ice::platform::window::pump_events();
	::Window parent = 0;
	CHECK(XGetTransientForHint(main.get_display(), preview.get_ref(), &parent) &&
	      parent == main.get_ref());
	CHECK(draw(main, 1, 0, 0));
	CHECK(draw(preview, 0, 1, 0));
	CHECK(draw(main, 0, 0, 1));
	def.width       = 260;
	def.height      = 180;
	def.window_name = "ICE resized preview";
	CHECK(!ICEFAIL(preview.update_params(def)));
	ice::platform::window::pump_events();
	int width_, height_;
	preview.get_client_size(&width_, &height_);
	CHECK(width_ == 260 && height_ == 180);
	CHECK(draw(preview, 1, 1, 0));
	def.full_screen = true;
	CHECK(!ICEFAIL(preview.update_params(def)));
	ice::platform::window::pump_events();
	preview.get_client_size(&width_, &height_);
	CHECK(width_ > 0 && height_ > 0);
	def.full_screen = false;
	CHECK(!ICEFAIL(preview.update_params(def)));
	ice::platform::window::pump_events();
	preview.get_client_size(&width_, &height_);
	CHECK(width_ == 260 && height_ == 180);
	XEvent event       = {};
	event.xkey.type    = KeyPress;
	event.xkey.display = main.get_display();
	event.xkey.window  = preview.get_ref();
	event.xkey.keycode = XKeysymToKeycode(main.get_display(), XK_a);
	XSendEvent(main.get_display(), preview.get_ref(), False, KeyPressMask, &event);
	XSync(main.get_display(), False);
	ice::platform::window::pump_events();
	CHECK(ice::input::input::instance()->is_down(ice::input::key_a));
	ice::input::input::instance()->update();
	event.xkey.type = KeyRelease;
	XSendEvent(main.get_display(), preview.get_ref(), False, KeyReleaseMask, &event);
	XSync(main.get_display(), False);
	ice::platform::window::pump_events();
	CHECK(ice::input::input::instance()->is_pressed(ice::input::key_a));
	event                      = {};
	event.xclient.type         = ClientMessage;
	event.xclient.window       = preview.get_ref();
	event.xclient.message_type = XInternAtom(main.get_display(), "WM_PROTOCOLS", False);
	event.xclient.format       = 32;
	event.xclient.data.l[0]    = XInternAtom(main.get_display(), "WM_DELETE_WINDOW", False);
	XSendEvent(main.get_display(), preview.get_ref(), False, NoEventMask, &event);
	XSync(main.get_display(), False);
	ice::platform::window::pump_events();
	CHECK(preview.should_close());
	CHECK(!main.should_close());
	preview.cleanup();
	preview.cleanup();
	CHECK(draw(main, 0, 1, 1));
	main.cleanup();
	main.cleanup();
	std::puts("X11 platform checks passed: independent windows/contexts, transient owner, resize, "
	          "key events, close and cleanup");
	return 0;
}
