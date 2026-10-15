#pragma once

#include "ice/render/index_buf.h"
namespace ice::render::gl {

#ifdef ICGL
} // namespace ice::render::gl
#include "ice/render/gl/api.h"
namespace ice::render::gl {

/// Owned native GL index buffer; returned buffer names are borrowed and must not be deleted by
/// callers.
class index_buffer : public ice::render::index_buf {
public:
	index_buffer(void);
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	virtual ~index_buffer(void) { cleanup(); };

	virtual ice::result cleanup(void);

	virtual ice::result lock(ice::render::index_lock *index_lock);
	virtual ice::result unlock(void);

	/// Borrow lock staging bytes until the matching unlock; do not retain or free the pointer.
	void *get_locked() { return lock_buf_; };
	/// Borrow the native name/path metadata; validity follows the owning object/resource lifetime.
	GLuint &get_name() { return name_; };

	/// Process-wide legacy GL hardware-buffer switch; configure before creating resources.
	static bool hardware_enabled;

private:
	GLuint name_;
	void  *lock_buf_;
};

#endif // ICGL
} // namespace ice::render::gl
