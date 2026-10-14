#ifndef __IC_WINDOW_H__
#define __IC_WINDOW_H__

#include "Core/icGlobals.h"
#include "Core/Application/icPlatformEvents.h"
#include "Math/Vector/icVector2Int.h"

/*! Helper for window creation
**/
struct icWindowDef
{
    uint            ui32WinFlags;    //! Flags that define this window
    short            i16Width;       //! Width of the window
    short            i16Height;      //! Height of the window
    short            i16PosX;        //! Position of Upper Left Corner on X
    short            i16PosY;        //! Position of Upper Left Corner on Y
    const char*        pWindowName;  //! Name of the window
    bool            bFullScreen;     //! If the window is fullscreen
};


/*! Class provides a common interface to platform specific
 *    implementations.
**/
class icWindow
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    virtual ~icWindow(void);
    
    virtual ICRESULT Cleanup(void) { return IC_FAIL_GEN; };
    
    static icWindow* GetWindow(const icWindowDef& winDef, icWindow* parent=0);
    
    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT Show(void)=0;
    virtual ICRESULT Hide(void)=0;
    
    virtual ICRESULT UpdateParams( const icWindowDef& winDef )=0;

    virtual ICRESULT GetWindowPos(icVector2Int* pV2Pos)=0;
    
    // Event pumping is application-wide; close state and sizes belong to each window.
    static void PumpEvents();
    bool ShouldClose() const { return m_closeRequested; }
    icWindowID GetID() const { return m_id; }
    virtual void RequestClose() {
        if (!m_closeRequested) icPlatformEvents::Post(icPlatformEventType::CloseRequested, m_id);
        m_closeRequested = true;
    }
    virtual void GetClientSize(int* width,int* height) const {
        if(width)*width=m_WindowInfo.i16Width;if(height)*height=m_WindowInfo.i16Height;
    }
    virtual void GetFramebufferSize(int* width,int* height) const { GetClientSize(width,height); }

    //////////////////////////////////////////////////////////////////////////
    // ACCESS
    ICRESULT GetParams(icWindowDef* const pWinDef);
    
protected:
    //////////////////////////////////////////////////////////////////////////
    // PROTECTED METHODS
    icWindow(const icWindowDef& winDef, icWindow* parent);
    
    //////////////////////////////////////////////////////////////////////////
    // PROTECTED MEMBERS
    icWindowDef     m_WindowInfo;         //! Contains info about the window
    icWindow*       m_Parent;
    icWindow*       m_Child;
    bool            m_bActive;
    bool m_closeRequested = false;
    icWindowID m_id;
    
private:
    //////////////////////////////////////////////////////////////////////////
    // PRIVATE METHODS
    icWindow(const icWindow& ref);                    //! Disable copy
    const icWindow& operator=(const icWindow& ref);   //! Disable assignment    
};

FORCE_INLINE ICRESULT icWindow::GetParams(icWindowDef* const pWinDef)
{
    *pWinDef = m_WindowInfo;
    //memcpy(pWinDef,&m_WindowInfo,sizeof(icWindowDef));
    return IC_OK;
}// END FUNCTION GetParams(icWindowDef* const pWinDef)

#endif //__IC_WINDOW_H__
