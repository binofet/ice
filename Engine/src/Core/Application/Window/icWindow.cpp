
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
    return (icWindow*)new icWindowOSX(winDef);
#elif defined(WIN32)
    return (icWindow*)new icWindowWIN(winDef, parent);
#elif defined(X11)
    return (icWindow*)new icWindowX11(winDef);
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
    m_WindowInfo = ref;
    m_Parent = parent;
    m_Child = NULL;
}// END FUNCTION icWindow(const icWindow& ref)
