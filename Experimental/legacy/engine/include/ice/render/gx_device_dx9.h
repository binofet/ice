#pragma once

#include "ice/render/graphics_device.h"
namespace ice::render {

#if (defined(WIN32) && defined(ICDX9))
//////////////////////////////////////////////////////////////////////////////
// SYSTEM INCLUDES
} // namespace ice::render
#include "DirectX/Include/d3d9.h"
#include "DirectX/Include/d3dx9.h"
#include <windows.h>

#include "ice/render/vert_buffer.h"
namespace ice::render {

typedef IDirect3D9 *(__stdcall *D3DCREATEDEV)(UINT);
typedef HRESULT(_stdcall *D3DTEXFROMFILE)(LPDIRECT3DDEVICE9, LPCSTR, LPDIRECT3DTEXTURE9 *);

class gx_device_dx9 : public graphics_device {
public:
	//////////////////////////////////////////////////////////////////////////
	// LIFECYCLE
	gx_device_dx9(void);
	~gx_device_dx9(void);

	ice::result Cleanup(void);
	ice::result Init(class ice::platform::window *const pWindow);

	//////////////////////////////////////////////////////////////////////////
	// OPERATIONS
	virtual ice::result Update(void);

	virtual void FrameBegin(void);
	virtual void FrameEnd(bool bSwap = true);

	virtual ice::result DrawVertBuf(class vert_buffer *pVB, ice::uint offset, ice::uint count);

	virtual ice::result DrawIndexedVertBuf(class vert_buffer *pVB, class index_buf *pIB);

	virtual ice::result CreateVB(class vert_buffer **ppVB, struct vert_def *pVertDef);
	virtual ice::result ReleaseVB(class vert_buffer *pVB);

	virtual ice::result CreateIB(class index_buf **ppIB, struct index_def *pIndexDef);
	virtual ice::result ReleaseIB(class index_buf *pIB);

	virtual ice::result CreateTex(class texture **ppTex, struct tex_def *pTexDef);

	virtual ice::result CreateTexFromFile(class texture **ppTex, const char *szFile);

	virtual ice::result ReleaseTex(class texture *pTex);

	virtual ice::result CreateFont(class font **ppIFont, struct font_params *pFontParams);
	virtual ice::result ReleaseFont(class font *ppIFont);

	//////////////////////////////////////////////////////////////////////////
	// RENDER STATE

	virtual ice::result EnableAlphaBlend(bool bAlphaBlend);
	virtual ice::result EnableDepthTest(bool bDepth);
	virtual ice::result EnableFog(bool bEnable, const fog_params &params);

	virtual ice::result SetAmbientColor(const ice::math::color &col);
	virtual ice::result SetClearColor(const ice::math::color &col);
	virtual ice::result SetCullMode(ice::render::IC_CULL_MODE cMode_e);
	virtual ice::result SetProjection(const ice::math::mat4 &proj);
	virtual ice::result SetViewMatrix(const ice::math::mat4 &view);
	virtual ice::result SetWorldTransform(const ice::math::mat4 &world);

	virtual ice::result SetTexture(ice::uint index, texture *pTex);

	//////////////////////////////////////////////////////////////////////////
	// CALLBACKS
	virtual void ChangeSize(ice::ushort sizeX, ice::ushort sizeY);

	IDirect3DDevice9 *GetDevice() { return m_pDevice; };

protected:
	//////////////////////////////////////////////////////////////////////////
	// HELPER FUNCTIONS
	ice::result CreateDevice();

private:
	//////////////////////////////////////////////////////////////////////////
	// PRIVATE MEMBERS
	IDirect3D9       *m_pd3d;
	IDirect3DDevice9 *m_pDevice;
	HINSTANCE         m_D3DLibrary;
	HINSTANCE         m_D3DUtilLib;

	D3DCREATEDEV   m_CreateD3DFunc;
	D3DTEXFROMFILE m_CreateTexFunc;

	IDirect3DVertexDeclaration9 *_vDecl[ic_vert_def_count];
};
#endif //(defined(WIN32) && defined(ICDX9))

} // namespace ice::render
