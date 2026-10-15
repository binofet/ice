#pragma once

#include "ice/render/vert_buffer.h"
namespace ice::render {

#ifdef ICGLES
} // namespace ice::render
#include "ice/render/gl/api.h"
namespace ice::render {

class vert_buff_gles : public vert_buffer {
public:
	//////////////////////////////////////////////////////////////////////
	// LIFECYCLE
	vert_buff_gles(void);
	virtual ~vert_buff_gles(void) { Cleanup(); };

	virtual ice::result Cleanup(void);

	//////////////////////////////////////////////////////////////////////
	// OPERATIONS
	virtual ice::result Lock(vert_lock *pVertLock);
	virtual ice::result Unlock(void);

	void *GetLocked() { return m_LockBuf; };

private:
	void *m_LockBuf;
};

#endif // ICGLES

} // namespace ice::render
