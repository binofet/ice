#pragma once

#include "ice/globals.h"
#include "ice/math/vec3.h"
#include <string>

#include "ice/audio/sound_header.h"
namespace ice::audio {

#define ICSND_MAX_SOURCE (64)  //! Maximum number of source_ buffers
#define ICSND_MAX_PLAYING (16) //! Maximum number of simultenous sounds
#define ICSND_MAX_STREAMS (4)  //! Maximum number of streaming sounds

//! TODO, make this more clear, do we need another one for realtime conversion
//      formats?
/*! Describes a PCM WAV format
 **/
enum snd_fmt {
	icsnd_fmt_uknown = 0, //! Invalid pcm wav format
	snd_fmt_wav_mono8,    //! pcm mono 8 bit sampled audio
	snd_fmt_wav_stereo8,
	icsnd_fmt_wav_mono16,
	icsnd_fmt_wav_stereo16,
	icsnd_fmt_num_formats
};

/*! Wraps up a sound buffer
 **/
struct sound_buffer {
	/// Construct inactive state; initialize explicitly before active use.
	sound_buffer() = default;
	/// Construct with the borrowed dependencies described by this type; initialize explicitly
	/// before active use.
	sound_buffer(const sound_buffer &rhs) {
		name    = rhs.name;
		data    = rhs.data;
		header  = rhs.header;
		format  = rhs.format;
		freq    = rhs.freq;
		size    = rhs.size;
		buff_id = rhs.buff_id;
	};
	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	bool operator<(const sound_buffer &rhs) const { return name < rhs.name; };
	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	bool operator<=(const sound_buffer &rhs) const { return name <= rhs.name; };
	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	bool operator>(const sound_buffer &rhs) const { return name > rhs.name; };
	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	bool operator>=(const sound_buffer &rhs) const { return name >= rhs.name; };
	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	bool operator==(const sound_buffer &rhs) const { return name == rhs.name; };

	/// Assign the supplied value without changing the ownership contract of this object.
	const sound_buffer &operator=(const sound_buffer &rhs) {
		name    = rhs.name;
		data    = rhs.data;
		header  = rhs.header;
		format  = rhs.format;
		freq    = rhs.freq;
		size    = rhs.size;
		buff_id = rhs.buff_id;

		return *this;
	};

	/// Human-readable identifier; the enclosing object owns the string.
	std::string name;                       //! Name of this buffer
	void       *data    = nullptr;          //! Pointer to data
	void       *header  = nullptr;          //! Pointer to header
	snd_fmt     format  = icsnd_fmt_uknown; //! Type of buffer
	int         freq    = 0;                //! Frequency of the audio
	int         size    = 0;                //! Size of the data
	ice::uint   buff_id = 0;                //! ID of the buffer
};

/*! Structure used to store information about a sound object
 **/
struct sound_param {
	/// Position in the coordinate space documented by the owning record.
	ice::math::vec3 pos;   //! Position of the sound
	ice::math::vec3 vel;   //! Velocity of the soun
	float           vol;   //! Volume of the sound (0.0f - 1.0f)
	float           pitch; //! Pitch of the sound (0.0f - 1.0f)
	float           pan;
	/// Repeat playback when the source reaches its end.
	bool loop;   //! Is the sound looping
	bool paused; //! Is the sound paused
};

/*! Structure used to precisely define what a listener is
 *  and how they are moving/oriented in the world.
 **/
struct sound_listener {
	/// Position in the coordinate space documented by the owning record.
	ice::math::vec3 pos; //! Position of listener
	ice::math::vec3 vel; //! Velocity of listener
	ice::math::vec3 at;  //! Orientation of listener (at direction)
	ice::math::vec3 up;  //! Orientation of listener (up direction)
	ice::math::real vol; //! Listeners volume level
};

/// Decode a native WAVE file into the supplied sound buffer; the receiving audio device owns its
/// eventual upload.
bool load_wave_file(const char *fname, sound_buffer *header);

/*! Provides an interface to a sound object
 **/
class sound_i {
public:
	/// Start or resume playback; output voices remain owned by the audio device.
	virtual ice::result play(void) = 0;
	/// Start 3D playback using world-space parameters; release output voices through the device.
	virtual ice::result play3d(void) = 0;
	/// Stop activity and cancel pending input; callers must keep borrowed client/native handles
	/// alive through shutdown.
	virtual ice::result stop(bool save_pos = false) = 0;
	/// Pause playback/state processing without transferring resource ownership.
	virtual ice::result pause(void) = 0;

	/// Apply the supplied parameter snapshot; inspect the result for unsupported/invalid state.
	virtual ice::result set_params(const sound_param &params) = 0;

	/// Copy current parameters into caller storage; inspect status for invalid output/state.
	FORCE_INLINE const sound_param &get_params(void) { return params_; };

protected:
	virtual ~sound_i(void) {};

	sound_param  params_; //! Sound parameters
	sound_buffer buf_;

	bool playing_;
	bool paused_;
	bool b3d_;

	friend class sound_device_i;
};

/*! Provides an interface to a streaming sound
 *      must also implement icSoundI
 **/
class sound_stream_i : public sound_i {
public:
	/// Report whether this audio stream is currently active.
	virtual bool is_streaming(void) = 0;

protected:
	virtual ~sound_stream_i(void) {};
};

/*! Provides an interface to a sound device
 **/
class sound_device_i {
public:
	sound_device_i(void);
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	virtual ~sound_device_i(void) {};

	/// Release resources owned by this subsystem; borrowed dependencies must remain alive until
	/// cleanup completes.
	virtual ice::result cleanup(void) = 0;

	/// Initialize the explicit subsystem state before loading or using its resources.
	virtual ice::result initialize(void) = 0;

	/// Cache an owned audio source from the supplied file; inspect status before requesting a
	/// voice.
	virtual ice::result load_source(const char *file) = 0;
	/// Release an audio source after its voices have been returned.
	virtual ice::result unload_source(const char *file) = 0;

	/// Advance frame-dependent state on the owning thread; elapsed-time arguments are seconds.
	virtual ice::result update(const float delta_seconds) = 0;

	/// Borrow an available device voice for this source through the output pointer; release it
	/// through the device.
	virtual ice::result get_sound(const char *name, sound_i **out_sound) = 0;

	/// Start or resume playback; output voices remain owned by the audio device.
	virtual ice::result play(const char *name, const sound_param &sound_params,
	                         sound_i **out_sound) = 0;

	/// Start 3D playback using world-space parameters; release output voices through the device.
	virtual ice::result play3_d(const char *name, const sound_param &sound_params,
	                            sound_i **out_sound) = 0;

	/// Start streaming playback; unsupported implementations return a failure status.
	virtual ice::result play_stream(const char *file, const sound_param &sound_params,
	                                sound_stream_i **stream) = 0;

	/// Start world-positioned streaming playback; unsupported implementations return a failure
	/// status.
	virtual ice::result play_stream3_d(const char *file, const sound_param &sound_params,
	                                   sound_stream_i **stream) = 0;

	/// Return a borrowed playback voice to the owning device.
	virtual ice::result release_sound(sound_i *sound) = 0;

	/// Return a borrowed streaming voice to the owning device.
	virtual ice::result release_stream(sound_stream_i *stream) = 0;

	/// ACCESS/INQUIRY

	virtual void set_listener(const sound_listener &listener) { listener_ = listener; };

protected:
	/// PROTECTED FUNCTIONS
	ice::result find_source(const char *file, sound_buffer &buffer);
	ice::result create_source(const char *file, sound_buffer &buffer);
	/// ICRESULT CreateSource(const char* szName, int bufsize,
	//                             ICSND_FMT format);

	sound_i    *get_free_sound(void);
	ice::result free_sound(sound_i *sound);

	sound_stream_i *get_free_stream(void);
	ice::result     free_stream(sound_stream_i *stream);

	sound_listener listener_; //! The current listener

	// TODO: this isn't going to work with a heap
	/// basically what will happen is icSoundBuffers will get moved
	/// after icSoundI is already referencing it.
	/// really dangerous, must refactor!
	sound_buffer src_bufs_[ICSND_MAX_SOURCE]; //! Sound data buffers

	sound_i *sounds_[ICSND_MAX_PLAYING];  //! Free sounds
	sound_i *playing_[ICSND_MAX_PLAYING]; //! Playing sounds

	sound_stream_i *streams_[ICSND_MAX_STREAMS]; //! Streams

	ice::uchar num_playing_;      //! Num playing sounds
	ice::uchar num_free_sounds_;  //! Num free sounds
	ice::uchar num_streaming_;    //! Num playing streams
	ice::uchar num_free_streams_; //! Num free streams

	ice::ushort num_src_; //! Num src buffs loaded
};

} // namespace ice::audio
