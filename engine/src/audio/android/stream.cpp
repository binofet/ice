#include "stream.h"
#include "ice/core/utf8.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace ice::audio::android_detail {
ice::expected<std::vector<std::u16string>, diagnostic> segments(std::string_view text,
                                                                std::size_t maximum_utf16_units) {
	if (text.empty() || !core::valid_utf8_text(text, 16u * 1024u * 1024u) || !maximum_utf16_units ||
	    maximum_utf16_units > 1024u * 1024u)
		return ice::unexpected(diagnostic{"Invalid Android speech text/segment profile"});
	std::vector<std::u16string> result;
	std::u16string              segment;
	segment.reserve(std::min(text.size(), maximum_utf16_units));
	for (std::size_t at = 0; at < text.size();) {
		auto          first = static_cast<unsigned char>(text[at++]);
		std::uint32_t point = first;
		if (first >= 128) {
			const unsigned count = first >= 0xf0 ? 3 : first >= 0xe0 ? 2 : 1;
			point                = first & ((1u << (6 - count)) - 1);
			for (unsigned n = 0; n < count; ++n)
				point = (point << 6) | (static_cast<unsigned char>(text[at++]) & 0x3f);
		}
		const std::size_t units = point >= 0x10000 ? 2 : 1;
		if (units > maximum_utf16_units)
			return ice::unexpected(
			    diagnostic{"Android speech limit cannot hold one Unicode character"});
		if (segment.size() + units > maximum_utf16_units) {
			if (result.size() == 65536)
				return ice::unexpected(diagnostic{"Android speech segment count exhausted"});
			result.push_back(std::move(segment));
			segment = {};
			segment.reserve(std::min(text.size() - at + units, maximum_utf16_units));
		}
		if (units == 1)
			segment.push_back(static_cast<char16_t>(point));
		else {
			point -= 0x10000;
			segment.push_back(static_cast<char16_t>(0xd800 + (point >> 10)));
			segment.push_back(static_cast<char16_t>(0xdc00 + (point & 0x3ff)));
		}
	}
	if (!segment.empty()) {
		if (result.size() == 65536)
			return ice::unexpected(diagnostic{"Android speech segment count exhausted"});
		result.push_back(std::move(segment));
	}
	return result;
}
ice::expected<std::uint64_t, diagnostic> stream::start(speech_output output) {
	std::lock_guard lock(mutex_);
	if (generation_ == std::numeric_limits<std::uint64_t>::max())
		return ice::unexpected(diagnostic{"Android speech generation exhausted"});
	output_ = std::move(output);
	++generation_;
	rate_ = channels_ = 0;
	carried_ = sample_bytes_ = 0;
	active_                  = true;
	return generation_;
}
void stream::fault(std::string_view message) noexcept {
	active_  = false;
	carried_ = 0;
	output_.fail(message);
}
bool stream::begin(std::uint64_t generation, std::uint32_t rate, encoding format,
                   std::uint32_t channels) noexcept {
	try {
		std::lock_guard lock(mutex_);
		if (!active_ || generation != generation_)
			return false;
		if (rate < 8000 || rate > 192000 || (channels != 1 && channels != 2) ||
		    (format != encoding::pcm_u8 && format != encoding::pcm_s16 &&
		     format != encoding::pcm_f32) ||
		    carried_ || (rate_ && (rate_ != rate || channels_ != channels || format_ != format))) {
			fault("Invalid/changing Android speech PCM format");
			return false;
		}
		rate_         = rate;
		channels_     = channels;
		format_       = format;
		sample_bytes_ = format == encoding::pcm_u8 ? 1 : format == encoding::pcm_s16 ? 2 : 4;
		return true;
	} catch (...) {
		return false;
	}
}
bool stream::audio(std::uint64_t generation, std::span<const std::byte> bytes) noexcept {
	try {
		std::lock_guard lock(mutex_);
		if (!active_ || generation != generation_)
			return false;
		if (!rate_ || bytes.size() > 64u * 1024u * 1024u) {
			fault("Android speech PCM arrived without format or exceeded callback budget");
			return false;
		}
		std::array<float, 4096> converted{};
		std::size_t             used = 0;
		for (const auto byte : bytes) {
			carry_[carried_++] = byte;
			if (carried_ != sample_bytes_ * channels_)
				continue;
			for (std::uint32_t channel = 0; channel < channels_; ++channel) {
				const auto offset = channel * sample_bytes_;
				auto       octet  = [&](std::size_t n) {
                    return std::to_integer<std::uint32_t>(carry_[offset + n]);
				};
				float value = 0;
				if (format_ == encoding::pcm_u8)
					value = (float(octet(0)) - 128.f) / 128.f;
				else if (format_ == encoding::pcm_s16) {
					auto bits = static_cast<std::uint16_t>(octet(0) | (octet(1) << 8));
					value     = float(std::bit_cast<std::int16_t>(bits)) / 32768.f;
				} else {
					auto bits = octet(0) | (octet(1) << 8) | (octet(2) << 16) | (octet(3) << 24);
					value     = std::bit_cast<float>(bits);
				}
				if (!std::isfinite(value) || value < -1 || value > 1) {
					fault("Android speech PCM is not finite/normalized");
					return false;
				}
				converted[used++] = value;
			}
			carried_ = 0;
			if (used == converted.size()) {
				if (!output_.append(rate_, channels_, std::span(converted).first(used))) {
					active_ = false;
					return false;
				}
				used = 0;
			}
		}
		if (used && !output_.append(rate_, channels_, std::span(converted).first(used))) {
			active_ = false;
			return false;
		}
		return true;
	} catch (...) {
		return false;
	}
}
bool stream::done(std::uint64_t generation, bool final_segment) noexcept {
	try {
		std::lock_guard lock(mutex_);
		if (!active_ || generation != generation_)
			return false;
		if (carried_ || !rate_) {
			fault("Android speech completed with incomplete PCM");
			return false;
		}
		if (final_segment) {
			active_ = false;
			output_.complete();
		}
		return true;
	} catch (...) {
		return false;
	}
}
void stream::fail(std::uint64_t generation, std::string_view message, std::int32_t code) noexcept {
	try {
		std::lock_guard lock(mutex_);
		if (active_ && generation == generation_) {
			active_  = false;
			carried_ = 0;
			output_.fail(message, code);
		}
	} catch (...) {
	}
}
void stream::cancel() noexcept {
	try {
		std::lock_guard lock(mutex_);
		active_  = false;
		carried_ = 0;
		output_  = {};
	} catch (...) {
	}
}
} // namespace ice::audio::android_detail
