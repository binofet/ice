#pragma once

#include "ice/render/index_buf.h"
namespace ice::render {

#ifdef ICGLES
} // namespace ice::render
#include "ice/render/gl/api.h"
namespace ice::render {

class index_buf_gles : public index_buf {
public:
	//////////////////////////////////////////////////////////////////////////
	// LIFECYCLE
	index_buf_gles(void);
	virtual ~index_buf_gles(void) { Cleanup(); };

	virtual ice::result Cleanup(void);

	//////////////////////////////////////////////////////////////////////////
	// OPERATIONS
	virtual ice::result Lock(index_lock *pIndexLock);
	virtual ice::result Unlock(void);

	void *GetLocked() { return m_LockBuf; };

private:
	void *m_LockBuf;
};

#endif // ICGL
} // namespace ice::render
