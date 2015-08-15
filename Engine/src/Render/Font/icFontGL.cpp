
#include "Render/Font/icFontGL.h"

#ifdef ICGL

#include "Core/GXDevice/icGXDeviceGL.h"

#ifdef WIN32
#include <windows.h>
#endif

/*! Default c'tor
**/
icFontGL::icFontGL()
{
    m_Base = -1;
}// END FUNCTION icFontGL(void)


/*! Default d'tor
 *
 *  Releases initialized font (if any)
 *  
**/
icFontGL::~icFontGL(void)
{
    if (m_Base > -1)
        glDeleteLists(m_Base,96);
}// END FUNCTION ~icFontGL(void)


/*! Initializes font
 *
 *  @param      szFontName  Name of the font
 *  @param      pDevice     Graphics device
 *  @returns    ICRESULT    Status result of font initialization
**/
ICRESULT icFontGL::Initialize(const char* szFileName, const int size,
							  icGXDevice* pDevice)
{
    _gxDevice = pDevice;
#ifdef WIN32
    m_Base = glGenLists(256);

    HFONT old_font;

    HFONT font = CreateFont(size,           // Height
                            0,              // Width
                            0,              // Angle of Escapement
                            0,              // Orientation Angle
                            FW_BOLD,        // Font Weight
                            FALSE,          // Italic
                            FALSE,          // Underline
                            FALSE,          // Strikeout
                            ANSI_CHARSET ,   // Charater set
                            OUT_TT_PRECIS,  // Output Precision
                            CLIP_DEFAULT_PRECIS,    // Clipping Precision
                            ANTIALIASED_QUALITY,    // Output Quality
                            FF_DONTCARE|DEFAULT_PITCH,  // Famimly and Pitch
                            szFileName);

    if (!font)
        return IC_FAIL_GEN;

    icGXDeviceGL* glDevice = (icGXDeviceGL*)_gxDevice;

    HDC hDC = glDevice->GetDeviceContext();

    if (!hDC)
        return IC_FAIL_GEN;

    old_font = (HFONT)SelectObject(hDC, font);
    wglUseFontBitmaps(hDC, 32, 96, m_Base);
    SelectObject(hDC, old_font);
    DeleteObject(font);
#elif defined(__APPLE__)
#else
#endif

    return IC_OK;
}// END FUNCTION Initialize(const char* szFontName,
 //                         const icGXDevice* const pDevice, bool bold)



/*! Prints a character array to the screen
 *
 *  @param      x           The x coordinate
 *  @param      y           The y coordinate
 *  @param      color       The color to use printing the font
 *  @param      buff        The character array to display
 *  @returns    ICRESULT    Condition status of function
**/
ICRESULT icFontGL::Print(const icVector2Int& v2ScreenPos, const icColor& color, const char *buff)
{
    if (m_Base > -1)
    {
        icMatrix44 temp;
        temp.Identity();

        _gxDevice->SetWorldTransform(temp);
        _gxDevice->SetViewMatrix(temp);
        _gxDevice->SetProjection(_gxDevice->GetOrtho());
        
        glColor3f(color.R(),color.G(),color.B());//,color.A());
        glRasterPos2i(v2ScreenPos.x,v2ScreenPos.y);


        glPushAttrib(GL_LIST_BIT);
        glListBase(m_Base - 32);
        glCallLists(strlen(buff), GL_UNSIGNED_BYTE, buff);
        glPopAttrib();
        return IC_OK;
    }
    return IC_FAIL_GEN;
}// END FUNCTION Print(int x, int y, const icColor& color, const char* buff)


/*! Prints a formatted character array to the screen
 *
 *  @param      x           The x coordinate
 *  @param      y           The y coordinate
 *  @param      color       The color to use printing the font
 *  @param      buff        The character array to display
 *  @param      ...         List of arguments to use in formatting
 *  @returns    ICRESULT    Condition status of function
**/
ICRESULT icFontGL::Printf(const icVector2Int& v2ScreenPos, const icColor& color,
                          const char *buff, ...)
{
    if (m_Base > -1)
    {
        char temp[512];
        va_list ap;

        va_start(ap, buff);
#ifdef WIN32
        vsprintf_s(temp,512,buff,ap);
#else
        vsprintf(temp, buff, ap);
#endif
        va_end(ap);
        return Print(v2ScreenPos,color,temp);
    }
    return IC_FAIL_GEN;
}// END FUNCTION Printf(int x, int y, const icColor& color,
 //                     const char* buff, ...)

#endif //ICGL