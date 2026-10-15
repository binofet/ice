#pragma once

#include "ice/audio/sound.h"
namespace ice::audio {

#ifdef __APPLE__
} // namespace ice::audio
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
namespace ice::audio {
#else
} // namespace ice::audio
#include <AL/al.h>
#include <AL/alc.h>
namespace ice::audio {
#endif

/*! Implements the sound interface under openAL
 **/
class sound_al : public sound_i {
public:
	virtual ice::result play(void);
	virtual ice::result play3d(void);
	virtual ice::result stop(bool save_pos = false);
	virtual ice::result pause(void);

	virtual ice::result set_params(const sound_param &params);

private:
	sound_al(void);
	virtual ~sound_al(void);

	ALuint a_lsource_;
	bool   initialized_;

	friend class sound_device_al;
};

/*! Implements the sound stream interface under openAL
 **/
class sound_stream_al : public sound_stream_i {
public:
	/// Report whether this audio stream is currently active.
	virtual bool is_streaming(void) = 0;

private:
	virtual ~sound_stream_al(void);

	friend class sound_device_al;
};

/*! Provides an interface to a sound device
 **/
class sound_device_al : public sound_device_i {
public:
	sound_device_al(void);
	virtual ~sound_device_al(void);

	virtual ice::result cleanup(void);

	virtual ice::result initialize(void);

	virtual ice::result load_source(const char *file);
	virtual ice::result unload_source(const char *file);

	virtual ice::result update(const float delta_seconds);

	virtual ice::result get_sound(const char *name, sound_i **out_sound);

	virtual ice::result play(const char *name, const sound_param &sound_params,
	                         sound_i **out_sound);

	virtual ice::result play3_d(const char *name, const sound_param &sound_params,
	                            sound_i **out_sound);

	virtual ice::result play_stream(const char *file, const sound_param &sound_params,
	                                sound_stream_i **stream);

	virtual ice::result play_stream3_d(const char *file, const sound_param &sound_params,
	                                   sound_stream_i **stream);

	virtual ice::result release_sound(sound_i *sound);

	virtual ice::result release_stream(sound_stream_i *stream);

private:
	sound_al *owned_sounds_[ICSND_MAX_PLAYING] = {};
	bool      initialized_;

	ALCcontext *context_;
	ALCdevice  *device_;
};

} // namespace ice::audio
