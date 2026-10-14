#ifdef __APPLE__
#include <CoreText/CoreText.h>
#include <vector>
#endif

#include "Render/Font/icFontGL.h"

#ifdef ICGL

#include "Core/GXDevice/icGXDeviceGL.h"
#ifdef X11
#include <X11/Xlib.h>
#include <GL/glx.h>
#endif

#ifdef WIN32
#include <windows.h>
#endif

/*! Default c'tor
**/
icFontGL::icFontGL()
{
    m_Base = -1;
    m_Texture = 0;
    m_Size = 24;
}// END FUNCTION icFontGL(void)


/*! Default d'tor
 *
 *  Releases initialized font (if any)
 *  
**/
icFontGL::~icFontGL(void)
{
    if(m_Texture) glDeleteTextures(1,&m_Texture);
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
    m_Size = size;
    const int cell=64,width=1024,height=512;
    std::vector<unsigned char> pixels(width*height*4,0);
    CGColorSpaceRef colorSpace=CGColorSpaceCreateDeviceRGB();
    CGContextRef context=CGBitmapContextCreate(pixels.data(),width,height,8,width*4,colorSpace,kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(colorSpace);
    CFStringRef name=CFStringCreateWithCString(nullptr,szFileName,kCFStringEncodingUTF8);
    CTFontRef font=CTFontCreateWithName(name,size,nullptr);CFRelease(name);
    if(!context||!font){if(context)CGContextRelease(context);if(font)CFRelease(font);return IC_FAIL_GEN;}
    CGContextSetRGBFillColor(context,1,1,1,1);
    for(int i=0;i<96;++i){
        UniChar character=i+32;CGGlyph glyph;
        CTFontGetGlyphsForCharacters(font,&character,&glyph,1);
        CGSize advance;CTFontGetAdvancesForGlyphs(font,kCTFontOrientationHorizontal,&glyph,&advance,1);
        m_Advance[i]=advance.width;
        CGPoint position=CGPointMake((i%16)*cell+4,(i/16)*cell+16);
        CTFontDrawGlyphs(font,&glyph,&position,1,context);
    }
    CFRelease(font);CGContextRelease(context);
    // CoreGraphics stores the top scanline first; OpenGL's atlas starts at the bottom.
    for(int row=0;row<height/2;++row)
        for(int col=0;col<width*4;++col)
            std::swap(pixels[row*width*4+col],pixels[(height-1-row)*width*4+col]);
    glGenTextures(1,&m_Texture);glBindTexture(GL_TEXTURE_2D,m_Texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    glBindTexture(GL_TEXTURE_2D,0);
#elif defined(X11)
    Display* display=XOpenDisplay(nullptr);if(!display)return IC_FAIL_GEN;
    char pattern[128];snprintf(pattern,sizeof(pattern),"-misc-fixed-medium-r-normal--%d-*-*-*-*-*-iso8859-1",size);
    XFontStruct* font=XLoadQueryFont(display,pattern);
    if(!font)font=XLoadQueryFont(display,"10x20");
    if(!font)font=XLoadQueryFont(display,"fixed");
    if(!font){XCloseDisplay(display);return IC_FAIL_GEN;}
    m_Base=glGenLists(96);glXUseXFont(font->fid,32,96,m_Base);
    XFreeFont(display,font);XCloseDisplay(display);
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
#ifdef __APPLE__
    if(!buff||!m_Texture)return IC_FAIL_GEN;
    glPushAttrib(GL_ENABLE_BIT|GL_COLOR_BUFFER_BIT|GL_TEXTURE_BIT|GL_CURRENT_BIT|GL_DEPTH_BUFFER_BIT);
    GLint matrixMode;glGetIntegerv(GL_MATRIX_MODE,&matrixMode);
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadMatrixf((const float*)(const icReal*)_gxDevice->GetOrtho());
    glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
    glDisable(GL_LIGHTING);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);
    glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,m_Texture);
    glColor4f(color.R(),color.G(),color.B(),color.A());
    float x=v2ScreenPos.x,y=v2ScreenPos.y;
    for(const unsigned char* c=(const unsigned char*)buff;*c;++c){
        if(*c=='\n'){x=v2ScreenPos.x;y-=m_Size;continue;}
        if(*c<32||*c>127)continue;
        int i=*c-32;float u=(i%16)*64.0f/1024.0f,v=(i/16)*64.0f/512.0f;
        float left=x-4,bottom=y-16;
        glBegin(GL_QUADS);
        glTexCoord2f(u,v);glVertex2f(left,bottom);
        glTexCoord2f(u+64.0f/1024,v);glVertex2f(left+64,bottom);
        glTexCoord2f(u+64.0f/1024,v+64.0f/512);glVertex2f(left+64,bottom+64);
        glTexCoord2f(u,v+64.0f/512);glVertex2f(left,bottom+64);
        glEnd();x+=m_Advance[i];
    }
    glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(matrixMode);glPopAttrib();
    return IC_OK;
#endif
    if (m_Base > -1)
    {
        glPushAttrib(GL_ENABLE_BIT|GL_CURRENT_BIT|GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glDisable(GL_LIGHTING);glDisable(GL_DEPTH_TEST);glDisable(GL_TEXTURE_2D);
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
    if (m_Base > -1 || m_Texture)
    {
        char temp[512];
        va_list ap;

        va_start(ap, buff);
#ifdef WIN32
        vsprintf_s(temp,512,buff,ap);
#else
        vsnprintf(temp, sizeof(temp), buff, ap);
#endif
        va_end(ap);
        return Print(v2ScreenPos,color,temp);
    }
    return IC_FAIL_GEN;
}// END FUNCTION Printf(int x, int y, const icColor& color,
 //                     const char* buff, ...)

#endif //ICGL