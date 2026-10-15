#pragma once

#include "ice/globals.h"
namespace ice::render {

#define ICTEX_FMT_LUM8 1
// #define ICTEX_FMT_RBGA32    2
#define ICTEX_FMT_RBG24 2
#define ICTEX_FMT_RBGA32 3

#define ICTEX_MAX_NAME 256

/*! Defines parameters about a texture
 **/
struct tex_def {
	/// Horizontal extent in pixels unless the owning API specifies logical UI units.
	ice::uint  width;      //! Width in pixels
	ice::uint  height;     //! Height in pixels
	ice::uint  mip_levels; //! Number of mip map levels
	ice::uint  format;     //! Texture format
	ice::uchar usage;      //! How this buffer is to be used
};

/// Borrowed texture mip lock region. Pitch is bytes per row and storage lasts until unlock.
struct tex_lock {
	/// Zero-based mip level being accessed.
	ice::uint level;
	/// Horizontal extent in pixels unless the owning API specifies logical UI units.
	ice::uint width;
	/// Vertical extent in pixels unless the owning API specifies logical UI units.
	ice::uint height;
	/// Format/token interpreted by the enclosing API.
	ice::uint format;
	/// Bytes between successive pixel rows.
	ice::uint pitch;
	/// Borrowed lock bytes; valid only for the documented lock lifetime.
	void *data;
};

/*! Provides an interface for textures
 **/
class texture {
public:
	/// Construct inactive state; initialize explicitly before active use.
	texture(void) {
		ptr_tex_ = NULL;
		locked_  = false;
	};
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	virtual ~texture(void) { cleanup(); };

	/// Release resources owned by this subsystem; borrowed dependencies must remain alive until
	/// cleanup completes.
	virtual ice::result cleanup(void) { return IC_OK; };

	/// Borrow a writable CPU region for the resource; unlock before drawing, releasing, or
	/// relocking it.
	virtual ice::result lock(tex_lock *tex_lock) = 0;
	/// Commit/release the active CPU lock; borrowed lock pointers become invalid.
	virtual ice::result unlock(ice::uint level = 0) = 0;

	//! Gets the texture object
	//\returns void** Pointer to pointer of texture
	inline void **get_texture(void) { return &ptr_tex_; };

	//! Gets a pointer to the texture definition struct
	//\returns icTexDef* Pointer to texture definition
	inline tex_def *get_tex_def(void) { return &description_; };

	/// Return whether the legacy format includes an alpha channel.
	inline bool has_alpha(void) { return description_.format == ICTEX_FMT_RBGA32; };

	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	int operator<(const texture &rhs);
	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	int operator>(const texture &rhs);
	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	int operator<=(const texture &rhs);
	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	int operator>=(const texture &rhs);
	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	int operator==(const texture &rhs);
	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	int operator!=(const texture &rhs);

	/// Origin path/name used for diagnostics and legacy texture ordering.
	char source_name[ICTEX_MAX_NAME]; //! Texture path + name
protected:
	void *ptr_tex_; //! Pointer to texture data
	bool  locked_;  //! Tells whether the texture is locked

	tex_def description_; //! Defines this texture
};

FORCE_INLINE int texture::operator<(const texture &rhs) {
	return strcmp(source_name, rhs.source_name);
}

FORCE_INLINE int texture::operator>(const texture &rhs) {
	return strcmp(source_name, rhs.source_name);
}

FORCE_INLINE int texture::operator<=(const texture &rhs) {
	return strcmp(source_name, rhs.source_name);
}

FORCE_INLINE int texture::operator>=(const texture &rhs) {
	return strcmp(source_name, rhs.source_name);
}

FORCE_INLINE int texture::operator==(const texture &rhs) {
	return strcmp(source_name, rhs.source_name);
}

FORCE_INLINE int texture::operator!=(const texture &rhs) {
	return strcmp(source_name, rhs.source_name);
}

} // namespace ice::render
