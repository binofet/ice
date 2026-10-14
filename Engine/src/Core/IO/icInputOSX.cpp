#include "Core/IO/icInput.h"
#include "Core/Application/Window/icWindowOSX.h"

#include <algorithm>
static double previousX=0,previousY=0;
static bool mouseBaseline=false;
icInput* icInput::m_ptrInstance=nullptr;
icInput::icInput():m_bHideCursor(false),m_LastPressed(0),m_CurKey(0),m_Mouse{},m_Elapsed(0){
    memset(m_Keys,0,sizeof(m_Keys));memset(m_KeyBuffer,0,sizeof(m_KeyBuffer));memset(m_Axis,0,sizeof(m_Axis));
}
icInput::~icInput(){}
ICRESULT icInput::Init(){return IC_OK;}
ICRESULT icInput::Update(){memcpy(m_Keys[1],m_Keys[0],sizeof(m_Keys[0]));memset(m_Axis,0,sizeof(m_Axis));m_LastPressed=0;return IC_OK;}
void icInput::UpdateMousePosition(double x,double y,int height){
    if(mouseBaseline&&height>0){m_Axis[0]+=(x-previousX)/height;m_Axis[1]+=(y-previousY)/height;}
    previousX=x;previousY=y;mouseBaseline=true;
    m_Mouse.m_Xpos=(short)std::max(-32767.0,std::min(x,32767.0));
    m_Mouse.m_Ypos=(short)std::max(-32767.0,std::min(height-y,32767.0));
}
void icInput::WarpCursor(short x,short y){ResetMouseTracking();icWindowOSX::WarpCursor(x,y);}
void icInput::GetPos(short* x,short* y){if(x)*x=m_Mouse.m_Xpos;if(y)*y=m_Mouse.m_Ypos;}
icReal icInput::GetAxis(uchar axis){return axis<16?m_Axis[axis]:0;}
void icInput::HideCurs(){mouseBaseline=false;memset(m_Axis,0,sizeof(m_Axis));m_bHideCursor=true;icWindowOSX::SetCursorCaptured(true);}
void icInput::ShowCurs(){mouseBaseline=false;memset(m_Axis,0,sizeof(m_Axis));m_bHideCursor=false;icWindowOSX::SetCursorCaptured(false);}

void icInput::AddMouseDelta(double x,double y,int height){
    if(height>0){m_Axis[0]+=x/height;m_Axis[1]+=y/height;}
}
void icInput::ResetMouseTracking(){mouseBaseline=false;memset(m_Axis,0,sizeof(m_Axis));}
