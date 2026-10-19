#include "ice/platform/desktop_window.h"
#include <cstdio>
using namespace ice::platform;
#define CHECK(x)                                                                                   \
	do {                                                                                           \
		if (!(x)) {                                                                                \
			std::fprintf(stderr, "Clipboard check %d: %s\n", __LINE__, #x);                        \
			return 1;                                                                              \
		}                                                                                          \
	} while (false)
namespace {
class fake_window final : public desktop_window {
public:
	int                  reads = 0, writes = 0;
	std::string          incoming, outgoing;
	bool                 fail = false;
	void                 show(bool) override {}
	bool                 resize(int, int) override { return true; }
	void                 fullscreen(bool) override {}
	window_extent        extent() const override { return {}; }
	native_window_handle native_handle() const override { return {native_window_kind::appkit}; }
	ice::expected<std::string, window_diagnostic> read_native_clipboard_text(std::size_t) override {
		++reads;
		if (fail)
			return ice::unexpected(window_diagnostic{"denied"});
		return incoming;
	}
	ice::expected<void, window_diagnostic>
	write_native_clipboard_text(std::string_view text) override {
		++writes;
		if (fail)
			return ice::unexpected(window_diagnostic{"denied"});
		outgoing = text;
		return {};
	}
};
} // namespace
int main() {
	fake_window window;
	CHECK(!window.read_clipboard_text(0));
	CHECK(!window.read_clipboard_text(16 * 1024 * 1024 + 1));
	CHECK(window.reads == 0);
	CHECK(!window.write_clipboard_text(std::string("a\0b", 3)));
	CHECK(!window.write_clipboard_text("\xED\xA0\x80"));
	CHECK(!window.write_clipboard_text(std::string(16 * 1024 * 1024 + 1, 'a')));
	CHECK(window.writes == 0);
	CHECK(window.write_clipboard_text("猫😀\n"));
	CHECK(window.outgoing == "猫😀\n");
	window.incoming = "猫😀";
	auto text       = window.read_clipboard_text(7);
	CHECK(text && *text == window.incoming);
	CHECK(!window.read_clipboard_text(6));
	window.incoming = "\xF0\x9F";
	CHECK(!window.read_clipboard_text());
	window.incoming.clear();
	CHECK(window.read_clipboard_text() && window.read_clipboard_text()->empty());
	window.fail = true;
	CHECK(!window.read_clipboard_text() && !window.write_clipboard_text("cut"));
	CHECK(window.outgoing == "猫😀\n");
}
