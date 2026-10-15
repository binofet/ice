#pragma once

#include "ice/render/index_buf.h"
namespace ice::render {

#ifdef ICDX9
} // namespace ice::render

#include "DirectX/Include/d3d9.h"
namespace ice::render {
typedef IDirect3DIndexBuffer9 DX9IB;

class index_buf_dx9 : public index_buf {
public:
	//////////////////////////////////////////////////////////////////////////
	// LIFECYCLE
	index_buf_dx9(void);
	~index_buf_dx9(void);

	ice::result Cleanup(void);

	//////////////////////////////////////////////////////////////////////////
	// OPERATIONS
	ice::result Lock(index_lock *pIndexLock);
	ice::result Unlock(void);

	DX9IB *GetIB() { return (DX9IB *)m_pIndexBuf; };
};

#endif //__IC_INDEX_SIZEFER_DX9_H__

} // namespace ice::render
