
#include "ice/assets/content_loader.h"

#include "ice/render/index_buf.h"
#include "ice/render/model.h"
#include "ice/render/texture.h"
#include "ice/render/vert_buffer.h"

#include "ice/io/file.h"
namespace ice::assets {

/*! c'tor
 **/
content_loader::content_loader(ice::render::graphics_device *device,
                               ice::audio::sound_device_i   *audio) {
	audio_  = audio;
	device_ = device;

	tex_heap_   = NULL;
	model_heap_ = NULL;

	num_tex_   = 0;
	num_model_ = 0;
	max_tex_   = _IC_MAX_TEXUTRE;
	max_model_ = _IC_MAX_MODEL;
}

/*! d'tor
 **/
content_loader::~content_loader(void) { free_content(); }

/*! Initializes the content loader
 *
 *  @returns        ICRESULT        Success/failure of initialization
 **/
ice::result content_loader::initialize(void) {
	// make sure we have a device
	if (!device_)
		return IC_FAIL_NO_DEVICE;

	// make sure we are starting from a clean state
	if (ICEFAIL(free_content())) {
		ice::warning("Content loader could not initialze");
		return IC_FAIL_GEN;
	}

	// create the space for models and textures
	tex_heap_   = (ice::render::texture **)malloc(sizeof(ice::render::texture *) * _IC_MAX_TEXUTRE);
	model_heap_ = (ice::render::model **)malloc(sizeof(ice::render::model *) * _IC_MAX_MODEL);

	if (tex_heap_ && model_heap_) {
		// zero the mem
		memset(tex_heap_, 0, sizeof(ice::render::texture *) * _IC_MAX_TEXUTRE);
		memset(model_heap_, 0, sizeof(ice::render::model *) * _IC_MAX_MODEL);
		return IC_OK;
	}

	return IC_FAIL_OUT_OF_MEM;
}

/*! Frees all loaded content
 *
 *  @returns        ICRESULT        Success/failure of freeing content
 **/
ice::result content_loader::free_content(void) {
	if (device_) {
		// free allocated textures
		if (tex_heap_) {
			for (int i = 0; i < num_tex_; ++i) {
				device_->release_tex(tex_heap_[i]);
				tex_heap_[i] = NULL;
			}
		}

		// free allocated models (vert buffs and index buffs)
		if (model_heap_) {
			for (int i = 0; i < num_model_; ++i) {
				device_->release_vb(model_heap_[i]->vb_);
				device_->release_ib(model_heap_[i]->ib_);
				model_heap_[i] = NULL;
			}
		}

		return IC_OK;
	}
	return IC_FAIL_NO_DEVICE;
}

/*! Template load function
 *
 *      This is a guaranteed failure, the specialization doesn't exist for T
 *      if the compiler uses this function.  Should maybe make this a compiler
 *      error.
 **/
template <class T>
ice::result content_loader::load(const char *file_name [[maybe_unused]], T **out_object) {
	*out_object = NULL;
	return IC_NOT_IMPL;
}

/*! Audio loader
 *
 *  @param          file_name      Name of the sound file
 *  @param[in/out]  out_object           Storage place for handle to sound object
 *  @returns        ICRESULT        Success/failure of sound load
 **/
template <>
ice::result content_loader::load<ice::audio::sound_i>(const char           *file_name,
                                                      ice::audio::sound_i **out_object) {
	audio_->load_source(file_name);
	audio_->get_sound(file_name, out_object);

	return IC_OK;
}

/*! Template unload function
 *
 *      This is a guaranteed failure, the specialization doesn't exist for T
 *      if the compiler uses this function.  Should maybe make this a compiler
 *      error.
 **/
template <class T> ice::result content_loader::unload(T **out_object [[maybe_unused]]) {
	return IC_FAIL_GEN;
}

} // namespace ice::assets
