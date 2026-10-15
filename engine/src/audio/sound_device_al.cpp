namespace ice::audio {} // namespace ice::audio
#include "ice/audio/sound_al.h"

#include "ice/debug.h"
#include "ice/memory/min_heap.h"
namespace ice::audio {} // namespace ice::audio
#include <string.h>
namespace ice::audio {

/*! c'tor
 **/
sound_device_al::sound_device_al(void) {
	context_     = NULL;
	device_      = NULL;
	initialized_ = false;
} // END FUNCTINO icSoundDeviceAL(void)

/*! d'tor
 **/
sound_device_al::~sound_device_al(void) { cleanup(); }

/*! Frees all memory allocated by the sound device
 *
 *  @returns        ICRESULT        Success/failure of the cleanup
 **/
ice::result sound_device_al::cleanup(void) {
	if (context_) {
		alcMakeContextCurrent(context_);
		for (auto &sound : owned_sounds_) {
			if (!sound)
				continue;
			sound->stop();
			alDeleteSources(1, &sound->a_lsource_);
			sound->a_lsource_   = 0;
			sound->initialized_ = false;
			delete sound;
			sound = nullptr;
		}
		for (int i = 0; i < ICSND_MAX_SOURCE; ++i) {
			if (src_bufs_[i].buff_id)
				alDeleteBuffers(1, &src_bufs_[i].buff_id);
			free(src_bufs_[i].data);
		}
		alcMakeContextCurrent(NULL);
		alcDestroyContext(context_);
		context_ = NULL;
	}
	if (device_) {
		alcCloseDevice(device_);
		device_ = NULL;
	}
	for (auto &buffer : src_bufs_)
		buffer = sound_buffer{};
	memset(sounds_, 0, sizeof(sounds_));
	memset(playing_, 0, sizeof(playing_));
	num_src_ = num_free_sounds_ = num_playing_ = 0;
	initialized_                               = false;
	return IC_OK;
}

/*! Initializes the sound device
 *
 *  @returns        ICRESULT        Success/failure of initializing the device
 **/
ice::result sound_device_al::initialize(void) {
	ice::result result_code = cleanup();
	if (ICEFAIL(result_code)) {
		ALenum err = alGetError();
		ice::warningf("icSoundDeviceAL::Initialize failed with error: %i", err);
		return result_code;
	}

	device_ = alcOpenDevice(NULL);
	if (NULL == device_) {
		ALenum err = alGetError();
		ice::warningf("icSoundDeviceAL::Initialize failed with error: %i", err);
		return IC_FAIL_GEN;
	}

	context_ = alcCreateContext(device_, NULL);

	if (!alcMakeContextCurrent(context_)) {
		ALenum err = alGetError();
		ice::warningf("icSoundDeviceAL::Initialize failed with error: %i", err);
		return IC_FAIL_GEN;
	}

	ALenum err = alGetError();

	if (err) {
		ALenum err = alGetError();
		ice::warningf("icSoundDeviceAL::Initialize failed with error: %i", err);
		return IC_FAIL_GEN;
	}

	ALuint temp_sounds[ICSND_MAX_PLAYING];
	ALuint temp_buffers[ICSND_MAX_SOURCE];
	memset(temp_sounds, 0, sizeof(ALuint) * ICSND_MAX_PLAYING);
	memset(temp_buffers, 0, sizeof(ALuint) * ICSND_MAX_SOURCE);

	// create the sources
	alGenSources(ICSND_MAX_PLAYING, temp_sounds);

	err = alGetError();
	if (err) {
		ALenum err = alGetError();
		ice::warningf("icSoundDeviceAL::Initialize failed with error: %i", err);
		return IC_FAIL_GEN;
	}

	// create the buffers
	alGenBuffers(ICSND_MAX_SOURCE, temp_buffers);

	err = alGetError();
	if (err) {
		ALenum err = alGetError();
		ice::warningf("icSoundDeviceAL::Initialize failed with error: %i", err);
		return IC_FAIL_GEN;
	}

	for (int i = 0; i < ICSND_MAX_SOURCE; ++i)
		src_bufs_[i].buff_id = temp_buffers[i];

	for (int i = 0; i < ICSND_MAX_PLAYING; ++i) {
		sounds_[i]       = new sound_al();
		owned_sounds_[i] = (sound_al *)sounds_[i];
		num_free_sounds_++;
		sound_al *temp     = (sound_al *)sounds_[i];
		temp->a_lsource_   = temp_sounds[i];
		temp->initialized_ = true;
	}

	ice::memory::min_heap_build(sounds_, num_free_sounds_);

	// TODO 3d stuff
	// alDopplerFactor(ALfloat factor);
	// alDopplerVelocity(ALfloat velocity);

	initialized_ = true;
	return IC_OK;
}

/*! Call this to unload a sound object from memory
 *
 *  @param          file          The file to unload
 *  @returns        ICRESULT        Success/failure of unloading the resource
 **/
ice::result sound_device_al::load_source(const char *file) {
	sound_buffer buf;

	if (ICEFAIL(create_source(file, buf)))
		return IC_FAIL_GEN;

	ALenum err = AL_NO_ERROR;

	err = alGetError();

	ALenum al_format;

	switch (buf.format) {
	case icsnd_fmt_wav_mono16:
		al_format = AL_FORMAT_MONO16;
		break;
	case icsnd_fmt_wav_stereo16:
		al_format = AL_FORMAT_STEREO16;
		break;
	case snd_fmt_wav_mono8:
		al_format = AL_FORMAT_MONO8;
		break;
	case snd_fmt_wav_stereo8:
		al_format = AL_FORMAT_STEREO8;
		break;
	default:
		return IC_FAIL_GEN;
	}

	ALsizei al_freq = (ALsizei)buf.freq;
	ALsizei al_size = (ALsizei)buf.size;

	// put the data into an openAL buffer
	alBufferData(buf.buff_id, al_format, buf.data, al_size, al_freq);

	err = alGetError();
	if (err != AL_NO_ERROR)
		return IC_FAIL_GEN;

	return IC_OK;
}

/*! Call this to unload a sound object from memory
 *
 *  @param          file          The file to unload
 *  @returns        ICRESULT        Success/failure of unloading the resource
 **/
ice::result sound_device_al::unload_source(const char *file [[maybe_unused]]) {

	// release the data
	// alutUnloadWAV(alFormatBuffer, alBuffer, alBufferLen, alFreqBuffer);
	return IC_FAIL_GEN;
}

/*! This should be called by the main thread every frame
 *
 *  @param              delta_seconds      Elapsed time since last frame
 *  @returns            ICRESULT        Success/failure of update
 **/
ice::result sound_device_al::update(const float delta_seconds [[maybe_unused]]) {
	alListenerfv(AL_POSITION, glm::value_ptr(listener_.pos));
	alListenerfv(AL_VELOCITY, glm::value_ptr(listener_.vel));
	const float orientation[] = {listener_.at.x, listener_.at.y, listener_.at.z,
	                             listener_.up.x, listener_.up.y, listener_.up.z};
	alListenerfv(AL_ORIENTATION, orientation);

	// loop through and get this shit
	//    alGetSourcei( uiSource, AL_SOURCE_STATE, &iState);
	//} while (iState == AL_PLAYING);

	return IC_OK;
}

/*! Gets a handle to a sound for playing
 *
 *  @param          name          Audio file to play
 *  @param[out]     out_sound        Handle to the sound object
 *  @returns        ICRESULT        Success/failure of playing audio
 **/
ice::result sound_device_al::get_sound(const char *name, sound_i **out_sound) {
	sound_i *sound = get_free_sound();
	if (!sound)
		return IC_FAIL_GEN;

	sound_buffer buf;

	if (ICEFAIL(find_source(name, buf))) {
		if (ICEFAIL(load_source(name))) {
			return IC_FAIL_GEN;
		}

		if (ICEFAIL(find_source(name, buf))) {
			return IC_FAIL_GEN;
		}
	}

	sound_al *al_sound = (sound_al *)sound;

	al_sound->buf_ = buf;

	// assign the buffer to this source
	alSourcei(al_sound->a_lsource_, AL_BUFFER, buf.buff_id);

	ALenum err = alGetError();
	if (err != AL_NO_ERROR)
		return IC_FAIL_GEN;

	*out_sound = sound;

	return IC_OK;
}

/*! Used to play an ambient sound
 *
 *  Note, if you call this on a file that has not been Loaded
 *  (see icSoundDeviceAL::LoadSource), it will keep this audio
 *  loaded in memory until you call UnloadSource or the device
 *  is destroyed.
 *
 *  @param          name          Audio file to play
 *  @param          sound_params      How to play that audio
 *  @param[out]     out_sound        Handle to the sound object
 *  @returns        ICRESULT        Success/failure of playing audio
 **/
ice::result sound_device_al::play(const char *name, const sound_param &sound_params,
                                  sound_i **out_sound) {
	ALenum err = AL_NO_ERROR;

	err = alGetError();

	sound_i *sound = get_free_sound();
	if (!sound)
		return IC_FAIL_GEN;

	sound_buffer buf;

	if (ICEFAIL(find_source(name, buf)))
		load_source(name);

	if (ICEFAIL(find_source(name, buf)))
		return IC_FAIL_GEN;

	sound_al *al_sound = (sound_al *)sound;

	al_sound->buf_    = buf;
	al_sound->params_ = sound_params;

	// assign the buffer to this source
	alSourcei(al_sound->a_lsource_, AL_BUFFER, buf.buff_id);

	err = alGetError();
	if (err != AL_NO_ERROR)
		return IC_FAIL_GEN;

	al_sound->play();

	*out_sound = sound;

	return IC_OK;
}
//                   const icSoundParam& soundParams,
//                   icSoundI** ppSound)

/*! Used to play a 3d sound
 *
 *  Note, if you call this on a file that has not been Loaded
 *  (see icSoundDeviceAL::LoadSource), it will keep this audio
 *  loaded in memory until you call UnloadSource or the device
 *  is destroyed.
 *
 *  @param          name          Audio file to play
 *  @param          sound_params      How to play that audio
 *  @param[out]     out_sound        Handle to the sound object
 *  @returns        ICRESULT        Success/failure of playing audio
 **/
ice::result sound_device_al::play3_d(const char *name, const sound_param &sound_params,
                                     sound_i **out_sound) {
	sound_i *sound = get_free_sound();
	if (!sound)
		return IC_FAIL_GEN;

	sound_buffer buf;

	if (ICEFAIL(find_source(name, buf)))
		load_source(name);

	if (ICEFAIL(find_source(name, buf)))
		return IC_FAIL_GEN;

	sound_al *al_sound = (sound_al *)sound;

	al_sound->buf_ = buf;

	al_sound->params_ = sound_params;

	// assign the buffer to this source
	alSourcei(al_sound->a_lsource_, AL_BUFFER, buf.buff_id);

	al_sound->play3d();

	*out_sound = sound;

	return IC_OK;
}
//                     const icSoundParam& soundParams,
//                     icSoundI** ppSound)

/*! Used to play ambient streaming audio
 *
 *  @param          file          Audio file to stream
 *  @param          sound_params      How to play that stream
 *  @param[out]     stream        Handle to the stream
 *  @returns        ICRESULT        Success/failure of starting stream
 **/
ice::result sound_device_al::play_stream(const char        *file [[maybe_unused]],
                                         const sound_param &sound_params [[maybe_unused]],
                                         sound_stream_i   **stream [[maybe_unused]]) {
	return IC_FAIL_GEN;
}
//                         const icSoundParam& soundParams,
//                         icSoundStreamI** ppStream)

/*! Used to play streaming audio that has spatial properties
 *
 *  @param          file          Audio file to stream
 *  @param          sound_params      How to play that stream
 *  @param[out]     stream        Handle to the stream
 *  @returns        ICRESULT        Success/failure of starting stream
 **/
ice::result sound_device_al::play_stream3_d(const char        *file [[maybe_unused]],
                                            const sound_param &sound_params [[maybe_unused]],
                                            sound_stream_i   **stream [[maybe_unused]]) {
	return IC_FAIL_GEN;
}
//                           const icSoundParam& soundParams,
//                           icSoundStreamI** ppStream)

/*! Releases a sound object for reuse
 *
 *  @param          sound          Pointer to a sound object
 *  @returns        ICRESULT        Success/failure of releasing sound
 **/
ice::result sound_device_al::release_sound(sound_i *sound) {
	sound_al *alsound = (sound_al *)sound;
	alsound->stop();
	// TODO: zero this!
	// alsound->m_buf
	return free_sound(sound);
}

/*! Releases a audio stream for reuse
 *
 *  @param          stream         Pointer to stream object
 *  @returns        ICRESULT        Success/failure of releasing stream
 **/
ice::result sound_device_al::release_stream(sound_stream_i *stream) {
	sound_stream_al *alstream = (sound_stream_al *)stream;
	alstream->stop();
	return free_stream(stream);
}
} // namespace ice::audio
