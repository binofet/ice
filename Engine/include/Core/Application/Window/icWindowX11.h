#ifndef __IC_WINDOW_X11_H__
#define __IC_WINDOW_X11_H__

#include "icWindow.h"

#ifdef X11

#include <X11/Xlib.h>

class icWindowX11 : public icWindow
{
public:
    /////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icWindowX11(const icWindowDef& winDef);
    virtual ~icWindowX11(void) { Cleanup(); };

    virtual ICRESULT Cleanup(void);

    /////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT Show(void);
    virtual ICRESULT Hide(void);

    virtual ICRESULT UpdateParams(const icWindowDef& winDef);


private:
    /////////////////////////////////////////////////////////////////////////
    // PRIVATE METHODS
    icWindowX11(void);
    icWindowX11(const icWindowX11& ref);
    const icWindowX11& operator=(const icWindowX11& ref);

    void Create(void);

    /////////////////////////////////////////////////////////////////////////
    // PRIVATE MEMBERS

    static Display* m_pDsp;
    static int      m_iScreenNum;
    Window          m_Window;
};

#endif //__IC_WINDOW_X11_H__
#endif //X11
