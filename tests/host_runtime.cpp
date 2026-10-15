#include "ice/platform/host.h"
#include <cmath>
#include <cstdio>
#include <thread>
#define CHECK(x)                                                                                   \
	do {                                                                                           \
		if (!(x)) {                                                                                \
			std::fprintf(stderr, "host check failed line %d: %s\n", __LINE__, #x);                 \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)
struct Client : ice::platform::host_client {
	int    updates = 0, renders = 0, added = 0, removed = 0, cancels = 0, inputs = 0;
	double dt = -1;
	void   on_surface_added(const ice::platform::surface &) override { ++added; }
	void   on_surface_removed(const ice::platform::surface &) override { ++removed; }
	void   on_update(double d) override {
        ++updates;
        dt = d;
	}
	void on_render(const ice::platform::surface &) override { ++renders; }
	void on_input(const ice::platform::device_input &e) override {
		++inputs;
		if (e.phase == ice::platform::input_phase::cancel)
			++cancels;
	}
};
int main() {
	Client              client;
	ice::platform::host host(client);
	auto                pointer =
	    host.add_device(ice::platform::input_capability(ice::platform::input_kind::pointer));
	auto sensor =
	    host.add_device(ice::platform::input_capability(ice::platform::input_kind::acceleration));
	ice::platform::device_input touch;
	touch.device = pointer;
	touch.phase  = ice::platform::input_phase::begin;
	CHECK(!host.post_input(touch));
	host.set_state(ice::platform::app_state::active);
	host.frame(10);
	CHECK(client.updates == 1 && client.renders == 0 && client.dt == 0); // no root window
	ice::platform::surface surface;
	surface.width  = 320;
	surface.height = 240;
	auto a = host.add_surface(surface), b = host.add_surface(surface);
	CHECK(a && b != a);
	touch.surface = a;
	CHECK(host.post_input(touch));
	host.frame(10.01);
	CHECK(client.inputs == 1 && client.renders == 2);
	CHECK(host.remove_surface(a));
	CHECK(client.cancels == 1 && client.removed == 1);
	CHECK(host.post_input(touch));
	host.frame(10.02);
	CHECK(client.inputs == 2); // only cancellation; stale surface rejected
	auto c = host.add_surface(surface);
	CHECK(c != a && c != b);
	touch.surface = b;
	touch.contact = 42;
	CHECK(host.post_input(touch));
	host.frame(10.03);
	host.set_state(ice::platform::app_state::background);
	CHECK(client.cancels == 2);
	int updates = client.updates;
	host.frame(900);
	CHECK(client.updates == updates && !host.post_input(touch));
	host.set_state(ice::platform::app_state::active);
	host.frame(1000);
	CHECK(client.dt == 0);
	host.frame(1001);
	CHECK(client.dt == .1);
	host.frame(999);
	CHECK(client.dt == 0);
	host.frame(1001.01);
	CHECK(client.dt > .009 && client.dt < .011);
	CHECK(host.resize_surface(b, 0, 0, 2));
	int renders = client.renders;
	host.frame(1001.02);
	CHECK(client.renders == renders + 1);
	CHECK(host.post_input(touch));
	host.frame(1001.03);
	CHECK(host.remove_device(pointer));
	CHECK(client.cancels == 3);
	ice::platform::device_input motion;
	motion.device      = sensor;
	motion.kind        = ice::platform::input_kind::acceleration;
	motion.x           = 9.81f;
	int         inputs = client.inputs;
	std::thread producer([&] { host.post_input(motion); });
	producer.join();
	host.frame(1001.04);
	CHECK(client.inputs == inputs + 1);
	// Removing a pointer device must not remove an independent sensor.
	pointer = host.add_device(ice::platform::input_capability(ice::platform::input_kind::pointer));
	touch.device  = pointer;
	touch.surface = c;
	CHECK(host.post_input(touch));
	host.frame(1001.05);
	for (int i = 0; i < 1025; ++i)
		host.post_input(motion);
	host.frame(1001.06);
	CHECK(host.dropped_input() == 1 && client.cancels == 4);
	touch.phase = ice::platform::input_phase::end;
	inputs      = client.inputs;
	host.post_input(touch);
	host.frame(1001.07);
	CHECK(client.inputs == inputs);
	CHECK(host.set_surface_visible(c, false));
	renders = client.renders;
	host.frame(1001.08);
	CHECK(client.renders == renders);
	CHECK(host.set_surface_visible(c, true));
	host.frame(1001.09);
	CHECK(client.renders == renders + 1);
	host.stop();
	CHECK(client.removed == 3 && host.state() == ice::platform::app_state::stopped);
	CHECK(!host.set_state(ice::platform::app_state::active) && !host.add_surface(surface) &&
	      !host.add_device(1));
	host.stop();
	Client              routed;
	ice::platform::host routing(routed);
	routing.set_state(ice::platform::app_state::active);
	auto left = routing.add_surface(surface), right = routing.add_surface(surface);
	auto fingers =
	    routing.add_device(ice::platform::input_capability(ice::platform::input_kind::pointer));
	ice::platform::device_input first;
	first.device                       = fingers;
	first.surface                      = left;
	first.phase                        = ice::platform::input_phase::begin;
	ice::platform::device_input second = first;
	second.surface                     = right;
	routing.post_input(first);
	routing.post_input(second);
	routing.frame(1);
	CHECK(routed.inputs == 2);
	routing.remove_surface(left);
	CHECK(routed.cancels == 1);
	second.phase = ice::platform::input_phase::end;
	routing.post_input(second);
	routing.frame(2);
	CHECK(routed.inputs == 4 && routed.cancels == 1);
	first.surface = right;
	routing.post_input(first);
	routing.set_surface_visible(right, false);
	routing.frame(3);
	CHECK(routed.inputs == 4);
	ice::platform::device_input unsupported;
	unsupported.device = fingers;
	unsupported.kind   = ice::platform::input_kind::acceleration;
	routing.post_input(unsupported);
	routing.frame(4);
	CHECK(routed.inputs == 4);
	unsupported.seconds = NAN;
	CHECK(!routing.post_input(unsupported));
	routing.stop();
	std::puts("Host checks passed: windowless updates, multiple/recreated surfaces, resume clock, "
	          "touch cancellation, sensors, overflow and shutdown");
}
