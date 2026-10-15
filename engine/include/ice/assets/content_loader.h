#pragma once

#include "ice/globals.h"
#include "ice/render/graphics_device.h"

#include "ice/audio/sound.h"
namespace ice::assets {

#define _IC_MAX_TEXUTRE 256
#define _IC_MAX_MODEL 256
} // namespace ice::assets
namespace ice::render {
class texture;
}
namespace ice::render {
class model;
}
namespace ice::assets {
/*! Provides an interface for loading game assets
 **/
class content_loader {
public:
	content_loader(ice::render::graphics_device *device, ice::audio::sound_device_i *audio);
	~content_loader(void);

	ice::result initialize(void);
	ice::result free_content(void);

	template <class T> ice::result load(const char *file_name, T **out_object);

	// TODO: offer asynchronous loading
	// template <class T>
	// void LoadAsync(const char* szFileName, T** ppObj, );

	template <class T> ice::result unload(T **out_object);

	/// Borrow the graphics device supplied at loader construction; the loader does not own it.
	FORCE_INLINE ice::render::graphics_device *get_device(void) { return device_; };
	/// Borrow the audio device supplied at loader construction; the loader does not own it.
	FORCE_INLINE ice::audio::sound_device_i *get_audio(void) { return audio_; };

private:
	ice::render::graphics_device *device_; //! Passed in pointer to grafx device
	ice::audio::sound_device_i   *audio_;  //! Passed in pointer to audio device

	ice::render::texture **tex_heap_;
	ice::render::model   **model_heap_;

	int num_tex_;
	int num_model_;
	int max_tex_;
	int max_model_;
};

} // namespace ice::assets
