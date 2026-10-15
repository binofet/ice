#pragma once

#include "ice/render/texture.h"
namespace ice::render {

#ifdef ICDX9
} // namespace ice::render

#include "DirectX/Include/d3d9.h"
namespace ice::render {
typedef IDirect3DTexture9 DX9TEX;

class texture_dx9 : public texture {
public:
	//////////////////////////////////////////////////////////////////////////
	// LIFECYCLE
	texture_dx9(void);
	~texture_dx9(void);

	ice::result Cleanup(void);

	//////////////////////////////////////////////////////////////////////////
	// OPERATIONS
	ice::result Lock(tex_lock *pTexLock);
	ice::result Unlock(ice::uint level = 0);

	DX9TEX *GetTex();
};

#endif //__IC_TEXTURE_DX9_H__

} // namespace ice::render
