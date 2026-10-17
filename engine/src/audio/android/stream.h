#pragma once
#include "ice/audio/speech.h"
#include <array>
#include <mutex>

namespace ice::audio::android_detail {
// Exact Android AudioFormat constants; unsupported/compressed formats are never relabeled.
enum class encoding : std::int32_t { pcm_s16 = 2, pcm_u8 = 3, pcm_f32 = 4 };
// Preserve every code point and source character; never split a UTF-16 surrogate pair.
ice::expected<std::vector<std::u16string>, diagnostic> segments(std::string_view text,
                                                                std::size_t maximum_utf16_units);
// Serialized per logical utterance. Only weak speech_output callbacks escape this object.
class stream {
public:
	ice::expected<std::uint64_t, diagnostic> start(speech_output output);
	bool begin(std::uint64_t generation, std::uint32_t rate, encoding format,
	           std::uint32_t channels) noexcept;
	bool audio(std::uint64_t generation, std::span<const std::byte> bytes) noexcept;
	bool done(std::uint64_t generation, bool final_segment) noexcept;
	void fail(std::uint64_t generation, std::string_view message, std::int32_t code) noexcept;
	void cancel() noexcept;

private:
	void                     fault(std::string_view message) noexcept;
	std::mutex               mutex_;
	speech_output            output_;
	std::uint64_t            generation_ = 0;
	std::uint32_t            rate_ = 0, channels_ = 0;
	encoding                 format_ = encoding::pcm_s16;
	std::array<std::byte, 8> carry_{};
	std::size_t              carried_ = 0, sample_bytes_ = 0;
	bool                     active_ = false;
};
} // namespace ice::audio::android_detail
