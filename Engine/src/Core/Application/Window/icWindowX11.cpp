
#include "Core/Application/Window/icWindowX11.h"

#ifdef X11

Display* icWindowX11::m_pDsp = NULL;
int      icWindowX11::m_iScreenNum = -1;

icWindowX11::icWindowX11(const icWindowDef& winDef) : icWindow(winDef)
{
    Create();
}

void icWindowX11::Create(void)
{
    if (!m_pDsp)
    {
        m_pDsp = XOpenDisplay( NULL );
        m_iScreenNum = DefaultScreen(m_pDsp);
    }

    if( !m_pDsp)
        return;

    unsigned long white = WhitePixel(m_pDsp, m_iScreenNum);
    unsigned long black = BlackPixel(m_pDsp, m_iScreenNum);

    m_Window = XCreateSimpleWindow(m_pDsp, DefaultRootWindow(m_pDsp),
                                   50,50,200,200,0,black,white);

    XMapWindow( m_pDsp, m_Window);

    long eventMask = StructureNotifyMask;
    XSelectInput( m_pDsp, m_Window, eventMask);
}

ICRESULT icWindowX11::Cleanup(void)
{
    return IC_OK;
}

ICRESULT icWindowX11::Show(void)
{
    return IC_OK;
}

ICRESULT icWindowX11::Hide(void)
{
    return IC_OK;
}

ICRESULT icWindowX11::UpdateParams(const icWindowDef& winDef)
{
    return IC_OK;
}

#endif // X11