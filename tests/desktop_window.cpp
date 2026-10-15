#include "ice/platform/desktop_window.h"
#include <chrono>
#include <iostream>
#include <thread>

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::cerr << "Desktop window failed " << __LINE__ << ": " << #condition << '\n';       \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)
int main() {
	using namespace ice::platform;
	CHECK(!desktop_window::create({"invalid", 0, 400, false}));
	CHECK(!desktop_window::create({std::string("\xED\xA0\x80"), 640, 400, false}));
	auto first = desktop_window::create({"ICE native surface one", 640, 400, false});
	if (!first) {
		std::cerr << first.error().message << '\n';
		return 1;
	}
	auto second = desktop_window::create({"ICE native surface two", 800, 500, false});
	CHECK(second && (*first)->id() != (*second)->id());
	CHECK((*first)->native_handle().window && (*second)->native_handle().window);
	CHECK((*first)->extent().width == 640 && (*first)->extent().height == 400);
	CHECK(!(*first)->resize(-1, 500));
	CHECK((*first)->resize(720, 480));
	(*first)->show();
	(*second)->show();
	for (int i = 0; i < 20; ++i) {
		desktop_window::pump_events();
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	auto size = (*first)->extent();
	CHECK(size.width == 720 && size.height == 480 && size.scale > 0);
	CHECK(size.pixel_width >= size.width && size.pixel_height >= size.height);
	platform_event event;
	while (platform_events::poll(event)) {
	}
	(*first)->request_close();
	(*first)->request_close();
	CHECK((*first)->should_close() && !(*second)->should_close());
	unsigned closes = 0;
	while (platform_events::poll(event))
		if (event.type == platform_event_type::close_requested)
			++closes;
	CHECK(closes == 1);
	auto retired = (*first)->id();
	first->reset();
	bool observed = false;
	while (platform_events::poll(event))
		if (event.type == platform_event_type::window_retired && event.window == retired)
			observed = true;
	CHECK(observed && (*second)->native_handle().window && !(*second)->should_close());
	second->reset();
	std::cout << "Two independent native windows: size, routing, close/retire and borrowed surface "
	             "endpoints passed; no GPU/context created\n";
}
