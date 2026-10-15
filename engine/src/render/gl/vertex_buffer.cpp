
#include "ice/render/gl/vertex_buffer.h"

#include "ice/render/gl/api.h"
namespace ice::render::gl {

bool vert_buff_gl::hardware_enabled = false;

/*! c'tor
 **/
vert_buff_gl::vert_buff_gl(void) : ice::render::vert_buffer() {
	lock_buf_ = NULL;
	name_     = 0xFFFFFFFF;
}

/*! Cleanups up memory used by the vert buffer
 *
 *    @returns    ICRESULT    Result of cleanup
 *
 **/
ice::result vert_buff_gl::cleanup(void) {
	ice::result result_code = IC_OK;
	if (lock_buf_) {
		free(lock_buf_);
		lock_buf_   = NULL;
		result_code = IC_WARN_ALREADY_LOCKED_VB;
	}
	if (hardware_enabled) {
		if (name_ != 0xFFFFFFFF) {
			glDeleteBuffers(1, &name_);
			name_ = 0xFFFFFFFF;
		}
	}
	return result_code;
}

/*! Locks a vertex buffer
 *
 *    @param[in/out]    vert_lock   Contains pointer to locked vert data
 *    @returns          ICRESULT    Result of locking the vert buffer
 **/
ice::result vert_buff_gl::lock(ice::render::vert_lock *vert_lock) {
	if (!locked_) {
		// hardware buffers only get a temp system copy from openGL
		if (hardware_enabled) {
			glBindBuffer(GL_ARRAY_BUFFER, name_);
			lock_buf_ = glMapBuffer(GL_ARRAY_BUFFER, GL_READ_WRITE);
			if (!lock_buf_) {
				GLenum err = glGetError();
				warningf("icVertBuffGL::Lock() - glMapBuffer failed, error: %i", err);
				return IC_FAIL_LOCK_VB;
			}
		} else {
			// TODO: check for buffer size changes
			// check if we already have the space allocated
			if (!lock_buf_) {
				// we want to switch this to a memory pool using doug leas allocator
				void *mem = malloc(description_.num_verts * description_.vert_size);
				if (mem) {
					lock_buf_            = mem;
					vert_lock->data      = lock_buf_;
					vert_lock->num_verts = description_.num_verts;
					vert_lock->vert_size = description_.vert_size;
					locked_              = true;
					return IC_OK;
				}
				vert_lock->data      = 0;
				vert_lock->num_verts = 0;
				vert_lock->vert_size = 0;
				return IC_FAIL_OUT_OF_MEM;
			}
		}
		vert_lock->data      = lock_buf_;
		vert_lock->num_verts = description_.num_verts;
		vert_lock->vert_size = description_.vert_size;
		locked_              = true;
		return IC_OK;
	}
	return IC_WARN_ALREADY_LOCKED_VB;
}

/*! Unlocks a locked vertex buffer
 *
 *    @returns    ICRESULT    Result of unlocking the vert buffer
 *
 **/
ice::result vert_buff_gl::unlock(void) {
	if (locked_ && lock_buf_) {
		// TODO: provide a mechanism to prevent two different Vertex
		//       Buffers from obtaining a lock, opengl does not support
		//       that.
		if (hardware_enabled) {
			glUnmapBuffer(GL_ARRAY_BUFFER);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			lock_buf_ = NULL;
		} else {
			// memory stuff?
		}
		locked_ = false;
		return IC_OK;
	}
	return IC_WARN_GEN;
}

} // namespace ice::render::gl
