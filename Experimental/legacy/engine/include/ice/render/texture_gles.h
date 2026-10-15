#pragma once

#include "ice/render/texture.h"
namespace ice::render {

#ifdef ICGLES
} // namespace ice::render
#include "ice/render/gl/api.h"
namespace ice::render {

class texture_gles : public texture {
public:
	//////////////////////////////////////////////////////////////////////////
	// LIFECYCLE
	texture_gles(void);
	virtual ~texture_gles(void) { Cleanup(); };

	virtual ice::result Cleanup(void);

	//////////////////////////////////////////////////////////////////////////
	// OPERATIONS
	virtual ice::result Lock(tex_lock *pTexLock);
	virtual ice::result Unlock(ice::uint level = 0);

	void   *GetLocked(void) { return m_LockBits; };
	GLuint &GetName(void) { return m_Name; };

private:
	GLuint m_Name;
	void  *m_LockBits;
};

#endif // ICGLES
} // namespace ice::render
