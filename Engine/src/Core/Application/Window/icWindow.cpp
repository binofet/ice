
#include "Core/Application/Window/icWindow.h"

#ifdef __APPLE__
#include "Core/Application/Window/icWindowOSX.h"
#elif defined(WIN32)
#include "Core/Application/Window/icWindowWIN.h"
#elif defined(X11)
#include "Core/Application/Window/icWindowX11.h"
#endif


/*! Creates platform specific window
**/
icWindow* icWindow::GetWindow(const icWindowDef& winDef, icWindow* parent)
{
#ifdef __APPLE__
    auto window=new icWindowOSX(winDef,parent);
    if(!window->GetRef()){delete window;return nullptr;}return window;
#elif defined(WIN32)
    auto window=new icWindowWIN(winDef,parent);
    if(!window->GetHWND()){delete window;return nullptr;}return window;
#elif defined(X11)
    auto window=new icWindowX11(winDef,parent);
    if(!window->GetRef()){delete window;return nullptr;}return window;
#else
#error This platform isn't supported yet
#endif
}//END FUNCTION CreateWindow(const icWindow& winDef)


/*! Protected c'tor
 *
 *
 *    @param    winDef    Struct containing information for creating a window
 *
**/
icWindow::icWindow(const icWindowDef& ref, icWindow* parent)
{
    m_id = icPlatformEvents::AllocateWindowID();
    icPlatformEvents::Post(icPlatformEventType::WindowRegistered, m_id);
    m_WindowInfo = ref;
    m_Parent = parent;
    m_Child = NULL;
    m_bActive = false;
}// END FUNCTION icWindow(const icWindow& ref)

void icWindow::PumpEvents() {
#ifdef __APPLE__
    icWindowOSX::PumpEvents();
#elif defined(WIN32)
    icWindowWIN::PumpEvents();
#elif defined(X11)
    icWindowX11::PumpEvents();
#endif
}

icWindow::~icWindow() {
    icPlatformEvents::Post(icPlatformEventType::WindowRetired, m_id);
}
