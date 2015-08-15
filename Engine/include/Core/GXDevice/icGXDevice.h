#ifndef __IC_GDEVICE_H__
#define __IC_GDEVICE_H__

//////////////////////////////////////////////////////////////////////////
// SYSTEM INCLUDES
#include "Core/icGlobals.h"

#include "Math/icColor.h"
#include "Math/Matrix/icMatrix33.h"
#include "Math/Matrix/icMatrix44.h"

#include "Util/icFileName.h"
#include "Util/icTime.h"

enum IC_CULL_MODE
{
    IC_CULL_CLOCKWISE=0,
    IC_CULL_ANTICLOCK,
    IC_CULL_OFF
};


struct icDeviceSetup
{
    uint    screenWidth;
    uint    screenHeight;
    uint    backbufWidth;
    uint    backbufHeight;
    bool    windowed;
    bool    useDefaultMaterials;
};


struct icFogParams
{
    friend bool operator==(const icFogParams& Lhs, const icFogParams& Rhs)
    {
        return (Rhs.col == Lhs.col &&
                Rhs.density == Lhs.density &&
                Rhs.start == Lhs.start &&
                Rhs.end == Lhs.end &&
                Rhs.type == Lhs.type);
    };

    friend bool operator!=(const icFogParams& Lhs, const icFogParams& Rhs)
    {
        return (Rhs.col != Lhs.col ||
                Rhs.density != Lhs.density ||
                Rhs.start != Lhs.start ||
                Rhs.end != Lhs.end ||
                Rhs.type != Lhs.type);
    };

    icColor col;
    float   density;
    float   end;
    float   start;
    int     type;
};


class icGXDevice
{
public:
    enum CLEAR_FLAGS
    {
        CLEAR_NOTHING = 0,
        CLEAR_COLOR = 1<<0,
        CLEAR_DEPTH = 1<<1
    };

    //////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icGXDevice(void);

    virtual ~icGXDevice(void){Cleanup();};

    virtual ICRESULT Cleanup(void){return IC_OK;};
    virtual ICRESULT Init(class icWindow* const ptrWindow)=0;

    //////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT Update(void);

    virtual void FrameBegin(void)=0;
    virtual void FrameEnd(bool bSwap=true );

    virtual ICRESULT DrawVertBuf(class icVertBuffer* pVB, uint offset=0,
                                 uint count=0)=0;
    virtual ICRESULT DrawIndexedVertBuf(class icVertBuffer* pVB,
                                        class icIndexBuf* pIB )=0;

    virtual ICRESULT CreateVB(  class icVertBuffer** ppVB,
                                struct icVertDef* pVertDef )=0;
    virtual ICRESULT ReleaseVB( class icVertBuffer* pVB )=0;

    virtual ICRESULT CreateIB(  class icIndexBuf** ppIB,
                                struct icIndexDef* pIndexDef )=0;
    virtual ICRESULT ReleaseIB( class icIndexBuf* pIB )=0;

    virtual ICRESULT CreateTex( class icTexture** ppTex,
                                struct icTexDef* pTexDef )=0;
    virtual ICRESULT CreateTexFromFile( class icTexture** ppTex,
                                        const char* szFile )=0;
    virtual ICRESULT ReleaseTex( class icTexture* pTex )=0;

	virtual ICRESULT CreateFont(class icFontI** ppIFont,
	                            struct icFontParams* pFontParams)=0;
	virtual ICRESULT ReleaseFont(class icFontI* ppIFont)=0;

    //////////////////////////////////////////////////////////////////////
    // RENDER STATE
    virtual ICRESULT EnableAlphaBlend(bool bAlphaBlend)=0;
    virtual ICRESULT EnableDepthTest(bool bDepth)=0;
    virtual ICRESULT EnableFog(bool bEnable, const icFogParams& params)=0;

    virtual ICRESULT SetAmbientColor(const icColor& col)=0;
	virtual ICRESULT SetClearColor(const icColor& col)=0;
    virtual ICRESULT SetCullMode(IC_CULL_MODE cMode_e)=0;
    virtual ICRESULT SetProjection(const icMatrix44& proj)=0;
    virtual ICRESULT SetViewMatrix(const icMatrix44& view)=0;
    virtual ICRESULT SetWorldTransform(const icMatrix44& world)=0;

    virtual ICRESULT SetTexture(uint index, icTexture* pTex)=0;

    const icColor& GetAmbientColor(void) const;
    const IC_CULL_MODE GetCullMode(void) const;

    const icMatrix44& GetOrtho(void) const;

    //////////////////////////////////////////////////////////////////////
    // CALLBACKS
    virtual void ChangeSize(ushort sizeX, ushort sizeY)=0;

    const icReal UFPS(void) { return m_UFPS; };
    const icReal ULOW(void) { return m_UlowFPS; };
    const icReal UHIGH(void) { return m_UhighFPS; };

    const icReal RFPS(void) { return m_RFPS; };
    const icReal RLOW(void) { return m_RlowFPS; };
    const icReal RHIGH(void) { return m_RhighFPS; };

    const icReal UElapsed(void) { return m_UElapsed; };
    const icReal RElapsed(void) { return m_RElapsed; };

    int GetScreenX(void) { return m_iWidth; };
    int GetScreenY(void) { return m_iHeight; };

protected:
    //////////////////////////////////////////////////////////////////////
    // PROTECTED MEMBERS
    //void*            m_pMainWindow;
    class icWindow* m_pMainWindow;

    icDeviceSetup   m_DeviceInfo;

    icColor         m_colClear;                //! Current clear color
    icColor         m_colAmbient;            //! Current ambient color

    int             m_iHeight;
    int             m_iWidth;
    IC_CULL_MODE    m_Cullmode_e;            //! Current poly cull mode

    /////////////////////////////////////////////////////////////////////////
    // PROFILE DATA
    uint64          m_ui64PrimitivesDrawn;
    uint64          m_ui64DeviceCalls;

    icTimer         m_UTimer;
    icTimer         m_RTimer;

    icReal          m_UElapsed;
    icReal          m_RElapsed;

    icReal          m_UFPS;
    icReal          m_UhighFPS;
    icReal          m_UlowFPS;

    icReal          m_RFPS;
    icReal          m_RhighFPS;
    icReal          m_RlowFPS;

    icReal          m_RunCounter;
    icReal          m_RunTime;
    uint64          m_ui64CurFrame;

    icFogParams     m_FogParams;

    icMatrix44      m_screenOrtho;

    bool            m_bFogEnable;

    bool            m_bDepthTest;
};


/*! Returns the current ambient color
 *
 *    @returns    const icColor&        The current ambient color
 *
**/
FORCE_INLINE const icColor& icGXDevice::GetAmbientColor(void) const
{
    return m_colAmbient;
}// END FUNCTION GetAmbientColor(void) const


/*! Returns the current cull mode
 *
 *  @returns    const IC_CULL_MODE    The current cull mode
**/
FORCE_INLINE const IC_CULL_MODE icGXDevice::GetCullMode(void) const
{
    return m_Cullmode_e;
}// END FUNCTION GetCullMode(void) const


/*! Returns the default ortho matrix
 *
 *  @returns    const icMatrix44&    The current default ortho projection
**/
FORCE_INLINE const icMatrix44& icGXDevice::GetOrtho(void) const
{
    return m_screenOrtho;
}// END FUNCTION GetOrtho(void) const
#endif //__IC_GDEVICE_H__

