
#include "ice/render/gl/index_buffer.h"
namespace ice::render::gl {

bool index_buffer::hardware_enabled = false;

/*! c'tor
 **/
index_buffer::index_buffer(void) : ice::render::index_buf() {
	name_     = 0xFFFFFFFF;
	lock_buf_ = NULL;
}

/*! Cleanups up memory used by the index buffer
 *
 *    @returns    ICRESULT    Result of cleanup
 *
 **/
ice::result index_buffer::cleanup(void) {
	ice::result result_code = IC_OK;
	if (lock_buf_) {
		if (!hardware_enabled)
			free(lock_buf_);

		lock_buf_   = NULL;
		result_code = IC_WARN_ALREADY_LOCKED_IB;
	}
	if (hardware_enabled) {
		if (name_ != 0xFFFFFFFF) {
			glDeleteBuffers(1, &name_);
			name_ = 0xFFFFFFFF;
		}
	}
	return result_code;
}

/*! Locks an index buffer
 *
 *    @param[in/out]    index_lock  Contains pointer to locked index data
 *    @returns          ICRESULT    Result of locking the index buffer
 *
 **/
ice::result index_buffer::lock(ice::render::index_lock *index_lock) {
	if (!locked_) {
		// hardware buffers only get a temp system copy from openGL
		if (hardware_enabled) {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, name_);
			lock_buf_ = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_READ_WRITE);
			if (!lock_buf_) {
				GLenum err = glGetError();
				warningf("icIndexBufGL::Lock() - glMapBuffer failed, error: %i", err);
				return IC_FAIL_LOCK_IB;
			}
		} else {
			// TODO: check for buffer size changes
			// check if we already have the space allocated
			if (!lock_buf_) {
				// we want to switch this to a memory pool using doug leas allocator
				ice::uint size = (description_.index_size == ice::render::ic_index_size_16) ? 2 : 4;
				void     *mem  = malloc(description_.num_index * size);
				if (mem) {
					lock_buf_              = mem;
					index_lock->data       = lock_buf_;
					index_lock->num_index  = description_.num_index;
					index_lock->index_size = description_.index_size;
					locked_                = true;
					return IC_OK;
				}
				index_lock->data      = 0;
				index_lock->num_index = 0;
				return IC_FAIL_OUT_OF_MEM;
			}
		}
		index_lock->data       = lock_buf_;
		index_lock->num_index  = description_.num_index;
		index_lock->index_size = description_.index_size;
		locked_                = true;
		return IC_OK;
	}
	return IC_WARN_ALREADY_LOCKED_IB;
}

/*! Unlocks a locked index buffer
 *
 *    @returns    ICRESULT    Result of unlocking the index buffer
 *
 **/
ice::result index_buffer::unlock(void) {
	if (locked_ && lock_buf_) {
		// TODO: provide a mechanism to prevent two different Index
		//       Buffers from obtaining a lock, opengl does not support
		//       that.
		if (hardware_enabled) {
			glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
			lock_buf_ = NULL;
		}
		locked_ = false;
		return IC_OK;
	}
	return IC_WARN_GEN;
}
} // namespace ice::render::gl
