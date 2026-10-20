#pragma once
#include "ice/core/expected.h"
#include "ice/render/types.h"
#include <cstddef>
#include <memory>
#include <vector>

namespace ice::render {
namespace detail {
/// Private native-backend bridge for creating tickets and retaining pixel admission owners.
struct readback_access;
/// Opaque native transfer implementation, defined outside this public ownership contract.
class readback_operation;
} // namespace detail
/// Explicit per-device admission bounds supplied by the trusted host. No implicit shipping cap.
/// Zero disables admission. The same limits should be used by all captures on a device; existing
/// reservations remain charged even if a later request supplies smaller limits.
struct readback_limits {
	/// Maximum simultaneous native reservations, including transfers retained after cancellation.
	std::size_t tickets = 0;
	/// Conservative reservation-byte cap, covering native transfer and retained pixel ownership.
	std::size_t bytes = 0;
};
/// Interleaved order of four 8-bit channels in each owned pixel.
enum class readback_layout { rgba8, bgra8 };
/// Image edge represented by the first row in the owned byte vector.
enum class readback_origin { top_left, bottom_left };
/// Nonblocking transfer state: ready permits one take; cancelled is safely discarded and
/// consumed has already transferred its pixels to the caller.
enum class readback_status { pending, ready, cancelled, consumed };
/// Owned CPU bytes; safe to move to a writer thread. Row padding is explicit; no mapped pointer.
/// Move-only so retained native admission accounting also covers these bytes until destruction.
struct readback_pixels {
	readback_pixels() = default;
	/// Release pixel storage before dropping its retained native admission reservation.
	~readback_pixels() { std::vector<std::uint8_t>().swap(bytes); }
	readback_pixels(readback_pixels &&) noexcept            = default;
	readback_pixels &operator=(readback_pixels &&) noexcept = default;
	readback_pixels(const readback_pixels &)                = delete;
	readback_pixels &operator=(const readback_pixels &)     = delete;
	/// Captured width in pixels; zero in a default empty value.
	int width = 0;
	/// Captured height in pixels; zero in a default empty value.
	int height = 0;
	/// Bytes between adjacent row starts, including any explicit row padding.
	std::size_t row_stride = 0;
	/// Channel order used by every pixel in these rows.
	readback_layout layout = readback_layout::rgba8;
	/// Orientation of the first stored row; consumers must not assume top-left origin.
	readback_origin origin = readback_origin::top_left;
	/// Owned pixel rows, still covered by the retained native admission reservation.
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
	/// Create an empty ticket with no native operation.
	readback_ticket();
	/// Drop delivery ownership; a pending native operation retains resources until GPU-safe.
	~readback_ticket();
	/// Transfer the native operation to this ticket on its creating render thread.
	readback_ticket(readback_ticket &&other) noexcept;
	/// Retire this ticket's prior operation and transfer ownership from the source ticket.
	readback_ticket &operator=(readback_ticket &&other) noexcept;
	readback_ticket(const readback_ticket &)            = delete;
	readback_ticket &operator=(const readback_ticket &) = delete;
	/// Zero-timeout poll; native failures are diagnostics, not a synchronous fallback.
	ice::expected<readback_status, diagnostic> poll();
	/// Only ready tickets yield pixels, once. Pending/failed/cancelled/empty takes diagnose.
	/// The bounded native-staging-to-owned-vector copy occurs here on the render thread.
	ice::expected<readback_pixels, diagnostic> take();
	/// Discard the result, not GPU resource lifetime. Poll remains pending until GPU-safe,
	/// then cancelled. Dropping a pending ticket retains native accounting until retirement.
	void cancel() noexcept;
	/// Whether an operation is owned; this does not establish readiness or GPU completion.
	explicit operator bool() const noexcept;

private:
	explicit readback_ticket(std::unique_ptr<detail::readback_operation> operation);
	std::unique_ptr<detail::readback_operation> operation_;
	friend struct detail::readback_access;
};
/// Native admission outcome; busy has no ticket, while submitted owns an asynchronous transfer.
struct readback_submission {
	/// Whether this call submitted work or was rejected as busy before submission.
	frame_status status = frame_status::busy;
	/// Owned transfer returned for a submitted frame; empty when admission was busy.
	readback_ticket ticket;
};
} // namespace ice::render
