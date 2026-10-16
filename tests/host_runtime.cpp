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
struct input_client final : ice::platform::host_client {
	std::vector<ice::platform::device_input> events;
	void on_input(const ice::platform::device_input &event) override { events.push_back(event); }
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
	// Typed controls contribute to held identity alongside legacy contact codes.
	// Two keys/buttons using the default contact zero must not suppress one another.
	{
		using namespace ice::platform;
		input_client        recorded;
		ice::platform::host controls(recorded);
		CHECK(controls.set_state(app_state::active));
		const auto   target   = controls.add_surface(surface);
		const auto   keyboard = controls.add_device(input_capability(input_kind::key));
		device_input w;
		w.device  = keyboard;
		w.surface = target;
		w.kind    = input_kind::key;
		w.phase   = input_phase::begin;
		w.key     = physical_key::w;
		auto d    = w;
		d.key     = physical_key::d;
		CHECK(w.contact == 0 && d.contact == 0);
		CHECK(controls.post_input(w) && controls.post_input(d));
		controls.frame(0);
		CHECK(recorded.events.size() == 2 && recorded.events[0].key == physical_key::w &&
		      recorded.events[1].key == physical_key::d);
		w.phase = input_phase::end;
		d.phase = input_phase::change;
		CHECK(controls.post_input(w) && controls.post_input(d));
		controls.frame(1);
		CHECK(recorded.events.size() == 4 && recorded.events[2].key == physical_key::w &&
		      recorded.events[2].phase == input_phase::end &&
		      recorded.events[3].key == physical_key::d &&
		      recorded.events[3].phase == input_phase::change);
		CHECK(controls.set_state(app_state::background));
		CHECK(recorded.events.size() == 5 && recorded.events.back().key == physical_key::d &&
		      recorded.events.back().phase == input_phase::cancel);
		CHECK(controls.set_state(app_state::active));
		const auto   mouse = controls.add_device(input_capability(input_kind::pointer));
		device_input primary;
		primary.device      = mouse;
		primary.surface     = target;
		primary.phase       = input_phase::begin;
		primary.button      = mouse_button::primary;
		auto secondary      = primary;
		secondary.button    = mouse_button::secondary;
		auto legacy_touch   = primary;
		legacy_touch.button = mouse_button::none;
		CHECK(primary.contact == 0 && secondary.contact == 0 && legacy_touch.contact == 0);
		CHECK(controls.post_input(primary) && controls.post_input(secondary) &&
		      controls.post_input(legacy_touch));
		controls.frame(2);
		CHECK(recorded.events.size() == 8 && recorded.events[5].button == mouse_button::primary &&
		      recorded.events[6].button == mouse_button::secondary &&
		      recorded.events[7].button == mouse_button::none);
		primary.phase   = input_phase::end;
		secondary.phase = input_phase::change;
		CHECK(controls.post_input(primary) && controls.post_input(secondary));
		controls.frame(3);
		CHECK(recorded.events.size() == 10 && recorded.events[8].button == mouse_button::primary &&
		      recorded.events[8].phase == input_phase::end &&
		      recorded.events[9].button == mouse_button::secondary &&
		      recorded.events[9].phase == input_phase::change);
		CHECK(controls.remove_device(mouse));
		CHECK(recorded.events.size() == 12 &&
		      recorded.events[10].button == mouse_button::secondary &&
		      recorded.events[10].phase == input_phase::cancel &&
		      recorded.events[11].button == mouse_button::none &&
		      recorded.events[11].phase == input_phase::cancel);
		controls.stop();
	}
	std::puts("Host checks passed: windowless updates, multiple/recreated surfaces, resume clock, "
	          "typed key/button and legacy contact identity, cancellation, sensors, overflow and "
	          "shutdown");
}
