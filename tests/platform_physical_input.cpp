#include "../engine/src/platform/physical_key_internal.h"
#include "ice/platform/desktop_window.h"
#include "ice/platform/host.h"
#include <cstdio>
#include <limits>
#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::fprintf(stderr, "Physical input failed at %d: %s\n", __LINE__, #condition);       \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)
namespace {
using namespace ice::platform;
class window final : public desktop_window {
public:
	~window() override { cancel_input(); }
	void show(bool visible) override {
		if (!visible)
			cancel_input();
	}
	bool                  resize(int, int) override { return true; }
	void                  fullscreen(bool) override {}
	window_extent         extent() const override { return {640, 480, 1280, 960, 2}; }
	native_window_handle  native_handle() const override { return {native_window_kind::appkit}; }
	relative_pointer_kind relative_pointer_capability() const override {
		return relative_pointer_kind::native;
	}
	bool     focused = true, native_captured = false;
	unsigned releases = 0;

private:
	ice::expected<void, window_diagnostic>
	change_pointer_mode(ice::platform::pointer_mode mode) override {
		if (mode == ice::platform::pointer_mode::relative && !focused)
			return ice::unexpected(window_diagnostic{"Test window is not focused"});
		if (mode == ice::platform::pointer_mode::normal && native_captured)
			++releases;
		native_captured = mode == ice::platform::pointer_mode::relative;
		return {};
	}
};
void drain() {
	platform_event event;
	while (platform_events::poll(event)) {
	}
}
} // namespace
int main() {
	using namespace ice::platform;
	CHECK(detail::mac_physical_key(13) == physical_key::w);
	CHECK(detail::mac_physical_key(56) == physical_key::left_shift);
	CHECK(detail::mac_physical_key(60) == physical_key::right_shift);
	CHECK(detail::win32_physical_key(0x11, false) == physical_key::w);
	CHECK(detail::win32_physical_key(0x1d, false) == physical_key::left_control);
	CHECK(detail::win32_physical_key(0x1d, true) == physical_key::right_control);
	CHECK(detail::win32_physical_key(0x48, false) == physical_key::keypad_8);
	CHECK(detail::win32_physical_key(0x48, true) == physical_key::up);
	CHECK(detail::x11_physical_key("AD02") == physical_key::w);
	CHECK(detail::x11_physical_key("AC01") == physical_key::a);
	CHECK(detail::x11_physical_key(std::string_view("UP\0\0", 4)) == physical_key::up);
	CHECK(detail::x11_physical_key("FK24") == physical_key::f24);
	CHECK(detail::x11_physical_key("not-a-key") == physical_key::unknown);
	CHECK(!valid_physical_key(physical_key::unknown));
	CHECK(!valid_physical_key(static_cast<physical_key>(0xffff)));
	device_input shared;
	shared.key    = physical_key::w;
	shared.button = mouse_button::secondary;
	CHECK(shared.key == physical_key::w && shared.button == mouse_button::secondary);
	window first, second;
	drain();
	CHECK(first.pointer_mode() == pointer_mode::normal);
	CHECK(first.set_pointer_mode(pointer_mode::relative) && first.native_captured);
	first.enable_text_input();
	first.enable_text_input(false);
	CHECK(first.pointer_mode() == pointer_mode::relative && first.native_captured);
	platform_event event;
	CHECK(platform_events::poll(event) && event.type == platform_event_type::input_cancelled &&
	      event.cancellation == input_cancellation::text);
	platform_event key{platform_event_type::physical_key, first.id(), 0};
	key.key       = physical_key::w;
	key.pressed   = true;
	key.modifiers = modifier_flag(key_modifier::shift);
	CHECK(platform_events::post(key));
	key.pressed = false;
	CHECK(platform_events::post(key));
	platform_event button{platform_event_type::pointer_button, second.id(), 0};
	button.button  = mouse_button::secondary;
	button.pressed = true;
	button.x       = 12.5;
	button.y       = 20.25;
	CHECK(platform_events::post(button));
	platform_event motion{platform_event_type::pointer_move, first.id(), 0};
	motion.x           = 100;
	motion.y           = 200;
	motion.delta_x     = -4.5;
	motion.delta_y     = 3.25;
	motion.delta_units = pointer_delta_unit::device_counts;
	CHECK(platform_events::post(motion));
	platform_event scroll{platform_event_type::scroll, second.id(), 0};
	scroll.scroll_x       = -1;
	scroll.scroll_y       = 2.5;
	scroll.precise_scroll = true;
	CHECK(platform_events::post(scroll));
	CHECK(platform_events::poll(event) && event.key == physical_key::w && event.pressed &&
	      event.text.empty());
	auto sequence = event.sequence;
	CHECK(platform_events::poll(event) && !event.pressed && event.sequence > sequence);
	CHECK(platform_events::poll(event) && event.button == mouse_button::secondary &&
	      event.x == 12.5);
	CHECK(platform_events::poll(event) && event.delta_x == -4.5 &&
	      event.delta_units == pointer_delta_unit::device_counts);
	CHECK(platform_events::poll(event) && event.scroll_y == 2.5 && event.precise_scroll);
	platform_events::post(platform_event_type::focus_lost, second.id());
	CHECK(first.pointer_mode() == pointer_mode::relative);
	drain();
	platform_events::post(platform_event_type::focus_lost, first.id());
	CHECK(first.pointer_mode() == pointer_mode::normal && !first.native_captured &&
	      first.releases == 1);
	drain();
	first.focused = false;
	CHECK(!first.set_pointer_mode(pointer_mode::relative));
	CHECK(first.pointer_mode() == pointer_mode::normal);
	first.focused = true;
	CHECK(!first.set_pointer_mode(static_cast<pointer_mode>(42)));
	const double nan = std::numeric_limits<double>::quiet_NaN();
	for (unsigned field = 0; field < 6; ++field) {
		CHECK(first.set_pointer_mode(pointer_mode::relative));
		auto    invalid = motion;
		double *fields[]{&invalid.x,       &invalid.y,        &invalid.delta_x,
		                 &invalid.delta_y, &invalid.scroll_x, &invalid.scroll_y};
		*fields[field] = nan;
		CHECK(!platform_events::post(invalid));
		CHECK(first.pointer_mode() == pointer_mode::normal && !first.native_captured);
		CHECK(platform_events::poll(event) && event.type == platform_event_type::input_cancelled &&
		      event.cancellation == input_cancellation::all);
	}
	key.key = physical_key::unknown;
	CHECK(!platform_events::post(key));
	drain();
	button.button = mouse_button::none;
	CHECK(!platform_events::post(button));
	drain();
	motion.window = application_scope;
	CHECK(!platform_events::post(motion));
	drain();
	motion.window  = first.id();
	motion.delta_x = std::numeric_limits<double>::infinity();
	CHECK(!platform_events::post(motion));
	drain();
	motion.delta_x = 1;
	CHECK(first.set_pointer_mode(pointer_mode::relative));
	for (unsigned index = 0; index < 1024; ++index)
		CHECK(platform_events::post(motion));
	CHECK(!platform_events::post(motion));
	CHECK(first.pointer_mode() == pointer_mode::normal && !first.native_captured);
	CHECK(platform_events::poll(event) && event.type == platform_event_type::input_cancelled &&
	      event.window == application_scope);
	CHECK(!platform_events::poll(event));
	CHECK(first.set_pointer_mode(pointer_mode::relative));
	first.show(false);
	CHECK(first.pointer_mode() == pointer_mode::normal);
	drain();
	CHECK(first.set_pointer_mode(pointer_mode::relative));
	first.request_close();
	CHECK(first.pointer_mode() == pointer_mode::normal && first.should_close());
	CHECK(!first.set_pointer_mode(pointer_mode::relative));
	drain();
	CHECK(second.set_pointer_mode(pointer_mode::relative));
	platform_events::post(platform_event_type::application_quit_requested);
	CHECK(second.pointer_mode() == pointer_mode::normal);
	std::puts("Physical input passed: shared positions, text separation, finite bounded payloads, "
	          "focus/close/overflow capture cancellation");
	return 0;
}
