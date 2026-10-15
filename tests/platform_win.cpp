#include "ice/input/input.h"
#include "ice/platform/win32/window.h"
#include "ice/render/gl/graphics_device.h"
#include <cstdio>
#define CHECK(x)                                                                                   \
	do {                                                                                           \
		if (!(x)) {                                                                                \
			std::fprintf(stderr, "Win32 smoke failed line %d: %s\n", __LINE__, #x);                \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)
static bool draw(ice::render::gl::graphics_device &device, float red, float green, float blue) {
	device.frame_begin();
	glClearColor(red, green, blue, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	unsigned char pixel[4] = {};
	glReadBuffer(GL_BACK);
	glReadPixels(10, 10, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
	bool correct = abs(pixel[0] - (int)(red * 255)) < 3 && abs(pixel[1] - (int)(green * 255)) < 3 &&
	               abs(pixel[2] - (int)(blue * 255)) < 3;
	device.frame_end();
	return correct && glGetError() == GL_NO_ERROR;
}
int main() {
	ice::platform::window_def def = {};
	def.width                     = 320;
	def.height                    = 240;
	def.window_name               = "ICE native main";
	ice::platform::win32::window main(def, nullptr);
	CHECK(main.get_hwnd());
	CHECK(!ICEFAIL(main.show()));
	ice::render::gl::graphics_device first;
	CHECK(!ICEFAIL(first.init(&main)));
	def.width       = 200;
	def.height      = 160;
	def.window_name = "ICE native preview";
	ice::platform::win32::window preview(def, &main);
	CHECK(preview.get_hwnd());
	CHECK(!ICEFAIL(preview.show()));
	CHECK(main.get_id() != preview.get_id() && main.get_id() != 0);
	ice::render::gl::graphics_device second;
	CHECK(!ICEFAIL(second.init(&preview)));
	ice::platform::window::pump_events();
	CHECK(GetWindow(preview.get_hwnd(), GW_OWNER) == main.get_hwnd());
	CHECK(draw(first, 1, 0, 0));
	CHECK(draw(second, 0, 1, 0));
	CHECK(draw(first, 0, 0, 1));
	def.width  = 260;
	def.height = 180;
	CHECK(!ICEFAIL(preview.update_params(def)));
	int width_, height_;
	preview.get_client_size(&width_, &height_);
	CHECK(width_ == 260 && height_ == 180);
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
	SendMessage(preview.get_hwnd(), WM_KEYDOWN, ice::input::key_a, 0);
	CHECK(ice::input::input::instance()->is_down(ice::input::key_a));
	ice::input::input::instance()->update();
	SendMessage(preview.get_hwnd(), WM_KEYUP, ice::input::key_a, 0);
	CHECK(ice::input::input::instance()->is_pressed(ice::input::key_a));
	SendMessage(preview.get_hwnd(), WM_CLOSE, 0, 0);
	CHECK(preview.should_close());
	CHECK(!main.should_close());
	second.cleanup();
	preview.cleanup();
	preview.cleanup();
	CHECK(draw(first, 0, 1, 1));
	first.cleanup();
	main.cleanup();
	main.cleanup();
	std::puts("Win32 platform checks passed: independent windows/contexts, owner, resize, key "
	          "events, close and cleanup");
	return 0;
}
