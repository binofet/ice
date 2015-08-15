#ifdef WIN32

#include "Core/IO/icInput.h"


icInput* icInput::m_ptrInstance = NULL;
//HCURSOR icInput::invisCurs = NULL;

extern bool HACK_FULLSCREEN;

/*! c'tor
**/
icInput::icInput(void)
{
    memset(m_Keys,0,sizeof(bool)*512);
    memset(m_KeyBuffer,0,sizeof(uchar)*1024);
    memset(m_Axis,0,sizeof(icReal)*16);
    
    m_CurKey = 0;
    m_LastPressed = 0;

    m_bHideCursor = false;

    m_Elapsed = m_Timer.Elapsed();
}// END FUNCTION icInput(void)


/*! d'tor
**/
icInput::~icInput(void)
{
}// END FUNCTION ~icInput(void


/*! Called from winproc
 *
 *  @param      wParam      Key that was pressed
 *  @param      bUp         True if the key was released
**/
void icInput::GetInput(WPARAM wParam, LPARAM lParam, bool bUp)
{
    m_Keys[0][wParam] = !bUp;
    m_KeyBuffer[m_CurKey++] = (uchar)wParam;
    if (bUp)
        m_LastPressed = (uchar)wParam;
}// END FUNCTION GetInput(WPARAM wParam)


/*! Called from winproc
 *
 *  @param      wParam      Mouse scroll wheel value
**/
void icInput::GetScrollWheel(short zDelta)
{
    m_Axis[ICAXIS_MOUSE_Z] = (icReal)zDelta/(icReal)WHEEL_DELTA;
}// END FUNCTION GetScrollWheel(short zDelta)


void icInput::UpdateMousePos(int x, int y, bool fullscreen)
{
    WINDOWPLACEMENT place;
    GetWindowPlacement(g_hwnd, &place);

    RECT rect;
    GetClientRect(g_hwnd,&rect);
    
    int winy = place.rcNormalPosition.bottom - place.rcNormalPosition.top;

    if (m_bHideCursor)
    {
        int yborder = GetSystemMetrics(SM_CYSIZEFRAME);
        int cx = rect.right/2;
        int cy = rect.bottom/2;
        int cwinx = (place.rcNormalPosition.right - place.rcNormalPosition.left)/2;
        int cwiny = (-place.rcNormalPosition.top + place.rcNormalPosition.bottom)/2;

        m_Axis[ICAXIS_MOUSE_X] = (icReal)(x - cx)/(icReal)winy;
        m_Axis[ICAXIS_MOUSE_Y] = (icReal)(y - cy)/(icReal)winy;

        m_Mouse.m_Xpos = (short)cwinx;
        m_Mouse.m_Ypos = (short)cwiny;

        if (winy != rect.bottom)
            SetCursorPos(place.rcNormalPosition.left+cwinx,place.rcNormalPosition.bottom-yborder-cy);
        else
            SetCursorPos(cx,cy);
    }
    else
    {
        m_Axis[ICAXIS_MOUSE_X] = (icReal)(x - m_Mouse.m_Xpos)/(icReal)winy;
        m_Axis[ICAXIS_MOUSE_Y] = (icReal)((rect.bottom - y) - m_Mouse.m_Ypos)/(icReal)winy;

        m_Mouse.m_Xpos = (short)x;
        m_Mouse.m_Ypos = (short)(rect.bottom - y);

        if (fullscreen)
            SetCursorPos(m_Mouse.m_Xpos,y);
    }
}


/*! Updates input objects
 *
 *  @param      ICRESULT    Status after the update
**/
ICRESULT icInput::Update(void)
{
    WINDOWPLACEMENT place;
    GetWindowPlacement(g_hwnd, &place);

    //if (m_Elapsed >= 0.00167f)
    {
        memcpy(m_Keys[1],m_Keys[0], sizeof(bool)*256);
        m_Elapsed = 0.0;
        memset(m_Axis,0,sizeof(icReal)*16);

        m_CurKey = 0;
        m_LastPressed = 0;

        m_Mouse.m_OldX = m_Mouse.m_Xpos;
        m_Mouse.m_OldY = m_Mouse.m_Ypos;
    }
   
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
    SetCursorPos((int)x,(int)y);
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
    ShowCursor(FALSE);
}// END FUNCTION HideCursor(void)


void icInput::ShowCurs(void)
{
    m_bHideCursor = false;
    ShowCursor(TRUE);
    //WINDOWPLACEMENT place;
    //GetWindowPlacement(g_hwnd, &place);
    //SetCursorPos(place.rcNormalPosition.left+(int)(place.rcNormalPosition.right-place.rcNormalPosition.left)/2,
    //    (int)(place.rcNormalPosition.bottom-place.rcNormalPosition.top)/2);
}// END FUNCTION ShowCursor(void)

#endif // WIN32