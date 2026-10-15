#pragma once
#include <cstddef>
#include <cstdint>
namespace ice::platform {

/// Process-scoped window identity; zero denotes application scope.
using window_id = std::uint64_t;
/// Zero denotes application scope. A native root window is not required.
constexpr window_id ic_application_scope = 0;

/// Window registration, close, focus, and application-termination notifications.
enum class platform_event_type {
	window_registered,
	window_retired,
	close_requested,
	focus_gained,
	focus_lost,
	application_quit_requested
};

/// Application event ordered by sequence number; window zero denotes application scope.
struct platform_event {
	/// Event category determining which payload fields are meaningful.
	platform_event_type type;
	/// Target window identity; zero denotes application scope.
	window_id window;
	/// Monotonically ordered event-stream sequence number.
	std::uint64_t sequence;
};

/// A bounded application event stream. Poll once, then route by window ID.
/// It supplements the legacy input singleton; it does not yet replace it.
class platform_events {
public:
	/// Allocate a process-local nonzero window identity; a native root window is not required.
	static window_id allocate_window_id();
	/// Append an event to the bounded application stream; route by window ID after polling.
	static void post(platform_event_type type, window_id window = ic_application_scope);
	/// Consume pending native/network activity without blocking; supplied times are monotonic
	/// seconds.
	static bool poll(platform_event &event);
	/// Return the cumulative count of events discarded by the bounded stream.
	static std::size_t dropped_events();
};

} // namespace ice::platform
