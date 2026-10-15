#pragma once

#include "ice/render/vert_buffer.h"
namespace ice::render {

#ifdef ICDX9
} // namespace ice::render

#include "DirectX/Include/d3d9.h"
namespace ice::render {
typedef IDirect3DVertexBuffer9 DX9VB;

class vert_buffer_dx9 : public vert_buffer {
public:
	//////////////////////////////////////////////////////////////////////
	// LIFECYCLE
	vert_buffer_dx9(void);
	~vert_buffer_dx9(void);

	ice::result Cleanup(void);

	//////////////////////////////////////////////////////////////////////
	// OPERATIONS
	ice::result Lock(vert_lock *pVertLock);
	ice::result Unlock(void);

	DWORD _fvf;
};

#endif //__IC_VERT_BUFFER_DX9_H__

} // namespace ice::render
