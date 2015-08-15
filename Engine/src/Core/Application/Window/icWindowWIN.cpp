#ifdef WIN32

#include "Core/Application/Window/icWindowWIN.h"
#include "Core/IO/icInput.h"

#include <windowsx.h>


extern HINSTANCE g_hInstance;
extern HWND g_hwnd;

LRESULT CALLBACK WinProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    icInput* pInput = icInput::Instance();

    if (message != WM_MOUSEWHEEL)
        pInput->GetScrollWheel(0);

    switch (message)
    {
    case WM_SYSKEYDOWN:
        {
            SHORT lalt = GetKeyState(VK_LMENU);
            SHORT ralt = GetKeyState(VK_RMENU);
            if (lalt & 0xF0)
                pInput->GetInput(VK_LMENU, lParam, false);
            else if (ralt & 0xF0)
                pInput->GetInput(VK_RMENU, lParam, false);
        }break;
    case WM_SYSKEYUP:
        {
            //SHORT lalt = GetKeyState(VK_LMENU);
            //SHORT ralt = GetKeyState(VK_RMENU);
            //if (lalt & 0x01)
                pInput->GetInput(VK_LMENU, lParam, true);
            //else if (ralt & 0x01)
                pInput->GetInput(VK_RMENU, lParam, true);
        }break;
    case WM_KEYDOWN:
        {
            pInput->GetInput(wParam, lParam, false);
        }break;
    case WM_KEYUP:
        {
            pInput->GetInput(wParam, lParam, true);
        }break;
    case WM_LBUTTONDOWN:
        {
            pInput->GetInput(VK_LBUTTON, lParam, false);
        }break;
    case WM_LBUTTONUP:
        {
            pInput->GetInput(VK_LBUTTON, lParam, true);
        }break;
    case WM_RBUTTONDOWN:
        {
            pInput->GetInput(VK_RBUTTON, lParam, false);
        }break;
    case WM_RBUTTONUP:
        {
            pInput->GetInput(VK_RBUTTON, lParam, true);
        }break;
    case WM_MBUTTONDOWN:
        {
            pInput->GetInput(VK_MBUTTON, lParam, false);
        }break;
    case WM_MBUTTONUP:
        {
            pInput->GetInput(VK_MBUTTON, lParam, true);
        }break;
    case WM_XBUTTONDOWN:
        {
            if ((WORD)wParam == MK_XBUTTON1)
                pInput->GetInput(VK_XBUTTON1, lParam, false);
            else
                pInput->GetInput(VK_XBUTTON2, lParam, false);
        }break;
    case WM_XBUTTONUP:
        {
            if ((WORD)wParam == MK_XBUTTON1)
                pInput->GetInput(VK_XBUTTON1, lParam, true);
            else
                pInput->GetInput(VK_XBUTTON2, lParam, true);
        }break;
    case WM_MOUSEWHEEL:
        {
            short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            pInput->GetScrollWheel(zDelta);
        }break;
    case WM_MOUSEMOVE:
        {
            pInput->UpdateMousePos(GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam));
        }break;
    }

    icWindowWIN* pWin = (icWindowWIN*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (NULL == pWin)
    {
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return pWin->WindowProc(message, wParam, lParam);
}// END FUNCTION WinProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)


icWindowWIN::icWindowWIN(const icWindowDef& winDef, icWindow* parent, HWND hwnd)
	: icWindow(winDef, parent)
{
	m_hwnd = hwnd;
}

icWindowWIN::icWindowWIN(const icWindowDef& winDef, icWindow* parent)
    : icWindow(winDef, parent)
{
    m_hwnd = NULL;
    Create();
}

ICRESULT icWindowWIN::Cleanup(void)
{
    return IC_FAIL_GEN;
}

void icWindowWIN::Create(void)
{
    if (!m_Parent)
    {
    WNDCLASS wc;
    memset(&wc, 0, sizeof(WNDCLASS));

    wc.style = CS_VREDRAW|CS_HREDRAW|CS_OWNDC;
    wc.lpfnWndProc = (WNDPROC)WinProc;
    wc.hInstance = g_hInstance;
    wc.lpszClassName = m_WindowInfo.pWindowName; // should this be app name?
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(g_hInstance,MAKEINTRESOURCE(101));

    if (!RegisterClass(&wc))
        return;
    }

    int posx = m_WindowInfo.i16PosX;
    int posy = m_WindowInfo.i16PosY;
    int flags = m_WindowInfo.ui32WinFlags;
    int width = m_WindowInfo.i16Width;
    int height = m_WindowInfo.i16Height;

    if (m_WindowInfo.bFullScreen)
    {
        posx = 0;
        posy = 0;
        flags &= ~WS_MAXIMIZE &
                 ~WS_MINIMIZE &
                 ~WS_OVERLAPPEDWINDOW; // remove minimize/maximize style

        flags |= WS_POPUP;

        width = GetSystemMetrics(SM_CXSCREEN);
        height = GetSystemMetrics(SM_CYSCREEN);
    }

    HWND parent = (m_Parent) ? ((icWindowWIN*)m_Parent)->GetHWND() : 0;

    icWindowDef parent_def = {0};
    if (m_Parent)
    {
        m_Parent->GetParams(&parent_def);
        //m_Parent->m_Child = this;
    }

    const char* className = (m_Parent) ? parent_def.pWindowName : m_WindowInfo.pWindowName;


    // Create the window
    HWND window = CreateWindow( className,
                                m_WindowInfo.pWindowName,
                                flags,
                                posx, posy,
                                width, height,
                                parent,                     // Parent
                                0,                          // Menu
                                g_hInstance, 0);

    if (window)
    {
        m_hwnd = window;

        if (!m_Parent)
            g_hwnd = window;

        SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (long)this);
        //return IC_OK;
    }
    //return IC_FAIL_CREATE_WIND;
}

ICRESULT icWindowWIN::Show(void)
{
    if (!ShowWindow(m_hwnd, SW_SHOW))
    {
        UpdateWindow(m_hwnd);
        return IC_OK;
    }

    return IC_FAIL_GEN;
}

ICRESULT icWindowWIN::Hide(void)
{
    if (ShowWindow(m_hwnd, SW_HIDE))
        return IC_OK;

    return IC_FAIL_GEN;
}

ICRESULT icWindowWIN::UpdateParams(const icWindowDef& winDef)
{
    if (m_hwnd)
    {
        //SetWindowPos(m_hwnd, 0, - - - -);
        if (UpdateWindow(m_hwnd))
            return IC_OK;
    }
	m_WindowInfo = winDef;
    return IC_FAIL_GEN;
}

ICRESULT icWindowWIN::GetWindowPos(icVector2Int* pV2Pos)
{
    if (m_hwnd && pV2Pos)
    {
        RECT rect;
        if (GetWindowRect(m_hwnd, &rect))
        {
            pV2Pos->Set(rect.left, rect.top);
            return IC_OK;
        }
    }

    return IC_FAIL_GEN;
}

LRESULT icWindowWIN::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch( message )
    {
        //case WM_SHOWWINDOW: break;
        //case WM_PAINT: break;

        case WM_DESTROY:
            PostQuitMessage( 0 );
            break;

        //case WM_WINDOWPOSCHANGING: break;

        //case WM_ACTIVATEAPP: break;

        //case WM_CHILDACTIVATE:
        //    {
        //        int a = 0;
        //    }break;

        //case WM_NCACTIVATE:
        //    {
        //        int a=0;
        //    }break;

        //case WM_ACTIVATE: break;

        //case WM_SETFOCUS: break;

        //case WM_SIZE:
        //    if (pApp)
        //        ((icGXDeviceGL*)pApp->m_pEngine)->ChangeSize(lParam&0xFFFF,(lParam>>16)&0xFFFF);
        default:
            return DefWindowProc(m_hwnd, message, wParam, lParam);
    }

    return DefWindowProc(m_hwnd, message, wParam, lParam);
}
#endif //WIN32