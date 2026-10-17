#include "ice/audio/wav.h"
#include <bit>
#include <cmath>
#include <cstdio>
#include <limits>

#define CHECK(expression)                                                                          \
	do {                                                                                           \
		if (!(expression)) {                                                                       \
			std::fprintf(stderr, "WAVE %d: %s\n", __LINE__, #expression);                          \
			return 1;                                                                              \
		}                                                                                          \
	} while (false)
namespace {
using bytes = std::vector<std::byte>;
void word(bytes &data, std::uint32_t value, unsigned count) {
	while (count--) {
		data.push_back(std::byte(value & 255));
		value >>= 8;
	}
}
void text(bytes &data, const char *value) {
	for (unsigned index = 0; index < 4; ++index)
		data.push_back(std::byte(value[index]));
}
void chunk(bytes &target, const char *name, const bytes &payload) {
	text(target, name);
	word(target, static_cast<std::uint32_t>(payload.size()), 4);
	target.insert(target.end(), payload.begin(), payload.end());
	if (payload.size() & 1)
		target.push_back(std::byte{0});
}
void fix_length(bytes &data) {
	const auto size = static_cast<std::uint32_t>(data.size() - 8);
	for (unsigned index = 0; index < 4; ++index)
		data[index + 4] = std::byte((size >> (index * 8)) & 255);
}
bytes wave(unsigned encoding, unsigned bits, unsigned channels, const bytes &pcm,
           bool ancillary = false) {
	bytes result;
	text(result, "RIFF");
	word(result, 0, 4);
	text(result, "WAVE");
	bytes format;
	word(format, encoding, 2);
	word(format, channels, 2);
	word(format, 48000, 4);
	word(format, 48000 * channels * bits / 8, 4);
	word(format, channels * bits / 8, 2);
	word(format, bits, 2);
	chunk(result, "fmt ", format);
	if (ancillary)
		chunk(result, "JUNK", {std::byte{7}, std::byte{8}, std::byte{9}});
	chunk(result, "data", pcm);
	fix_length(result);
	return result;
}
} // namespace
int main() {
	using namespace ice::audio;
	for (auto bits : {16u, 24u, 32u}) {
		bytes pcm;
		word(pcm, 1u << (bits - 1), bits / 8);
		word(pcm, 0, bits / 8);
		word(pcm, (1u << (bits - 1)) - 1, bits / 8);
		auto encoded = wave(1, bits, 1, pcm, true);
		auto decoded = decode_wav(encoded);
		CHECK(decoded && decoded->sample_rate == 48000 && decoded->channels == 1);
		CHECK(decoded->samples.size() == 3 && decoded->samples[0] == -1 &&
		      decoded->samples[1] == 0);
		CHECK(std::abs(decoded->samples[2] - 1) < .0001f);
		for (std::size_t length = 0; length < encoded.size(); ++length) {
			CHECK(!decode_wav(std::span(encoded).first(length)));
			// Preserve a valid RIFF envelope too, so truncation reaches chunk and
			// padding checks instead of stopping only at the document-size guard.
			if (length >= 12) {
				bytes truncated(encoded.begin(), encoded.begin() + length);
				fix_length(truncated);
				CHECK(!decode_wav(truncated));
			}
		}
		CHECK(!decode_wav(encoded, {encoded.size() - 1, 512}));
		CHECK(!decode_wav(encoded, {512, 8}));
	}
	bytes pcm;
	for (float value : {-.5f, .25f, -0.f, 1.f})
		word(pcm, std::bit_cast<std::uint32_t>(value), 4);
	auto encoded = wave(3, 32, 2, pcm);
	auto decoded = decode_wav(encoded);
	CHECK(decoded && decoded->channels == 2 && decoded->samples[0] == -.5f &&
	      decoded->samples[1] == .25f);
	CHECK(std::signbit(decoded->samples[2]));
	for (float value :
	     {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), 2.f}) {
		bytes invalid;
		word(invalid, std::bit_cast<std::uint32_t>(value), 4);
		CHECK(!decode_wav(wave(3, 32, 1, invalid)));
	}
	CHECK(!decode_wav(wave(1, 8, 1, {std::byte{1}})));
	CHECK(!decode_wav(wave(1, 16, 3, {})));
	CHECK(!decode_wav(wave(1, 16, 2, {std::byte{0}, std::byte{0}})));
	CHECK(!decode_wav(wave(0xfffe, 32, 1, pcm)));
	auto wrong_rate = encoded;
	wrong_rate[28] ^= std::byte{1};
	CHECK(!decode_wav(wrong_rate));
	auto duplicate = encoded;
	chunk(duplicate, "data", pcm);
	fix_length(duplicate);
	CHECK(!decode_wav(duplicate));
	duplicate = encoded;
	chunk(duplicate, "fmt ", bytes(encoded.begin() + 20, encoded.begin() + 36));
	fix_length(duplicate);
	CHECK(!decode_wav(duplicate));
	auto extended = encoded;
	extended.insert(extended.begin() + 36, 2, std::byte{0});
	extended[16] = std::byte{18};
	fix_length(extended);
	CHECK(decode_wav(extended));
	extended[36] = std::byte{1};
	CHECK(!decode_wav(extended));
	CHECK(decode_wav(wave(1, 16, 1, {})));
	std::puts("WAVE passed: independent PCM16/24/32/float reference samples, padding, bounds and "
	          "malformed inputs");
}
