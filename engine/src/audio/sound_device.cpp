namespace ice::audio {} // namespace ice::audio
#include "ice/audio/sound.h"
#include "ice/memory/min_heap.h"
namespace ice::audio {} // namespace ice::audio
#include <stdlib.h>
#include <string.h>
namespace ice::audio {

bool load_wave_file(const char *fname, sound_buffer *header) {
	FILE *fp;
#ifdef WIN32
	fopen_s(&fp, fname, "rb");
#else
	fp = fopen(fname, "rb");
#endif
	if (!fp)
		return false;

	snd_wav_header head;

	fread(&head, sizeof(snd_wav_header), 1, fp);

	header->freq = head.sample_rate;
	if (head.num_channels == 1) {
		if (head.bits_per_samp == 8)
			header->format = snd_fmt_wav_mono8;
		else if (head.bits_per_samp == 16)
			header->format = icsnd_fmt_wav_mono16;
		else {
			ice::warningf("ICE Audio does not support the format of: %s", fname);
			return false;
		}
	} else if (head.num_channels == 2) {
		if (head.bits_per_samp == 8)
			header->format = snd_fmt_wav_stereo8;
		else if (head.bits_per_samp == 16)
			header->format = icsnd_fmt_wav_stereo16;
		else {
			ice::warningf("ICE Audio does not support the format of: %s", fname);
			return false;
		}
	} else {
		ice::warningf("ICE Audio does not support the format of: %s", fname);
		return false;
	}

	header->data = malloc(head.subchunk2_size);
	header->size = head.subchunk2_size;

	fread(header->data, head.subchunk2_size, 1, fp);

	fclose(fp);

	return true;
}

/*! c'tor
 **/
sound_device_i::sound_device_i(void) {
	listener_ = {};
	for (auto &buffer : src_bufs_)
		buffer = sound_buffer{};
	memset(sounds_, 0, sizeof(sound_i *) * ICSND_MAX_PLAYING);
	memset(playing_, 0, sizeof(sound_i *) * ICSND_MAX_PLAYING);
	memset(streams_, 0, sizeof(sound_stream_i *) * ICSND_MAX_STREAMS);

	num_playing_      = 0; //! Num playing sounds
	num_free_sounds_  = 0; //! Num free sounds
	num_streaming_    = 0; //! Num playing streams
	num_free_streams_ = 0; //! Num free streams
	num_src_          = 0; //! Num src buffs loaded
} // END FUNCTINO icSoundDeviceI(void)

/*! Creates a source buffer
 *
 *  @param      file                File to load
 *  @returns    buffer                Reference to buffer object to fill
 *  @returns    ICRESULT              Pointer to sound buffer (NULL if failed)
 **/
ice::result sound_device_i::create_source(const char *file, sound_buffer &buffer) {
	// we don't allow double loading!
	if (ICEOK(find_source(file, buffer)))
		return IC_FAIL_GEN;

	if (num_src_ < ICSND_MAX_SOURCE - 1) {
		sound_buffer temp = src_bufs_[num_src_];
		temp.name         = file;

		// LOAD THE SOUND BITS
		if (load_wave_file(file, &temp)) {
			// insert the sound buffer into the list
			ice::memory::min_heap_insert(src_bufs_, num_src_++, temp);

			int i  = ice::memory::min_heap_find(src_bufs_, num_src_, temp);
			buffer = src_bufs_[i];
			return IC_OK;
		}
	}
	return IC_FAIL_GEN;
}

/*! Finds a source buffer by name
 *
 *  @param      file              Name of source to find
 *  @returns    buffer                Reference to buffer object to fill
 *  @returns    ICRESULT              Pointer to sound buffer (NULL if failed)
 **/
ice::result sound_device_i::find_source(const char *file, sound_buffer &buffer) {
	sound_buffer temp;
	temp.name = file;
	int i     = ice::memory::min_heap_find(src_bufs_, num_src_, temp);
	if (i >= 0) {
		buffer = src_bufs_[i];
		return IC_OK;
	}

	return IC_FAIL_GEN;
}

/*! Returns a sound that is free to be played (if any)
 *
 *  @returns    icSound*            Pointer to sound (NULL if none)
 **/
sound_i *sound_device_i::get_free_sound(void) {
	if (num_free_sounds_ > 0 && num_playing_ < ICSND_MAX_PLAYING) {
		sound_i *sound = ice::memory::min_heap_dequeue(sounds_, num_free_sounds_--);
		ice::memory::min_heap_insert(playing_, num_playing_++, sound);
		return sound;
	}
	return NULL;
}

/*! Frees a sound object that is no longer playing
 *
 *  @param      sound              Pointer to sound object
 *  @returns    ICRSESULT           Success/failure of freeing sound
 **/
ice::result sound_device_i::free_sound(sound_i *sound) {
	int i = ice::memory::min_heap_find(playing_, num_playing_, sound);
	if (i >= 0) {
		ice::memory::min_heap_decrease_key(playing_, i, (sound_i *)0);
		ice::memory::min_heap_dequeue(playing_, num_playing_--);
		ice::memory::min_heap_insert(sounds_, num_free_sounds_++, sound);
		return IC_OK;
	}
	return IC_FAIL_GEN;
}

/*! Returns a stream that is free to be played (if any)
 *
 *  @returns    icSoundStreamI*     Pointer to stream object (NULL if none)
 **/
sound_stream_i *sound_device_i::get_free_stream(void) { return NULL; }

/*! Frees a stream object that is no longer playing
 *
 *  @param      stream             Pointer to stream object
 *  @returns    ICRESULT            Success/failure of freeing stream
 **/
ice::result sound_device_i::free_stream(sound_stream_i *stream [[maybe_unused]]) {
	return IC_FAIL_GEN;
}
} // namespace ice::audio
