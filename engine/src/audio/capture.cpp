#include "ice/audio/capture.h"
#include <algorithm>
#include <limits>
#include <new>
#include <utility>

namespace ice::audio {
capture_buffer::capture_buffer(std::uint32_t sample_rate, std::vector<std::int16_t> storage)
    : sample_rate_(sample_rate), storage_(std::move(storage)) {}
capture_buffer::capture_buffer(capture_buffer &&other) noexcept
    : sample_rate_(std::exchange(other.sample_rate_, 0)), storage_(std::move(other.storage_)),
      frames_(std::exchange(other.frames_, 0)), dropped_(std::exchange(other.dropped_, 0)) {}
capture_buffer &capture_buffer::operator=(capture_buffer &&other) noexcept {
	if (this != &other) {
		storage_     = std::move(other.storage_);
		sample_rate_ = std::exchange(other.sample_rate_, 0);
		frames_      = std::exchange(other.frames_, 0);
		dropped_     = std::exchange(other.dropped_, 0);
	}
	return *this;
}
ice::expected<capture_buffer, diagnostic> capture_buffer::create(std::uint32_t sample_rate,
                                                                 std::uint32_t duration_ms) {
	if (sample_rate < 8000 || sample_rate > 192000 || !duration_ms || duration_ms > 10000)
		return ice::unexpected(diagnostic{"PCM capture requires 8000..192000 Hz and 1..10000 ms"});
	const auto frames = std::size_t(std::uint64_t(sample_rate) * duration_ms / 1000);
	try {
		return capture_buffer(sample_rate, std::vector<std::int16_t>(frames));
	} catch (const std::bad_alloc &) {
		return ice::unexpected(diagnostic{"Cannot allocate bounded PCM capture storage"});
	}
}
std::size_t capture_buffer::append(std::span<const std::int16_t> samples) noexcept {
	const auto accepted = std::min(samples.size(), storage_.size() - frames_);
	std::copy_n(samples.begin(), accepted, storage_.begin() + static_cast<std::ptrdiff_t>(frames_));
	frames_ += accepted;
	const auto rejected = std::uint64_t(samples.size() - accepted);
	dropped_ += std::min(rejected, std::numeric_limits<std::uint64_t>::max() - dropped_);
	return accepted;
}
void capture_buffer::reset() noexcept {
	std::fill(storage_.begin(), storage_.end(), 0);
	frames_  = 0;
	dropped_ = 0;
}
std::span<const std::int16_t> capture_buffer::samples() const { return {storage_.data(), frames_}; }
std::uint32_t                 capture_buffer::sample_rate() const { return sample_rate_; }
std::size_t                   capture_buffer::capacity_frames() const { return storage_.size(); }
std::uint64_t                 capture_buffer::dropped_frames() const { return dropped_; }
bool                          capture_buffer::full() const { return frames_ == storage_.size(); }
ice::expected<std::vector<std::byte>, diagnostic> encode_wav(const capture_buffer &capture) {
	if (capture.sample_rate() < 8000 || capture.sample_rate() > 192000)
		return ice::unexpected(diagnostic{"Cannot encode an unavailable/moved-from PCM buffer"});
	try {
		const auto bytes = std::uint32_t(capture.samples().size() * sizeof(std::int16_t));
		std::vector<std::byte> output(44 + bytes);
		auto                   text = [&](std::size_t offset, const char *value) {
            for (std::size_t index = 0; index < 4; ++index)
                output[offset + index] = std::byte(value[index]);
		};
		auto number = [&](std::size_t offset, std::uint32_t value, unsigned size) {
			for (unsigned index = 0; index < size; ++index)
				output[offset + index] = std::byte((value >> (8 * index)) & 255);
		};
		text(0, "RIFF");
		number(4, 36 + bytes, 4);
		text(8, "WAVE");
		text(12, "fmt ");
		number(16, 16, 4);
		number(20, 1, 2);
		number(22, 1, 2);
		number(24, capture.sample_rate(), 4);
		number(28, capture.sample_rate() * 2, 4);
		number(32, 2, 2);
		number(34, 16, 2);
		text(36, "data");
		number(40, bytes, 4);
		for (std::size_t index = 0; index < capture.samples().size(); ++index)
			number(44 + index * 2, static_cast<std::uint16_t>(capture.samples()[index]), 2);
		return output;
	} catch (const std::bad_alloc &) {
		return ice::unexpected(diagnostic{"Cannot allocate bounded WAV output"});
	}
}
} // namespace ice::audio
