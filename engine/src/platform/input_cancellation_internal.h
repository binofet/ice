#pragma once
#include "ice/platform/platform_events.h"
#include <array>
namespace ice::platform::detail {
// Event posting can happen off the UI thread. Native cancellation is consumed by each
// desktop owner at its next UI pump/query, never while holding the event-stream mutex.
void                         register_window_input(window_id window);
void                         unregister_window_input(window_id window);
std::array<std::uint64_t, 2> window_input_cancellation(window_id window);
} // namespace ice::platform::detail
