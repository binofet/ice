
#include "ice/render/gl/texture.h"
namespace ice::render::gl {

#ifdef ICGL

/*! c'tor
 **/
texture::texture(void) : ice::render::texture() {
	name_      = 0xFFFFFFFF;
	lock_bits_ = NULL;
}

/*! Cleans up memory on system/graphics device
 *
 *  @returns    ICRESULT    Result of cleanup
 **/
ice::result texture::cleanup(void) {
	if (lock_bits_) {
		free(lock_bits_);
		lock_bits_ = NULL;
	}
	return IC_OK;
}

/*! Locks the texture into system memory
 *
 *  @param[in/out]  tex_lock    Holds information about the texture
 *  @returns        ICRESULT    Result of locking the texture
 **/
ice::result texture::lock(ice::render::tex_lock *tex_lock) {
	if (!locked_) {
		if (!lock_bits_) {
			ice::uint depth = 32; // TODO: get from data
			void     *mem   = malloc(description_.width * description_.height * depth);
			if (mem) {
				lock_bits_       = mem;
				tex_lock->data   = lock_bits_;
				tex_lock->format = description_.format;
				tex_lock->height = description_.height;
				tex_lock->width  = description_.width;
				tex_lock->pitch  = 0; // TODO
				tex_lock->level  = 0; // TODO
				locked_          = true;
				return IC_OK;
			}
			return IC_FAIL_OUT_OF_MEM;
		}
		tex_lock->data   = lock_bits_;
		tex_lock->format = description_.format;
		tex_lock->height = description_.height;
		tex_lock->width  = description_.width;
		tex_lock->pitch  = 0; // TODO
		tex_lock->level  = 0; // TODO
		locked_          = true;
		return IC_OK;
	}
	return IC_WARN_ALREADY_LOCKED_VB;
}

/*! Unlocks a locked texture
 *
 *  @param      level       What level of texture to unlock
 *  @returns    ICRESULT    Result of unlocking texture
 **/
ice::result texture::unlock(ice::uint level) {
	if (locked_ && lock_bits_) {
		glTexImage2D(GL_TEXTURE_2D, level, 3, description_.width, description_.height, 0, GL_RGB,
		             GL_UNSIGNED_BYTE, lock_bits_);
		locked_ = false;
		free(lock_bits_);
		lock_bits_ = NULL;
		return IC_OK;
	}
	return IC_WARN_GEN;
}

#endif // ICGL
} // namespace ice::render::gl
