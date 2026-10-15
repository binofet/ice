
#include "ice/audio/sound_al.h"
namespace ice::audio {

/*! c'tor
 **/
sound_al::sound_al(void) {
	// TODO: zero this
	// m_buf = NULL;

	playing_     = false;
	b3d_         = false;
	paused_      = false;
	initialized_ = false;
}

/*! d'tor
 **/
sound_al::~sound_al(void) {}

/*! Sets the playback parameters for this sound
 *
 *  @param      params      The new parameters for this sound
 *  @returns    ICRESULT    Success/failure of changing params
 **/
ice::result sound_al::set_params(const sound_param &params) {
	params_       = params;
	params_.pitch = ice::math::clamp(params_.pitch, 0.0f, 1.0f);
	params_.vol   = ice::math::clamp(params_.vol, 0.0f, 1.0f);
	if (initialized_) {
		alSourcef(a_lsource_, AL_PITCH, params_.pitch);
		alSourcef(a_lsource_, AL_GAIN, params_.vol);
		alSourcei(a_lsource_, AL_LOOPING, params_.loop);

		if (b3d_) {
			alSourcefv(a_lsource_, AL_POSITION, glm::value_ptr(params_.pos));
			alSourcefv(a_lsource_, AL_VELOCITY, glm::value_ptr(params_.vel));
		}
	}
	return IC_OK;
}

/*! Plays the sound
 *
 *  @returns    ICRESULT     Success/failure of playing sound
 **/
ice::result sound_al::play(void) {
	if (!buf_.data || !initialized_)
		return IC_FAIL_GEN;

	// are we looping?
	if (params_.loop)
		alSourcei(a_lsource_, AL_LOOPING, AL_TRUE);
	else
		alSourcei(a_lsource_, AL_LOOPING, AL_FALSE);

	// play the sound
	alSourcePlay(a_lsource_);

	return IC_OK;
}

/*! Plays the sound in 3d
 *
 *  @returns    ICRESULT     Success/failure of playing sound
 **/
ice::result sound_al::play3d(void) {
	if (!buf_.data || !initialized_)
		return IC_FAIL_GEN;

	// are we looping?
	if (params_.loop)
		alSourcei(a_lsource_, AL_LOOPING, AL_TRUE);

	// set source position
	alSourcefv(a_lsource_, AL_POSITION, glm::value_ptr(params_.pos));

	// set source velocity
	alSourcefv(a_lsource_, AL_VELOCITY, glm::value_ptr(params_.vel));

	// play the sound
	alSourcePlay(a_lsource_);

	return IC_OK;
}

/*! Stops sound from playing
 *
 *  @param      save_pos      Set to true to be able to play from cur pos
 *  @returns    ICRESULT      Success/failure of stopping sound
 **/
ice::result sound_al::stop(bool save_pos) {
	if (playing_) {
		if (save_pos)
			alSourcePause(a_lsource_);
		else
			alSourceStop(a_lsource_);
	}
	return IC_WARN_GEN;
}

/*! Pauses or unpauses a sound
 *
 *  @returns    ICRESULT      Success/failure of pausing/unpausing
 **/
ice::result sound_al::pause(void) {
	if (!paused_) {
		paused_ = true;
		alSourcePause(a_lsource_);
	} else {
		paused_ = false;
		alSourcePlay(a_lsource_);
	}
	return IC_FAIL_GEN;
}
} // namespace ice::audio
