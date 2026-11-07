#pragma once
#include <utility>

namespace ice::render::vulkan::detail {
// Enter with ownership retained. A throw without a typed native outcome leaves an
// obligation; only API-specific guarantees may clear it after the call returns.
template <typename Call, typename Result>
Result queue_with_obligation(bool &pending, Call &&call, Result host_oom, Result device_oom) {
	pending             = true;
	const Result result = std::forward<Call>(call)();
	// QueueSubmit guarantees these two errors leave synchronization unaffected.
	pending = result != host_oom && result != device_oom;
	return result;
}
template <typename Call, typename Result>
Result acquire_with_obligation(bool &pending, Call &&call, Result success, Result suboptimal) {
	pending             = true;
	const Result result = std::forward<Call>(call)();
	// WSI guarantees unsuccessful acquisition leaves semaphore/fence unaffected.
	pending = result == success || result == suboptimal;
	return result;
}
} // namespace ice::render::vulkan::detail
