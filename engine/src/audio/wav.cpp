#include "ice/audio/wav.h"
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>

namespace ice::audio {
namespace {
std::uint16_t u16(std::span<const std::byte> data, std::size_t offset) {
	return std::uint16_t(std::to_integer<unsigned>(data[offset]) |
	                     (std::to_integer<unsigned>(data[offset + 1]) << 8));
}
std::uint32_t u32(std::span<const std::byte> data, std::size_t offset) {
	return std::uint32_t(u16(data, offset)) | (std::uint32_t(u16(data, offset + 2)) << 16);
}
bool tag(std::span<const std::byte> data, std::size_t offset, const char *text) {
	return std::memcmp(data.data() + offset, text, 4) == 0;
}
} // namespace
ice::expected<pcm_data, diagnostic> decode_wav(std::span<const std::byte> bytes,
                                               wav_limits                 limits) {
	if (bytes.size() > limits.max_encoded_bytes || bytes.size() < 12 ||
	    limits.max_pcm_bytes < sizeof(float) || !tag(bytes, 0, "RIFF") || !tag(bytes, 8, "WAVE") ||
	    std::uint64_t(u32(bytes, 4)) + 8 != bytes.size())
		return ice::unexpected(diagnostic{"Invalid/budget-exceeding RIFF/WAVE document"});
	std::size_t format = 0, format_size = 0, samples = 0, sample_bytes = 0;
	bool        have_format = false, have_samples = false;
	for (std::size_t offset = 12; offset < bytes.size();) {
		if (bytes.size() - offset < 8)
			return ice::unexpected(diagnostic{"Truncated WAVE chunk header"});
		const auto length = std::size_t(u32(bytes, offset + 4));
		const auto padded = std::uint64_t(length) + (length & 1u);
		if (padded > bytes.size() - offset - 8)
			return ice::unexpected(diagnostic{"Truncated WAVE chunk/padding"});
		if (tag(bytes, offset, "fmt ")) {
			if (have_format)
				return ice::unexpected(diagnostic{"Duplicate WAVE format chunk"});
			have_format = true;
			format      = offset + 8;
			format_size = length;
		} else if (tag(bytes, offset, "data")) {
			if (have_samples)
				return ice::unexpected(diagnostic{"Duplicate WAVE data chunk"});
			have_samples = true;
			samples      = offset + 8;
			sample_bytes = length;
		}
		offset += 8 + static_cast<std::size_t>(padded);
	}
	if (!have_format || !have_samples ||
	    (format_size != 16 && (format_size != 18 || u16(bytes, format + 16) != 0)))
		return ice::unexpected(diagnostic{"Missing/unsupported WAVE format or data"});
	const auto encoding = u16(bytes, format), channels = u16(bytes, format + 2),
	           bits = u16(bytes, format + 14), alignment = u16(bytes, format + 12);
	const auto rate = u32(bytes, format + 4);
	if ((channels != 1 && channels != 2) || rate < 8000 || rate > 192000 ||
	    !((encoding == 1 && (bits == 16 || bits == 24 || bits == 32)) ||
	      (encoding == 3 && bits == 32)) ||
	    alignment != channels * (bits / 8) ||
	    std::uint64_t(rate) * alignment != u32(bytes, format + 8) || sample_bytes % alignment != 0)
		return ice::unexpected(diagnostic{"Unsupported/inconsistent WAVE PCM format"});
	const auto count = sample_bytes / (bits / 8);
	if (count > limits.max_pcm_bytes / sizeof(float))
		return ice::unexpected(diagnostic{"Decoded WAVE exceeds PCM storage budget"});
	try {
		pcm_data result;
		if (count > result.samples.max_size())
			return ice::unexpected(diagnostic{"Decoded WAVE exceeds native sample container size"});
		result.sample_rate = rate;
		result.channels    = channels;
		result.samples.resize(count);
		for (std::size_t index = 0; index < count; ++index) {
			const auto offset = samples + index * (bits / 8);
			float      value  = 0;
			if (encoding == 3)
				value = std::bit_cast<float>(u32(bytes, offset));
			else if (bits == 16)
				value = float(std::bit_cast<std::int16_t>(u16(bytes, offset))) / 32768.f;
			else if (bits == 24) {
				auto integer =
				    std::int32_t(std::uint32_t(u16(bytes, offset)) |
				                 (std::to_integer<std::uint32_t>(bytes[offset + 2]) << 16));
				if (integer & 0x800000)
					integer -= 0x1000000;
				value = float(integer) / 8388608.f;
			} else
				value = static_cast<float>(double(std::bit_cast<std::int32_t>(u32(bytes, offset))) /
				                           2147483648.0);
			if (!std::isfinite(value) || value < -1 || value > 1)
				return ice::unexpected(
				    diagnostic{"WAVE float sample is not finite normalized PCM"});
			result.samples[index] = value;
		}
		return result;
	} catch (const std::bad_alloc &) {
		return ice::unexpected(diagnostic{"WAVE allocation failed"});
	}
}
} // namespace ice::audio
