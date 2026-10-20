#include "ice/platform/desktop_window.h"
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <poll.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::fprintf(stderr, "Native X11 clipboard failed at line %d: %s\n", __LINE__,         \
			             #condition);                                                              \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

namespace {
using ice::platform::desktop_window;
using ice::platform::platform_event;
using ice::platform::platform_event_type;
using ice::platform::platform_events;
constexpr const char *sample = "A cat: \xE7\x8C\xAB and \xF0\x9F\xA6\x8A";
enum class reply_kind {
	text,
	oversized,
	incremental,
	invalid_utf8,
	refused,
	silent,
	destroyed_requestor
};

// Each external owner has a distinct process/Display. Its bounded request loop
// cannot rely on the engine pumping a queue during synchronous clipboard reads.
class external_owner {
public:
	explicit external_owner(reply_kind kind) {
		int ready[2], stop[2];
		if (pipe(ready) != 0)
			return;
		if (pipe(stop) != 0) {
			::close(ready[0]);
			::close(ready[1]);
			return;
		}
		process_ = fork();
		if (process_ == 0) {
			::close(ready[0]);
			::close(stop[1]);
			const int result = serve(kind, ready[1], stop[0]);
			_exit(result);
		}
		::close(ready[1]);
		::close(stop[0]);
		if (process_ < 0) {
			::close(ready[0]);
			::close(stop[1]);
			return;
		}
		stop_ = stop[1];
		pollfd notification{ready[0], POLLIN, 0};
		char   accepted = 0;
		valid_          = ::poll(&notification, 1, 2000) > 0 && read(ready[0], &accepted, 1) == 1 &&
		         accepted == '1';
		::close(ready[0]);
	}
	~external_owner() { finish(); }
	bool valid() const { return valid_; }
	bool finish() {
		if (stop_ >= 0) {
			// EOF requests shutdown, including an already-exited peer (no SIGPIPE).
			::close(stop_);
			stop_ = -1;
		}
		if (process_ > 0) {
			int   status = 0;
			pid_t joined;
			do {
				joined = waitpid(process_, &status, 0);
			} while (joined < 0 && errno == EINTR);
			process_ = -1;
			return joined > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
		}
		return true;
	}

private:
	static int serve(reply_kind kind, int ready, int stop) {
		auto *display = XOpenDisplay(nullptr);
		if (!display)
			return 2;
		const auto window =
		    XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, 1, 1, 0, 0, 0);
		const auto selection = XInternAtom(display, "CLIPBOARD", False);
		const auto utf8      = XInternAtom(display, "UTF8_STRING", False);
		XSetSelectionOwner(display, selection, window, CurrentTime);
		XSync(display, False);
		if (XGetSelectionOwner(display, selection) != window || write(ready, "1", 1) != 1)
			return 3;
		::close(ready);
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
		while (std::chrono::steady_clock::now() < deadline) {
			while (XPending(display)) {
				XEvent request{};
				XNextEvent(display, &request);
				if (request.type != SelectionRequest || kind == reply_kind::silent)
					continue;
				const auto &input = request.xselectionrequest;
				if (kind == reply_kind::destroyed_requestor) {
					XDestroyWindow(display, input.requestor);
					XSync(display, False);
					continue;
				}
				XEvent reply{};
				reply.xselection.type      = SelectionNotify;
				reply.xselection.display   = display;
				reply.xselection.requestor = input.requestor;
				reply.xselection.selection = input.selection;
				reply.xselection.target    = input.target;
				reply.xselection.time      = input.time;
				reply.xselection.property  = None;
				if (kind != reply_kind::refused) {
					const auto property = input.property == None ? input.target : input.property;
					if (kind == reply_kind::incremental) {
						const unsigned long size = 65536;
						XChangeProperty(display, input.requestor, property,
						                XInternAtom(display, "INCR", False), 32, PropModeReplace,
						                reinterpret_cast<const unsigned char *>(&size), 1);
					} else {
						const std::string value =
						    kind == reply_kind::oversized      ? std::string(5000, 'x')
						    : kind == reply_kind::invalid_utf8 ? "\xED\xA0\x80"
						                                       : sample;
						XChangeProperty(display, input.requestor, property, utf8, 8,
						                PropModeReplace,
						                reinterpret_cast<const unsigned char *>(value.data()),
						                int(value.size()));
					}
					reply.xselection.property = property;
				}
				XSendEvent(display, input.requestor, False, 0, &reply);
				XFlush(display);
			}
			std::array<pollfd, 2> ready_fds{
			    {{stop, POLLIN, 0}, {ConnectionNumber(display), POLLIN, 0}}};
			if (::poll(ready_fds.data(), ready_fds.size(), 50) < 0 && errno != EINTR)
				return 4;
			if (ready_fds[0].revents & (POLLIN | POLLHUP)) {
				XDestroyWindow(display, window);
				XCloseDisplay(display);
				::close(stop);
				return 0;
			}
		}
		return 5;
	}
	pid_t process_ = -1;
	int   stop_    = -1;
	bool  valid_   = false;
};

bool engine_serves(Display *display, Window requestor, Atom target, bool old_property = false) {
	const auto selection = XInternAtom(display, "CLIPBOARD", False);
	const auto property  = XInternAtom(display, "_ICE_CLIPBOARD_TEST", False);
	XConvertSelection(display, selection, target, old_property ? None : property, requestor,
	                  CurrentTime);
	XFlush(display);
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
	while (std::chrono::steady_clock::now() < deadline) {
		desktop_window::pump_events();
		while (XPending(display)) {
			XEvent event{};
			XNextEvent(display, &event);
			if (event.type != SelectionNotify)
				continue;
			if (event.xselection.property != (old_property ? target : property))
				return false;
			Atom           type;
			int            format;
			unsigned long  count, remaining;
			unsigned char *bytes = nullptr;
			const auto     status =
			    XGetWindowProperty(display, requestor, event.xselection.property, 0, 4096, True,
			                       AnyPropertyType, &type, &format, &count, &remaining, &bytes);
			bool valid = status == Success && !remaining;
			if (target == XInternAtom(display, "TARGETS", False)) {
				valid = valid && type == XA_ATOM && format == 32 && count == 2 && bytes;
				if (valid) {
					const auto *atoms = reinterpret_cast<const Atom *>(bytes);
					valid             = atoms[0] == target &&
					        atoms[1] == XInternAtom(display, "UTF8_STRING", False);
				}
			} else
				valid = valid && type == target && format == 8 && bytes &&
				        std::string(reinterpret_cast<const char *>(bytes), count) == sample;
			if (bytes)
				XFree(bytes);
			return valid;
		}
		pollfd ready{ConnectionNumber(display), POLLIN, 0};
		::poll(&ready, 1, 1);
	}
	return false;
}
} // namespace

int main() {
	// This executable replaces the real CLIPBOARD selection. Run only on its own
	// disposable Xvfb server, never the user's display or native desktop clipboard.
	const auto *isolated = std::getenv("ICE_CLIPBOARD_TEST_ISOLATED");
	CHECK(isolated && std::strcmp(isolated, "1") == 0);
	auto first  = desktop_window::create({"ICE clipboard owner", 320, 200, false});
	auto second = desktop_window::create({"ICE clipboard reader", 320, 200, false});
	CHECK(first && second);
	auto *display = static_cast<Display *>((*first)->native_handle().display);
	CHECK((*first)->write_clipboard_text(sample));
	CHECK((*second)->read_clipboard_text().value() == sample);
	CHECK(!(*second)->read_clipboard_text(4));

	auto *peer = XOpenDisplay(nullptr);
	CHECK(peer);
	const auto requestor = XCreateSimpleWindow(peer, DefaultRootWindow(peer), 0, 0, 1, 1, 0, 0, 0);
	CHECK(engine_serves(peer, requestor, XInternAtom(peer, "TARGETS", False)));
	CHECK(engine_serves(peer, requestor, XInternAtom(peer, "UTF8_STRING", False)));
	CHECK(engine_serves(peer, requestor, XInternAtom(peer, "UTF8_STRING", False), true));

	// A peer can disappear after its request was queued. Serving it must not make
	// Xlib's default BadWindow handler terminate the process.
	XConvertSelection(peer, XInternAtom(peer, "CLIPBOARD", False),
	                  XInternAtom(peer, "UTF8_STRING", False),
	                  XInternAtom(peer, "_ICE_GONE", False), requestor, CurrentTime);
	XSync(peer, False);
	XDestroyWindow(peer, requestor);
	XSync(peer, False);
	desktop_window::pump_events();
	XCloseDisplay(peer);

	for (const auto kind : {reply_kind::text, reply_kind::oversized, reply_kind::incremental,
	                        reply_kind::invalid_utf8, reply_kind::refused, reply_kind::silent,
	                        reply_kind::destroyed_requestor}) {
		external_owner owner(kind);
		CHECK(owner.valid());
		// Neither external selection conversion nor timeout is allowed to consume
		// or dispatch the application's existing translated input queue.
		platform_event event;
		while (platform_events::poll(event)) {
		}
		platform_event kept{platform_event_type::text_committed, (*second)->id(), 0};
		kept.text = "pending prompt input";
		CHECK(platform_events::post(std::move(kept)));
		const auto started = std::chrono::steady_clock::now();
		const auto result  = (*second)->read_clipboard_text();
		const auto elapsed = std::chrono::steady_clock::now() - started;
		if (kind == reply_kind::text)
			CHECK(result && *result == sample);
		else if (kind == reply_kind::refused)
			CHECK(result && result->empty());
		else
			CHECK(!result);
		CHECK(elapsed < std::chrono::seconds(2));
		CHECK(platform_events::poll(event) && event.type == platform_event_type::text_committed &&
		      event.text == "pending prompt input" && event.window == (*second)->id());
		CHECK(!platform_events::poll(event));
		CHECK(owner.finish());
		XSync(display, False);
		desktop_window::pump_events();
	}

	CHECK((*second)->write_clipboard_text(sample));
	CHECK((*first)->read_clipboard_text().value() == sample);
	second->reset();
	CHECK((*first)->read_clipboard_text().value().empty());
	CHECK((*first)->write_clipboard_text(""));
	CHECK((*first)->read_clipboard_text().value().empty());
	std::puts("Native X11 clipboard: owned/external Unicode, TARGETS, bounds/refusal/INCR, "
	          "timeout, queue and lifetime passed");
	return 0;
}
