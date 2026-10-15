#pragma once
namespace ice::audio {

/// Serialized WAV container/sample metadata; byte fields describe file layout rather than host
/// structures.
struct snd_wav_header {
	/// Serialized WAV container chunk identifier.
	int chunk_id;
	/// Serialized container byte count.
	ice::uint chunk_size;
	/// Format/token interpreted by the enclosing API.
	ice::uint format;
	/// Serialized WAV format-chunk identifier.
	ice::uint subchunk1_id;
	/// Format-chunk payload size in bytes.
	ice::uint subchunk1_size;
	/// Serialized WAV audio encoding code.
	ice::ushort audio_format;
	/// Number of audio channels.
	ice::ushort num_channels;
	/// Audio samples per second per channel.
	ice::uint sample_rate;
	/// Serialized audio bytes per second.
	ice::uint byte_rate;
	/// Bytes per complete interleaved audio sample frame.
	ice::ushort block_align;
	/// Bits per audio sample component.
	ice::ushort bits_per_samp;
	/// Serialized WAV data-chunk identifier.
	ice::uint subchunk2_id;
	/// Audio data payload size in bytes.
	ice::uint subchunk2_size;
};

} // namespace ice::audio
