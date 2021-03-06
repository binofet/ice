#ifndef __ICE_icInput_H__
#define __ICE_icInput_H__

#include "Math/icCommonMath.h"
#include "Util/icTime.h"

#include "icButtonMap.h"

class icWindow;

#ifdef WIN32
extern HINSTANCE g_hInstance;
extern HWND g_hwnd;
#endif

struct _IC_MOUSE
{
    short   m_Xpos;
    short   m_Ypos;
    short   m_OldX;
    short   m_OldY;
};

/*! Provides interface to various input devices
**/
class icInput
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYLCE
    ~icInput(void);

    static icInput* const Instance(void);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    ICRESULT Init(void);
    ICRESULT Update(void);

    void WarpCursor(short x, short y);
    void GetPos(short* x, short* y);

    icReal GetAxis(uchar axis);

    void HideCurs(void);
    void ShowCurs(void);

    //////////////////////////////////////////////////////////////////////////
    // INQUIRY
    bool IsDown(uchar key) {return m_Keys[0][key];};
    bool IsPressed(uchar key) {return (!m_Keys[0][key] && m_Keys[1][key]);};

    bool AnyKey(void) {return (m_LastPressed!=0);};
#ifdef WIN32
    void GetInput(WPARAM wParam, LPARAM lParam, bool bUp);
    void GetScrollWheel(short zDelta);
    void UpdateMousePos(int x, int y, bool fullscreen=false);

  //  static HCURSOR invisCurs;
#endif


	void SetDown(uchar key, bool isDown) { m_Keys[0][key] = isDown; };
	void SetPressed(uchar key, bool isDown) { m_Keys[0][key] = !isDown; m_Keys[1][key] = isDown; };


private:
    //////////////////////////////////////////////////////////////////////////
    // PRIVATE FUNCTIONS
    icInput( void );

    //////////////////////////////////////////////////////////////////////////
    // PRIVATE MEMBERS
    bool    m_Keys[2][256];
    bool    m_bHideCursor;

    uchar       m_LastPressed;
    uchar       m_KeyBuffer[1024];
    ushort      m_CurKey;

    _IC_MOUSE   m_Mouse;

    icReal      m_Axis[16];
    icReal      m_Elapsed;
    icTimer     m_Timer;

    static icInput*        m_ptrInstance;
};

/*! Function gets the singleton instance
 *
 *  @returns    icInput*    Pointer to the singleton input instance
**/
FORCE_INLINE icInput* const icInput::Instance(void)
{
    // TODO: thread safety
    if (!m_ptrInstance)
    {
        m_ptrInstance = new icInput();
    
#ifdef WIN32
//BYTE ANDmaskCursor[] = 
//{ 
//    0xFF, 0xFC, 0x3F, 0xFF,   // line 1 
//    0xFF, 0xC0, 0x1F, 0xFF,   // line 2 
//    0xFF, 0x00, 0x3F, 0xFF,   // line 3 
//    0xFE, 0x00, 0xFF, 0xFF,   // line 4 
// 
//    0xF7, 0x01, 0xFF, 0xFF,   // line 5 
//    0xF0, 0x03, 0xFF, 0xFF,   // line 6 
//    0xF0, 0x03, 0xFF, 0xFF,   // line 7 
//    0xE0, 0x07, 0xFF, 0xFF,   // line 8 
// 
//    0xC0, 0x07, 0xFF, 0xFF,   // line 9 
//    0xC0, 0x0F, 0xFF, 0xFF,   // line 10 
//    0x80, 0x0F, 0xFF, 0xFF,   // line 11 
//    0x80, 0x0F, 0xFF, 0xFF,   // line 12 
// 
//    0x80, 0x07, 0xFF, 0xFF,   // line 13 
//    0x00, 0x07, 0xFF, 0xFF,   // line 14 
//    0x00, 0x03, 0xFF, 0xFF,   // line 15 
//    0x00, 0x00, 0xFF, 0xFF,   // line 16 
// 
//    0x00, 0x00, 0x7F, 0xFF,   // line 17 
//    0x00, 0x00, 0x1F, 0xFF,   // line 18 
//    0x00, 0x00, 0x0F, 0xFF,   // line 19 
//    0x80, 0x00, 0x0F, 0xFF,   // line 20 
// 
//    0x80, 0x00, 0x07, 0xFF,   // line 21 
//    0x80, 0x00, 0x07, 0xFF,   // line 22 
//    0xC0, 0x00, 0x07, 0xFF,   // line 23 
//    0xC0, 0x00, 0x0F, 0xFF,   // line 24 
// 
//    0xE0, 0x00, 0x0F, 0xFF,   // line 25 
//    0xF0, 0x00, 0x1F, 0xFF,   // line 26 
//    0xF0, 0x00, 0x1F, 0xFF,   // line 27 
//    0xF8, 0x00, 0x3F, 0xFF,   // line 28 
// 
//    0xFE, 0x00, 0x7F, 0xFF,   // line 29 
//    0xFF, 0x00, 0xFF, 0xFF,   // line 30 
//    0xFF, 0xC3, 0xFF, 0xFF,   // line 31 
//    0xFF, 0xFF, 0xFF, 0xFF    // line 32 
//};
// 
//// Yin-shaped cursor XOR mask 
// 
//BYTE XORmaskCursor[] = 
//{ 
//    0x00, 0x00, 0x00, 0x00,   // line 1 
//    0x00, 0x03, 0xC0, 0x00,   // line 2 
//    0x00, 0x3F, 0x00, 0x00,   // line 3 
//    0x00, 0xFE, 0x00, 0x00,   // line 4 
// 
//    0x0E, 0xFC, 0x00, 0x00,   // line 5 
//    0x07, 0xF8, 0x00, 0x00,   // line 6 
//    0x07, 0xF8, 0x00, 0x00,   // line 7 
//    0x0F, 0xF0, 0x00, 0x00,   // line 8 
// 
//    0x1F, 0xF0, 0x00, 0x00,   // line 9 
//    0x1F, 0xE0, 0x00, 0x00,   // line 10 
//    0x3F, 0xE0, 0x00, 0x00,   // line 11 
//    0x3F, 0xE0, 0x00, 0x00,   // line 12 
// 
//    0x3F, 0xF0, 0x00, 0x00,   // line 13 
//    0x7F, 0xF0, 0x00, 0x00,   // line 14 
//    0x7F, 0xF8, 0x00, 0x00,   // line 15 
//    0x7F, 0xFC, 0x00, 0x00,   // line 16 
// 
//    0x7F, 0xFF, 0x00, 0x00,   // line 17 
//    0x7F, 0xFF, 0x80, 0x00,   // line 18 
//    0x7F, 0xFF, 0xE0, 0x00,   // line 19 
//    0x3F, 0xFF, 0xE0, 0x00,   // line 20 
// 
//    0x3F, 0xC7, 0xF0, 0x00,   // line 21 
//    0x3F, 0x83, 0xF0, 0x00,   // line 22 
//    0x1F, 0x83, 0xF0, 0x00,   // line 23 
//    0x1F, 0x83, 0xE0, 0x00,   // line 24 
// 
//    0x0F, 0xC7, 0xE0, 0x00,   // line 25 
//    0x07, 0xFF, 0xC0, 0x00,   // line 26 
//    0x07, 0xFF, 0xC0, 0x00,   // line 27 
//    0x01, 0xFF, 0x80, 0x00,   // line 28 
// 
//    0x00, 0xFF, 0x00, 0x00,   // line 29 
//    0x00, 0x3C, 0x00, 0x00,   // line 30 
//    0x00, 0x00, 0x00, 0x00,   // line 31 
//    0x00, 0x00, 0x00, 0x00    // line 32 
//};
//
//        invisCurs = CreateCursor(g_hInstance, 0,0,32,32,ANDmaskCursor,XORmaskCursor);
#endif
    }
    return m_ptrInstance;
}// END FUNCTION Instance(void)

#endif //__ICE_icInput_H__

