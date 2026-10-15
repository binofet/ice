#include "../engine/src/platform/text_input_internal.h"
#include "ice/platform/desktop_window.h"
#include <clocale>
#include <cstdio>
#include <cstring>
#include <limits>
#if !defined(_WIN32)
#include <sys/mman.h>
#include <unistd.h>
#endif

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::fprintf(stderr, "Platform input failed at line %d: %s\n", __LINE__, #condition);  \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

namespace {
class window final : public ice::platform::desktop_window {
public:
	void                                show(bool) override {}
	bool                                resize(int, int) override { return true; }
	void                                fullscreen(bool) override {}
	ice::platform::window_extent        extent() const override { return {}; }
	ice::platform::native_window_handle native_handle() const override {
		return {ice::platform::native_window_kind::appkit};
	}
	int cancellations = 0;

private:
	void cancel_native_text_input() override { ++cancellations; }
};
void drain() {
	ice::platform::platform_event event;
	while (ice::platform::platform_events::poll(event)) {
	}
}
} // namespace
int main() {
	using namespace ice::platform;
	std::string decoded;
	const char  raw[] = {'a', 'b', 'c'};
	CHECK(detail::locale_characters(raw, 3, decoded) && decoded == "abc");
#if !defined(_WIN32)
	const std::string old_locale = std::setlocale(LC_CTYPE, nullptr);
	CHECK(std::setlocale(LC_CTYPE, "en_US.UTF-8") || std::setlocale(LC_CTYPE, "C.UTF-8"));
	const auto page_size = std::size_t(sysconf(_SC_PAGESIZE));
	void      *mapping =
	    mmap(nullptr, page_size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	CHECK(mapping != MAP_FAILED);
	CHECK(mprotect(static_cast<char *>(mapping) + page_size, page_size, PROT_NONE) == 0);
	const char unicode[] = "\xC3\xA9\xF0\x9F\xA6\x8A";
	char      *at_end    = static_cast<char *>(mapping) + page_size - 6;
	std::memcpy(at_end, unicode, 6);
	// The following page is inaccessible: neither string has a readable terminator.
	CHECK(detail::locale_characters(at_end, 2, decoded) && decoded == unicode);
	at_end = static_cast<char *>(mapping) + page_size - 3;
	std::memcpy(at_end, raw, 3);
	CHECK(detail::locale_characters(at_end, 3, decoded) && decoded == "abc");
	CHECK(munmap(mapping, page_size * 2) == 0);
	CHECK(std::setlocale(LC_CTYPE, old_locale.c_str()));
#endif
	drain();
	window first, second;
	CHECK(first.id() && second.id() != first.id());
	drain();
	CHECK(!first.text_input_enabled());
	first.enable_text_input();
	CHECK(first.text_input_enabled());
	CHECK(first.set_text_input_rect({10, 20, 1, 18}));
	CHECK(!first.set_text_input_rect({std::numeric_limits<float>::quiet_NaN(), 0, 1, 18}));
	CHECK(first.text_input_area().x == 10 && first.text_input_area().y == 20);
	platform_event commit{platform_event_type::text_committed, first.id(), 0};
	commit.text = "A\xF0\x9F\xA6\x8A\xC3\xA9";
	CHECK(platform_events::post(commit));
	platform_event preedit{platform_event_type::text_composition, second.id(), 0};
	preedit.text             = "\xE6\x97\xA5\xE6\x9C\xAC";
	preedit.selection_start  = 3;
	preedit.selection_length = 3;
	CHECK(platform_events::post(preedit));
	platform_event command{platform_event_type::edit_command, first.id(), 0};
	command.command   = edit_command::word_left;
	command.modifiers = modifier_flag(key_modifier::alt) | modifier_flag(key_modifier::shift);
	command.repeat    = true;
	CHECK(platform_events::post(command));
	platform_event event;
	CHECK(platform_events::poll(event) && event.window == first.id() && event.text == commit.text);
	const auto sequence = event.sequence;
	CHECK(platform_events::poll(event) && event.window == second.id() &&
	      event.text == preedit.text);
	CHECK(event.selection_start == 3 && event.selection_length == 3 && event.sequence > sequence);
	CHECK(platform_events::poll(event) && event.command == edit_command::word_left && event.repeat);
	CHECK(has_modifier(event.modifiers, key_modifier::shift) &&
	      has_modifier(event.modifiers, key_modifier::alt));
	CHECK(!platform_events::poll(event));
	preedit.selection_start  = 1;
	preedit.selection_length = 0;
	CHECK(!platform_events::post(preedit));
	CHECK(platform_events::poll(event) && event.type == platform_event_type::input_cancelled &&
	      event.window == second.id());
	commit.text = "\xED\xA0\x80";
	CHECK(!platform_events::post(commit));
	drain();
	commit.text = std::string("a\0b", 3);
	CHECK(!platform_events::post(commit));
	drain();
	commit.text = std::string(4097, 'x');
	CHECK(!platform_events::post(commit));
	drain();
	first.enable_text_input(false);
	CHECK(first.cancellations == 1 && !first.text_input_enabled());
	CHECK(platform_events::poll(event) && event.type == platform_event_type::input_cancelled &&
	      event.window == first.id());
	platform_events::post(platform_event_type::focus_lost, second.id());
	CHECK(platform_events::poll(event) && event.type == platform_event_type::focus_lost);
	CHECK(platform_events::poll(event) && event.type == platform_event_type::input_cancelled &&
	      event.window == second.id());
	second.request_close();
	second.request_close();
	CHECK(second.should_close() && second.cancellations == 1);
	CHECK(platform_events::poll(event) && event.type == platform_event_type::close_requested &&
	      event.window == second.id());
	CHECK(platform_events::poll(event) && event.type == platform_event_type::input_cancelled &&
	      event.window == second.id());
	const auto dropped = platform_events::dropped_events();
	for (int index = 0; index < 1024; ++index)
		CHECK(platform_events::post(command));
	CHECK(!platform_events::post(command));
	CHECK(platform_events::poll(event) && event.type == platform_event_type::input_cancelled &&
	      event.window == application_scope);
	CHECK(platform_events::dropped_events() > dropped && !platform_events::poll(event));
	commit.text.assign(4096, 'x');
	for (int index = 0; index < 64; ++index)
		CHECK(platform_events::post(commit));
	CHECK(!platform_events::post(commit));
	CHECK(platform_events::poll(event) && event.type == platform_event_type::input_cancelled &&
	      event.window == application_scope);
	CHECK(!platform_events::poll(event));
	commit.text = "recovered";
	CHECK(platform_events::post(commit));
	CHECK(platform_events::poll(event) && event.text == "recovered");
	std::puts("Platform input passed: UTF-8/preedit boundaries, per-window routing, modifiers, "
	          "focus/disable cancellation and event/text overflow recovery");
}
