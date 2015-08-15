#ifdef WIN32
#ifndef __IC_WINDOW_WIN_H__
#define __IC_WINDOW_WIN_H__

#include <windows.h>

#include "icWindow.h"

class icWindowWIN : public icWindow
{
public:
    /////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icWindowWIN(const icWindowDef& winDef, icWindow* parent);
	icWindowWIN(const icWindowDef& winDef, icWindow* parent, HWND hwnd);
    virtual ~icWindowWIN(void) { Cleanup(); };
    
    virtual ICRESULT Cleanup(void);
    
    /////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT Show(void);
    virtual ICRESULT Hide(void);
    
    virtual ICRESULT UpdateParams(const icWindowDef& winDef);

    virtual ICRESULT GetWindowPos(icVector2Int* pV2Pos);

    //! TODO: move this down a level and make platform independant
    virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);

	inline void SetHWND(HWND hwnd) { m_hwnd = hwnd; };

    inline HWND GetHWND(void) { return m_hwnd; };

private:
    /////////////////////////////////////////////////////////////////////////
    // PRIVATE METHODS
    icWindowWIN(void);
    icWindowWIN(const icWindowWIN& ref);
    const icWindowWIN& operator=(const icWindowWIN& ref);

    void Create(void);

    /////////////////////////////////////////////////////////////////////////
    // PRIVATE MEMBERS
    HWND        m_hwnd;        //! Handle to the window
};

#endif //__IC_WINDOW_WIN_H__
#endif //WIN32
