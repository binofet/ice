#ifdef WIN32
#include "Core/Application/Window/icWindowWIN.h"
#include "Core/IO/icInput.h"
#include <windowsx.h>
#include <vector>
#include <algorithm>
static std::vector<icWindowWIN*> nativeWindows;
HINSTANCE g_hInstance=GetModuleHandleA(nullptr);
HWND g_hwnd=nullptr;
bool HACK_FULLSCREEN=false;
static LRESULT CALLBACK Dispatch(HWND handle,UINT message,WPARAM wparam,LPARAM lparam){
    auto window=reinterpret_cast<icWindowWIN*>(GetWindowLongPtrA(handle,GWLP_USERDATA));
    if(message==WM_NCCREATE){
        window=static_cast<icWindowWIN*>(reinterpret_cast<CREATESTRUCTA*>(lparam)->lpCreateParams);
        window->SetHWND(handle);SetWindowLongPtrA(handle,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(window));
    }
    return window?window->WindowProc(message,wparam,lparam):DefWindowProcA(handle,message,wparam,lparam);
}
icWindowWIN::icWindowWIN(const icWindowDef& def,icWindow* parent):icWindow(def,parent),m_hwnd(nullptr){Create();if(m_hwnd)nativeWindows.push_back(this);}
icWindowWIN::icWindowWIN(const icWindowDef& def,icWindow* parent,HWND handle):icWindow(def,parent),m_hwnd(handle),m_owned(false){if(m_hwnd)nativeWindows.push_back(this);}
ICRESULT icWindowWIN::Cleanup(){
    nativeWindows.erase(std::remove(nativeWindows.begin(),nativeWindows.end(),this),nativeWindows.end());
    if(m_hwnd){if(g_hwnd==m_hwnd){g_hwnd=nullptr;icInput::Instance()->FocusChanged(false);}SetWindowLongPtrA(m_hwnd,GWLP_USERDATA,0);if(m_owned)DestroyWindow(m_hwnd);m_hwnd=nullptr;}return IC_OK;
}
void icWindowWIN::Create(){
    if(m_WindowInfo.i16Width<=0||m_WindowInfo.i16Height<=0)return;
    WNDCLASSEXA type={};type.cbSize=sizeof(type);type.style=CS_OWNDC|CS_HREDRAW|CS_VREDRAW;type.lpfnWndProc=Dispatch;
    type.hInstance=g_hInstance;type.hCursor=LoadCursor(nullptr,IDC_ARROW);type.lpszClassName="ICE.NativeWindow";
    if(!RegisterClassExA(&type)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return;
    DWORD style=m_WindowInfo.ui32WinFlags?m_WindowInfo.ui32WinFlags:WS_OVERLAPPEDWINDOW;
    if(m_WindowInfo.bFullScreen)style=WS_POPUP;
    RECT rect={0,0,m_WindowInfo.i16Width,m_WindowInfo.i16Height};AdjustWindowRectEx(&rect,style,FALSE,0);
    HWND owner=m_Parent?static_cast<icWindowWIN*>(m_Parent)->GetHWND():nullptr;
    m_hwnd=CreateWindowExA(0,type.lpszClassName,m_WindowInfo.pWindowName,style,m_WindowInfo.i16PosX,m_WindowInfo.i16PosY,
        rect.right-rect.left,rect.bottom-rect.top,owner,nullptr,g_hInstance,this);
    if(m_hwnd)UpdateParams(m_WindowInfo);
}
ICRESULT icWindowWIN::Show(){if(!m_hwnd)return IC_FAIL_CREATE_WIND;ShowWindow(m_hwnd,SW_SHOW);UpdateWindow(m_hwnd);return IC_OK;}
ICRESULT icWindowWIN::Hide(){if(!m_hwnd)return IC_FAIL_CREATE_WIND;ShowWindow(m_hwnd,SW_HIDE);return IC_OK;}
ICRESULT icWindowWIN::UpdateParams(const icWindowDef& def){
    if(!m_hwnd||def.i16Width<=0||def.i16Height<=0)return IC_FAIL_GEN;
    DWORD style=def.bFullScreen?WS_POPUP:(def.ui32WinFlags?def.ui32WinFlags:WS_OVERLAPPEDWINDOW);
    SetWindowLongPtrA(m_hwnd,GWL_STYLE,style);SetWindowTextA(m_hwnd,def.pWindowName?def.pWindowName:"ICE");
    RECT rect={0,0,def.i16Width,def.i16Height};AdjustWindowRectEx(&rect,style,FALSE,0);
    int x=def.i16PosX,y=def.i16PosY,width=rect.right-rect.left,height=rect.bottom-rect.top;
    if(def.bFullScreen){MONITORINFO monitor={};monitor.cbSize=sizeof(monitor);GetMonitorInfo(MonitorFromWindow(m_hwnd,MONITOR_DEFAULTTONEAREST),&monitor);x=monitor.rcMonitor.left;y=monitor.rcMonitor.top;width=monitor.rcMonitor.right-x;height=monitor.rcMonitor.bottom-y;}
    if(!SetWindowPos(m_hwnd,nullptr,x,y,width,height,SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED))return IC_FAIL_GEN;
    m_WindowInfo=def;return IC_OK;
}
ICRESULT icWindowWIN::GetWindowPos(icVector2Int* position){if(!m_hwnd||!position)return IC_FAIL_NPE;RECT rect;if(!GetWindowRect(m_hwnd,&rect))return IC_FAIL_GEN;position->Set(rect.left,rect.top);return IC_OK;}
void icWindowWIN::GetClientSize(int* width,int* height)const{RECT rect={};if(m_hwnd)GetClientRect(m_hwnd,&rect);if(width)*width=rect.right;if(height)*height=rect.bottom;}
LRESULT icWindowWIN::WindowProc(UINT message,WPARAM wparam,LPARAM lparam){
    auto input=icInput::Instance();
    switch(message){
    case WM_CLOSE:RequestClose();return 0;
    case WM_NCDESTROY:{HWND handle=m_hwnd;m_hwnd=nullptr;if(g_hwnd==handle)g_hwnd=nullptr;RequestClose();return DefWindowProcA(handle,message,wparam,lparam);}
    case WM_DESTROY:RequestClose();return 0; // Closing a preview must not post an application-wide quit.
    case WM_SETFOCUS:icPlatformEvents::Post(icPlatformEventType::FocusGained,GetID());g_hwnd=m_hwnd;input->FocusChanged(true);return 0;
    case WM_KILLFOCUS:icPlatformEvents::Post(icPlatformEventType::FocusLost,GetID());if(g_hwnd==m_hwnd)g_hwnd=nullptr;input->FocusChanged(false);return 0;
    case WM_KEYDOWN:case WM_SYSKEYDOWN:input->GetInput(wparam,lparam,false);break;
    case WM_KEYUP:case WM_SYSKEYUP:input->GetInput(wparam,lparam,true);break;
    case WM_LBUTTONDOWN:input->SetDown(ICMOUSE_L,true);break;case WM_LBUTTONUP:input->SetDown(ICMOUSE_L,false);break;
    case WM_RBUTTONDOWN:input->SetDown(ICMOUSE_R,true);break;case WM_RBUTTONUP:input->SetDown(ICMOUSE_R,false);break;
    case WM_MBUTTONDOWN:input->SetDown(ICMOUSE_M,true);break;case WM_MBUTTONUP:input->SetDown(ICMOUSE_M,false);break;
    case WM_MOUSEWHEEL:input->GetScrollWheel(GET_WHEEL_DELTA_WPARAM(wparam));return 0;
    case WM_MOUSEMOVE:input->UpdateMousePos(GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam));return 0;
    case WM_INPUT:input->GetRawInput(lparam);break;
    }
    return DefWindowProcA(m_hwnd,message,wparam,lparam);
}
void icWindowWIN::PumpEvents(){
    MSG event;
    while(PeekMessage(&event,nullptr,0,0,PM_REMOVE)){
        if(event.message==WM_QUIT){icPlatformEvents::Post(icPlatformEventType::ApplicationQuitRequested);for(auto window:nativeWindows)window->RequestClose();continue;}
        TranslateMessage(&event);DispatchMessage(&event);
    }
}
#endif
