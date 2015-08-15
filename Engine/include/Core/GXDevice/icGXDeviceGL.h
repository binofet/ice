#ifndef __IC_GDEVICE_GL_H__
#define __IC_GDEVICE_GL_H__


#include "Core/GXDevice/icGXDevice.h"

#ifdef ICGL
#include "icGLext.h"

#include "Render/VertBuffer/icVertBuffer.h"


/*! converts IC_PRIM to GL primitive types
**/
FORCE_INLINE GLenum _ICPRIM_TO_GL(IC_PRIM icPrim)
{
    switch(icPrim)
    {
    case IC_PRIM_PLIST: return GL_POINTS;
    case IC_PRIM_LLIST: return GL_LINES;
    case IC_PRIM_LSTRIP: return GL_LINE_STRIP;
    case IC_PRIM_TLIST: return GL_TRIANGLES;
    case IC_PRIM_TSTRIP: return GL_TRIANGLE_STRIP;
    case IC_PRIM_TFAN: return GL_TRIANGLE_FAN;
    default: icWarningf("INVAID IC_PRIM - %i", icPrim);
    }
    return 0;
};


/*! Implements the icGXDevice using the openGL API
**/
class icGXDeviceGL : public icGXDevice
{
public:
    //////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icGXDeviceGL(void);
    ~icGXDeviceGL(void);

    ICRESULT Cleanup(void);
    ICRESULT Init(class icWindow* const pWindow);

    //////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT Update(void);

    virtual void FrameBegin(void);
    virtual void FrameEnd(bool bSwap=true);

    virtual ICRESULT DrawVertBuf(class icVertBuffer* pVB, uint offset=0,
                                 uint count=0);
    virtual ICRESULT DrawIndexedVertBuf(class icVertBuffer* pVB,
                                        class icIndexBuf* pIB);

    virtual ICRESULT CreateVB(class icVertBuffer** ppVB,
                              struct icVertDef* pVertDef);
    virtual ICRESULT ReleaseVB(class icVertBuffer* pVB);

    virtual ICRESULT CreateIB(class icIndexBuf** ppIB,
                              struct icIndexDef* pIndexDef);
    virtual ICRESULT ReleaseIB(class icIndexBuf* pIB);

    virtual ICRESULT CreateTex(class icTexture** ppTex,
                               struct icTexDef* pTexDef);
    virtual ICRESULT CreateTexFromFile(class icTexture** ppTex,
                                       const char* szFile);
    virtual ICRESULT ReleaseTex(class icTexture* pTex);

	virtual ICRESULT CreateFont(class icFontI** ppIFont,
	                            struct icFontParams* pFontParams);
	virtual ICRESULT ReleaseFont( class icFontI* ppIFont );

    /////////////////////////////////////////////////////////////////////////
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

    /////////////////////////////////////////////////////////////////////////
    // CALLBACKS
    virtual void ChangeSize(ushort sizeX, ushort sizeY);

    /////////////////////////////////////////////////////////////////////////
    // ACCESS
#ifdef WIN32
    FORCE_INLINE HDC GetDeviceContext(void) {return m_hDC;};
#endif

protected:
    /////////////////////////////////////////////////////////////////////////
    // HELPER FUNCTIONS
    ICRESULT CreateDevice(void);

private:
    /////////////////////////////////////////////////////////////////////////
    // PRIVATE MEMBERS
#ifdef WIN32
    HDC                m_hDC;
    HGLRC              m_hRC;
#elif defined(__APPLE__)
    AGLContext         m_aglContext;
#elif defined(LINUX)
#endif


    //! Strings returned from the gl driver defining capabilities
    const char*         m_psGLLIB;
    const char*         m_psGLExt;
    const char*         m_psGLVendor;
    const char*         m_psGLRender;
    const char*         m_psGLShaderVer;

    bool                m_bHardwareBuffers;
};
#endif // ICGL
#endif //__IC_GDEVICE_GL_H__
