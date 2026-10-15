#ifndef ICE_SAMPLE_HOST_H
#define ICE_SAMPLE_HOST_H
#include "ice/math/rand.h"
#include "ice/memory/memory.h"
#include "ice/platform/window.h"
#include "ice/render/gl/api.h"
#include "ice/util/time.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <functional>
#include <string>
#include <thread>
#include <vector>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#ifndef WIN32
#include <unistd.h>
#endif
namespace sample {
inline void initialize() {
	char executable[4096] = {};
	bool located          = false;
#ifdef __APPLE__
	uint32_t size = sizeof(executable);
	located       = _NSGetExecutablePath(executable, &size) == 0;
#elif defined(WIN32)
	located = GetModuleFileNameA(nullptr, executable, sizeof(executable)) > 0;
#else
	located = readlink("/proc/self/exe", executable, sizeof(executable) - 1) > 0;
#endif
	if (located) {
		std::string path(executable);
		path = path.substr(0, path.find_last_of("/\\"));
#ifdef WIN32
		SetCurrentDirectoryA(path.c_str());
#else
		chdir(path.c_str());
#endif
	}
	setvbuf(stdout, nullptr, _IONBF, 0);
	ice::memory::memory::init();
	ice::util::sys_time_init();
	ice::math::rand_auto_seed();
}
inline bool capture(ice::platform::window *window, const std::string &path) {
	int width_, height_;
	window->get_framebuffer_size(&width_, &height_);
	if (width_ <= 0 || height_ <= 0)
		return false;
	std::vector<unsigned char> pixels(width_ * height_ * 3);
	glReadBuffer(GL_BACK);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, width_, height_, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
	if (glGetError() != GL_NO_ERROR ||
	    std::count_if(pixels.begin(), pixels.end(), [](unsigned char c) { return c > 20; }) < 128)
		return false;
	FILE *file = fopen(path.c_str(), "wb");
	if (!file)
		return false;
	fprintf(file, "P6\n%d %d\n255\n", width_, height_);
	for (int row = height_ - 1; row >= 0; --row)
		fwrite(pixels.data() + row * width_ * 3, 1, width_ * 3, file);
	fclose(file);
	return true;
}
} // namespace sample
#endif
