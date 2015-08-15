#ifndef __IC_GX_DEVICE_DX9_H__
#define __IC_GX_DEVICE_DX9_H__

#include "Core/GXDevice/icGXDevice.h"

#if (defined(WIN32) && defined(ICDX9))
//////////////////////////////////////////////////////////////////////////////
// SYSTEM INCLUDES
#include <windows.h>
#include "DirectX/Include/d3d9.h"
#include "DirectX/Include/d3dx9.h"

#include "Render/VertBuffer/icVertBuffer.h"

typedef IDirect3D9* (__stdcall *D3DCREATEDEV)(UINT);
typedef HRESULT (_stdcall *D3DTEXFROMFILE)(LPDIRECT3DDEVICE9,
                                           LPCSTR,
                                           LPDIRECT3DTEXTURE9*);

class icGXDeviceDX9 : public icGXDevice
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icGXDeviceDX9(void);
    ~icGXDeviceDX9(void);

    ICRESULT Cleanup(void);
    ICRESULT Init(class icWindow* const pWindow);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT Update(void);

    virtual void FrameBegin(void);
    virtual void FrameEnd(bool bSwap=true);

	virtual ICRESULT DrawVertBuf(class icVertBuffer* pVB, uint offset,
	                             uint count);

    virtual ICRESULT DrawIndexedVertBuf(class icVertBuffer* pVB,
                                        class icIndexBuf* pIB);

    virtual ICRESULT CreateVB(class icVertBuffer** ppVB,
                              struct icVertDef* pVertDef);
    virtual ICRESULT ReleaseVB(class icVertBuffer* pVB );

    virtual ICRESULT CreateIB(class icIndexBuf** ppIB,
                              struct icIndexDef* pIndexDef);
    virtual ICRESULT ReleaseIB(class icIndexBuf* pIB );

    virtual ICRESULT CreateTex(class icTexture** ppTex,
                               struct icTexDef* pTexDef);

    virtual ICRESULT CreateTexFromFile(class icTexture** ppTex,
                                       const char* szFile);

    virtual ICRESULT ReleaseTex(class icTexture* pTex);

	virtual ICRESULT CreateFont(class icFontI** ppIFont,
	                            struct icFontParams* pFontParams);
	virtual ICRESULT ReleaseFont( class icFontI* ppIFont );

    //////////////////////////////////////////////////////////////////////////
    // RENDER STATE

    virtual ICRESULT EnableAlphaBlend(bool bAlphaBlend);
    virtual ICRESULT EnableDepthTest(bool bDepth);
    virtual ICRESULT EnableFog(bool bEnable, const icFogParams& params);

    virtual ICRESULT SetAmbientColor(const icColor& col);
	virtual ICRESULT SetClearColor(const icColor& col);
    virtual ICRESULT SetCullMode(IC_CULL_MODE cMode_e);
    virtual ICRESULT SetProjection(const icMatrix44& proj);
    virtual ICRESULT SetViewMatrix(const icMatrix44& view);
    virtual ICRESULT SetWorldTransform(const icMatrix44& world);

    virtual ICRESULT SetTexture(uint index, icTexture* pTex);

    //////////////////////////////////////////////////////////////////////////
    // CALLBACKS
    virtual void ChangeSize(ushort sizeX, ushort sizeY);

	IDirect3DDevice9* GetDevice() { return m_pDevice; };
protected:
    //////////////////////////////////////////////////////////////////////////
    // HELPER FUNCTIONS
    ICRESULT CreateDevice();

private:
    //////////////////////////////////////////////////////////////////////////
    // PRIVATE MEMBERS
    IDirect3D9*           m_pd3d;
    IDirect3DDevice9*     m_pDevice;
    HINSTANCE             m_D3DLibrary;
    HINSTANCE             m_D3DUtilLib;

    D3DCREATEDEV          m_CreateD3DFunc;
    D3DTEXFROMFILE        m_CreateTexFunc;

	IDirect3DVertexDeclaration9* _vDecl[IC_VERT_DEF_COUNT];
};
#endif //(defined(WIN32) && defined(ICDX9))
#endif //__IC_GX_DEVICE_DX9_H__
