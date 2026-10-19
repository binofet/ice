#pragma once
#include "ice/core/expected.h"
#include "ice/render/types.h"
#include <cstddef>
#include <memory>
#include <vector>

namespace ice::render {
namespace detail {
struct readback_access;
class readback_operation;
} // namespace detail
/// Explicit per-device admission bounds supplied by the trusted host. No implicit shipping cap.
/// Zero disables admission. The same limits should be used by all captures on a device; existing
/// reservations remain charged even if a later request supplies smaller limits.
struct readback_limits {
	std::size_t tickets = 0, bytes = 0;
};
enum class readback_layout { rgba8, bgra8 };
enum class readback_origin { top_left, bottom_left };
enum class readback_status { pending, ready, cancelled, consumed };
/// Owned CPU bytes; safe to move to a writer thread. Row padding is explicit; no mapped pointer.
/// Move-only so retained native admission accounting also covers these bytes until destruction.
struct readback_pixels {
	readback_pixels() = default;
	~readback_pixels() { std::vector<std::uint8_t>().swap(bytes); }
	readback_pixels(readback_pixels &&) noexcept                 = default;
	readback_pixels &operator=(readback_pixels &&) noexcept      = default;
	readback_pixels(const readback_pixels &)                     = delete;
	readback_pixels          &operator=(const readback_pixels &) = delete;
	int                       width = 0, height = 0;
	std::size_t               row_stride = 0;
	readback_layout           layout     = readback_layout::rgba8;
	readback_origin           origin     = readback_origin::top_left;
	std::vector<std::uint8_t> bytes;

private:
	std::shared_ptr<void> reservation_;
	friend struct detail::readback_access;
};
/// One asynchronous transfer. All methods/moves/destruction use the creating render thread;
/// only the extracted owned pixels may cross to a CPU worker. Poll/take/cancel never explicitly
/// wait for the GPU; driver calls/copies do not promise bounded latency. Keep the device alive
/// through ticket retirement: final native-device destruction retains its existing shutdown drain.
class readback_ticket {
public:
	readback_ticket();
	~readback_ticket();
	readback_ticket(readback_ticket &&) noexcept;
	readback_ticket &operator=(readback_ticket &&) noexcept;
	readback_ticket(const readback_ticket &)            = delete;
	readback_ticket &operator=(const readback_ticket &) = delete;
	/// Zero-timeout poll; native failures are diagnostics, not a synchronous fallback.
	ice::expected<readback_status, diagnostic> poll();
	/// Only ready tickets yield pixels, once. Pending/failed/cancelled/empty takes diagnose.
	/// The bounded native-staging-to-owned-vector copy occurs here on the render thread.
	ice::expected<readback_pixels, diagnostic> take();
	/// Discard the result, not GPU resource lifetime. Poll remains pending until GPU-safe,
	/// then cancelled. Dropping a pending ticket retains native accounting until retirement.
	void     cancel() noexcept;
	explicit operator bool() const noexcept;

private:
	explicit readback_ticket(std::unique_ptr<detail::readback_operation> operation);
	std::unique_ptr<detail::readback_operation> operation_;
	friend struct detail::readback_access;
};
struct readback_submission {
	frame_status    status = frame_status::busy;
	readback_ticket ticket;
};
} // namespace ice::render
