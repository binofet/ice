#pragma once
#include "ice/render/readback.h"
#include <atomic>
#include <cstdint>

namespace ice::render::detail {
struct readback_accounting {
	std::atomic<std::size_t> tickets{0}, bytes{0};
};
struct readback_reservation {
	std::shared_ptr<readback_accounting> accounting;
	std::size_t                          bytes;
	~readback_reservation() {
		accounting->bytes.fetch_sub(bytes, std::memory_order_relaxed);
		accounting->tickets.fetch_sub(1, std::memory_order_relaxed);
	}
};
// Single render-thread admission, possibly concurrent completion/destruction releases.
inline std::shared_ptr<readback_reservation>
reserve_readback(const std::shared_ptr<readback_accounting> &accounting, readback_limits limits,
                 std::size_t bytes) {
	const auto used = accounting->bytes.load(std::memory_order_relaxed);
	if (!limits.tickets || !limits.bytes || !bytes || bytes > limits.bytes ||
	    used > limits.bytes - bytes ||
	    accounting->tickets.load(std::memory_order_relaxed) >= limits.tickets)
		return {};
	auto result        = std::make_shared<readback_reservation>();
	result->accounting = accounting;
	result->bytes      = bytes;
	accounting->tickets.fetch_add(1, std::memory_order_relaxed);
	accounting->bytes.fetch_add(bytes, std::memory_order_relaxed);
	return result;
}
class readback_operation {
public:
	virtual ~readback_operation()                                        = default;
	virtual ice::expected<readback_status, diagnostic> poll()            = 0;
	virtual ice::expected<readback_pixels, diagnostic> take()            = 0;
	virtual void                                       cancel() noexcept = 0;
};
struct readback_access {
	static readback_ticket ticket(std::unique_ptr<readback_operation> operation) {
		return readback_ticket(std::move(operation));
	}
	static void retain(readback_pixels &pixels, std::shared_ptr<void> reservation) {
		pixels.reservation_ = std::move(reservation);
	}
};
} // namespace ice::render::detail
