#pragma once
#include "ice/audio/capture.h"
#include <span>

namespace ice::audio {
/// Interleaved mono/stereo PCM. Floating samples are normalized and finite in [-1,1].
struct pcm_data {
	/// Source frequency, without relabeling or resampling decoded frames.
	std::uint32_t sample_rate = 48000;
	/// One or two interleaved channels.
	std::uint32_t channels = 1;
	/// Owned normalized samples; frames equal samples.size()/channels.
	std::vector<float> samples;
};
/// Configurable decoding budgets, checked before allocating sample storage.
struct wav_limits {
	/// Maximum complete encoded document size; default 512 MiB.
	std::size_t max_encoded_bytes = 512u * 1024u * 1024u;
	/// Maximum decoded float sample storage; default 512 MiB.
	std::size_t max_pcm_bytes = 512u * 1024u * 1024u;
};
/// Decode owned PCM from bounded RIFF/WAVE bytes. Supports mono/stereo PCM16/24/32 and
/// IEEE float32, 8000..192000 Hz, unknown ancillary chunks and odd chunk padding. Rejects
/// truncated/duplicate format or data chunks, extensible/compressed formats, nonfinite or
/// out-of-range float samples. No native device, file I/O, resampling or network access.
ice::expected<pcm_data, diagnostic> decode_wav(std::span<const std::byte> bytes,
                                               wav_limits                 limits = {});
} // namespace ice::audio
