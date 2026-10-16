#include "ice/audio/capture.h"
#include <array>
#include <cstdio>
#include <limits>
#include <utility>

#define CHECK(expression)                                                                          \
	do {                                                                                           \
		if (!(expression)) {                                                                       \
			std::fprintf(stderr, "Audio failed %d: %s\n", __LINE__, #expression);                  \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

int main() {
	using namespace ice::audio;
	for (auto rate : {0u, 7999u, 192001u, std::numeric_limits<std::uint32_t>::max()})
		CHECK(!capture_buffer::create(rate));
	for (auto duration : {0u, 10001u, std::numeric_limits<std::uint32_t>::max()})
		CHECK(!capture_buffer::create(48000, duration));
	auto capture = capture_buffer::create(8000, 1);
	CHECK(capture && capture->capacity_frames() == 8 && capture->sample_rate() == 8000);
	CHECK(capture->samples().empty() && !capture->full() && capture->dropped_frames() == 0);
	const std::array<std::int16_t, 5> first{-32768, -1, 0, 1, 32767};
	CHECK(capture->append(first) == 5);
	CHECK(capture->samples().size() == 5 && capture->samples()[0] == -32768);
	const std::array<std::int16_t, 5> second{123, -123, 42, 43, 44};
	CHECK(capture->append(second) == 3 && capture->full() && capture->dropped_frames() == 2);
	CHECK(capture->append(first) == 0 && capture->dropped_frames() == 7);
	CHECK(capture->append({}) == 0 && capture->samples()[7] == 42);
	auto wav = encode_wav(*capture);
	CHECK(wav && wav->size() == 60);
	auto byte = [&](std::size_t index) { return std::to_integer<unsigned>((*wav)[index]); };
	CHECK(byte(0) == 'R' && byte(3) == 'F' && byte(8) == 'W' && byte(12) == 'f');
	CHECK(byte(4) == 52 && byte(16) == 16 && byte(20) == 1 && byte(22) == 1);
	CHECK(byte(24) == 0x40 && byte(25) == 0x1f && byte(28) == 0x80 && byte(29) == 0x3e);
	CHECK(byte(32) == 2 && byte(34) == 16 && byte(40) == 16);
	CHECK(byte(44) == 0 && byte(45) == 0x80 && byte(46) == 0xff && byte(47) == 0xff);
	CHECK(byte(52) == 0xff && byte(53) == 0x7f && byte(58) == 42 && byte(59) == 0);
	capture->reset();
	CHECK(capture->samples().empty() && !capture->full() && capture->dropped_frames() == 0);
	CHECK(encode_wav(*capture)->size() == 44);
	auto moved = std::move(*capture);
	CHECK(capture->samples().empty() && capture->sample_rate() == 0 && !encode_wav(*capture));
	CHECK(moved.sample_rate() == 8000 && moved.append(first) == 5);
	*capture = std::move(moved);
	// The empty moved-from query is an explicit public ownership guarantee.
	// NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
	CHECK(capture->samples().size() == 5 && moved.samples().empty() && moved.sample_rate() == 0);
	for (auto rate : {8000u, 44100u, 48000u, 192000u}) {
		auto bounded = capture_buffer::create(rate);
		CHECK(bounded && bounded->capacity_frames() == std::size_t(rate) * 10);
		CHECK(bounded->append(first) == 5 && bounded->samples()[4] == 32767);
	}
	std::puts("Audio passed: bounded PCM16 retention/overflow/reset and exact WAV byte layout");
	return 0;
}
