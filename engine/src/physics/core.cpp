#include "internal.h"
#include <atomic>
#include <limits>
namespace ice::physics::detail {
std::uint64_t next_world_identity() {
	static std::atomic<std::uint64_t> next{1};
	auto                              value = next.load(std::memory_order_relaxed);
	for (;;) {
		if (value == std::numeric_limits<std::uint64_t>::max())
			return 0;
		if (next.compare_exchange_weak(value, value + 1, std::memory_order_relaxed))
			return value;
	}
}
} // namespace ice::physics::detail
