#ifndef __IC_WINDOW_H__
#define __IC_WINDOW_H__

#include "Core/icGlobals.h"
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
    virtual ~icWindow(void) { Cleanup(); };
    
    virtual ICRESULT Cleanup(void) { return IC_FAIL_GEN; };
    
    static icWindow* GetWindow(const icWindowDef& winDef, icWindow* parent=0);
    
    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT Show(void)=0;
    virtual ICRESULT Hide(void)=0;
    
    virtual ICRESULT UpdateParams( const icWindowDef& winDef )=0;

    virtual ICRESULT GetWindowPos(icVector2Int* pV2Pos)=0;
    
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
