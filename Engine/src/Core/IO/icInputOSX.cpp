
#ifdef __APPLE__

#include "Core/IO/icInput.h"


#include <stdlib.h>
#include <string.h>

icInput* icInput::m_ptrInstance=NULL;


/*! c'tor
**/
icInput::icInput(void)
{
    memset(m_Keys,0,sizeof(bool)*512);
    memset(m_KeyBuffer,0,sizeof(uchar)*1024);
    memset(m_Axis,0,sizeof(icReal)*16);
    
    m_CurKey = 0;
    m_LastPressed = 0;

    m_Elapsed = m_Timer.Elapsed();
}// END FUNCTION icInput(void)


/*! d'tor
**/
icInput::~icInput(void)
{
}// END FUNCTION ~icInput(void


/*! Updates input objects
 *
 *  @param      ICRESULT    Status after the update
**/
ICRESULT icInput::Update(void)
{
    memcpy(m_Keys[1],m_Keys[0], sizeof(bool)*256);

    if (m_Elapsed >= 0.1)
    {
        m_Elapsed = 0.0;
        memset(m_Axis,0,sizeof(icReal)*16);
    }

    m_CurKey = 0;
    m_LastPressed = 0;

    
    m_Elapsed += m_Timer.Elapsed();
    return IC_OK;
}// END FUNCTION Update(void)


/*! Warps the cursor position
 *
 *  @param      x           x-position to warp to
 *  @param      y           y-position to warp to
**/
void icInput::WarpCursor(short x, short y)
{
}// END FUNCTION WarpCursor(short x, short y)


/*! Gets the current cursor position
 *
 *  @param      x           Pointer to hold x position
 *  @param      y           Pointer to hold y position
**/
void icInput::GetPos(short* x, short* y)
{
    *x = m_Mouse.m_Xpos;
    *y = m_Mouse.m_Ypos;
}// END FUNCTION GetPos(short* x, short* y)


/*! Gets the current specified axis value
 *
 *  @returns    icReal      Axis value
**/
icReal icInput::GetAxis(uchar axis)
{
    return m_Axis[axis];
}// END FUNCTION GetAxis(uchar axis)

void icInput::HideCurs(void)
{
    m_bHideCursor = true;

}// END FUNCTION HideCursor(void)


void icInput::ShowCurs(void)
{
    m_bHideCursor = false;

}// END FUNCTION ShowCursor(void)

#endif // #ifdef __APPLE__

