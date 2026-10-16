#pragma once
#include "ice/core/expected.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ice::audio {
/// Capture/encoding failure; no audio hardware is implied by the portable buffer.
struct diagnostic {
	/// Human-readable context owned by the result.
	std::string message;
	/// Native result code, or zero for portable validation failures.
	std::int32_t native_code = 0;
};
/// Preallocated mono signed PCM16 storage with an explicit duration cap. Owner-thread
/// only: serialize writes, reads, reset and destruction. No hidden capture or sharing.
class capture_buffer {
public:
	/// Allocate 1..10000 ms at 8000..192000 Hz. Failure publishes no buffer. The
	/// duration is rounded down to whole frames; all storage is allocated here.
	static ice::expected<capture_buffer, diagnostic> create(std::uint32_t sample_rate,
	                                                        std::uint32_t duration_ms = 10000);
	/// Transfer storage/accounting; the moved-from owner becomes empty with rate zero.
	capture_buffer(capture_buffer &&other) noexcept;
	/// Replace storage/accounting; the moved-from owner becomes empty with rate zero.
	capture_buffer &operator=(capture_buffer &&other) noexcept;
	/// A mutable capture has a single owner; copying is unavailable.
	capture_buffer(const capture_buffer &) = delete;
	/// A mutable capture has a single owner; copy assignment is unavailable.
	capture_buffer &operator=(const capture_buffer &) = delete;
	/// Append only what fits without allocation; return accepted mono frames. Extra
	/// frames are counted as dropped with saturation. A full buffer never overwrites.
	std::size_t append(std::span<const std::int16_t> samples) noexcept;
	/// Clear recorded samples and accounting while retaining capacity; no allocation.
	void reset() noexcept;
	/// Borrow the recorded prefix until reset/move/destruction; no mutable storage escapes.
	std::span<const std::int16_t> samples() const;
	/// Negotiated/source sampling frequency in Hz; the buffer performs no resampling.
	std::uint32_t sample_rate() const;
	/// Maximum mono frames, independent of the currently recorded prefix.
	std::size_t capacity_frames() const;
	/// Frames not retained because the duration cap was reached; saturation is UINT64_MAX.
	std::uint64_t dropped_frames() const;
	/// Whether the next nonempty append would exceed available capacity.
	bool full() const;

private:
	capture_buffer(std::uint32_t sample_rate, std::vector<std::int16_t> storage);
	std::uint32_t             sample_rate_ = 0;
	std::vector<std::int16_t> storage_;
	std::size_t               frames_  = 0;
	std::uint64_t             dropped_ = 0;
};
/// Encode the recorded prefix as bounded little-endian RIFF/WAVE PCM16 mono with a
/// standard 44-byte header. Returns owned bytes; writes no file and sends no network data.
ice::expected<std::vector<std::byte>, diagnostic> encode_wav(const capture_buffer &capture);
} // namespace ice::audio
