#ifdef WIN32
#include "Core/IO/icInput.h"
#include <vector>
#include <algorithm>
static bool hidden=false,baseline=false;
static double previousX=0,previousY=0;
static void syncCursor(bool capture){
    bool active=capture&&g_hwnd&&GetForegroundWindow()==g_hwnd;
    if(active!=hidden){if(active)while(ShowCursor(FALSE)>=0){}else while(ShowCursor(TRUE)<0){}hidden=active;}
    if(active){RECT rect;GetClientRect(g_hwnd,&rect);POINT first={rect.left,rect.top},last={rect.right,rect.bottom};ClientToScreen(g_hwnd,&first);ClientToScreen(g_hwnd,&last);RECT clip={first.x,first.y,last.x,last.y};ClipCursor(&clip);}else ClipCursor(nullptr);
}
icInput* icInput::m_ptrInstance=nullptr;
icInput::icInput():m_bHideCursor(false),m_LastPressed(0),m_CurKey(0),m_Mouse{},m_Elapsed(0){memset(m_Keys,0,sizeof(m_Keys));memset(m_KeyBuffer,0,sizeof(m_KeyBuffer));memset(m_Axis,0,sizeof(m_Axis));}
icInput::~icInput(){syncCursor(false);}
ICRESULT icInput::Init(){RAWINPUTDEVICE mouse={0x01,0x02,0,nullptr};return RegisterRawInputDevices(&mouse,1,sizeof(mouse))?IC_OK:IC_FAIL_GEN;}
ICRESULT icInput::Update(){memcpy(m_Keys[1],m_Keys[0],sizeof(m_Keys[0]));memset(m_Axis,0,sizeof(m_Axis));m_LastPressed=0;syncCursor(m_bHideCursor);return IC_OK;}
void icInput::GetInput(WPARAM key,LPARAM,bool up){if(key<256)SetDown((uchar)key,!up);}
void icInput::GetScrollWheel(short delta){m_Axis[ICAXIS_MOUSE_Z]+=(icReal)delta/WHEEL_DELTA;}
void icInput::UpdateMousePosition(double x,double y,int height){if(baseline&&height>0){m_Axis[0]+=(x-previousX)/height;m_Axis[1]+=(y-previousY)/height;}previousX=x;previousY=y;baseline=true;m_Mouse.m_Xpos=(short)x;m_Mouse.m_Ypos=(short)(height-y);}
void icInput::UpdateMousePos(int x,int y,bool){if(!g_hwnd||m_bHideCursor)return;RECT rect;GetClientRect(g_hwnd,&rect);UpdateMousePosition(x,y,rect.bottom);}
void icInput::AddMouseDelta(double x,double y,int height){if(height>0){m_Axis[0]+=x/height;m_Axis[1]+=y/height;}}
void icInput::GetRawInput(LPARAM handle){
    if(!m_bHideCursor||!g_hwnd)return;
    UINT size=0;GetRawInputData((HRAWINPUT)handle,RID_INPUT,nullptr,&size,sizeof(RAWINPUTHEADER));
    std::vector<unsigned char> bytes(size);if(GetRawInputData((HRAWINPUT)handle,RID_INPUT,bytes.data(),&size,sizeof(RAWINPUTHEADER))!=size)return;
    auto raw=reinterpret_cast<RAWINPUT*>(bytes.data());if(raw->header.dwType!=RIM_TYPEMOUSE||(raw->data.mouse.usFlags&MOUSE_MOVE_ABSOLUTE))return;
    RECT rect;GetClientRect(g_hwnd,&rect);AddMouseDelta(raw->data.mouse.lLastX,raw->data.mouse.lLastY,rect.bottom);
}
void icInput::ResetMouseTracking(){baseline=false;memset(m_Axis,0,sizeof(m_Axis));}
void icInput::FocusChanged(bool focused){CancelHeldInput();if(focused)Init();syncCursor(m_bHideCursor);}
void icInput::WarpCursor(short x,short y){if(g_hwnd){POINT point={x,y};ClientToScreen(g_hwnd,&point);SetCursorPos(point.x,point.y);}ResetMouseTracking();}
void icInput::GetPos(short* x,short* y){if(x)*x=m_Mouse.m_Xpos;if(y)*y=m_Mouse.m_Ypos;}
icReal icInput::GetAxis(uchar axis){return axis<16?m_Axis[axis]:0;}
void icInput::HideCurs(){m_bHideCursor=true;ResetMouseTracking();Init();syncCursor(true);}
void icInput::ShowCurs(){m_bHideCursor=false;ResetMouseTracking();syncCursor(false);}
#endif
