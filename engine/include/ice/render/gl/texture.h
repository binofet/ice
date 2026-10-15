#pragma once

#include "ice/render/texture.h"
namespace ice::render::gl {

#ifdef ICGL
} // namespace ice::render::gl
#include "ice/render/gl/api.h"
namespace ice::render::gl {

/// Owned legacy texture interface with source-name metadata. Device/context ownership rules apply
/// through cleanup.
class texture : public ice::render::texture {
public:
	texture(void);
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	virtual ~texture(void) { cleanup(); };

	virtual ice::result cleanup(void);

	virtual ice::result lock(ice::render::tex_lock *tex_lock);
	virtual ice::result unlock(ice::uint level = 0);

	/// Borrow lock staging bytes until the matching unlock; do not retain or free the pointer.
	void *get_locked(void) { return lock_bits_; };
	/// Borrow the native name/path metadata; validity follows the owning object/resource lifetime.
	GLuint &get_name(void) { return name_; };

	/// Process-wide legacy GL hardware-buffer switch; configure before creating resources.
	static bool hardware_enabled;

private:
	GLuint name_;
	void  *lock_bits_;
};

#endif // ICGL
} // namespace ice::render::gl
