#ifndef __IC_FONT_GL_H__
#define __IC_FONT_GL_H__

#include "Core/icGlobals.h"

#ifdef ICGL
//////////////////////////////////////////////////////////////////////////////
// SYSTEM INCLUDES
#ifdef WIN32
#include <windows.h>
#include <GL/gl.h>
#elif defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include "icFont.h"
#include "Core/GXDevice/icGXDeviceGL.h"
#include "Content/icContentLoader.h"

class icFontGL : public icFontI
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icFontGL(void);
    virtual ~icFontGL(void);

    virtual ICRESULT Initialize(const char* szFileName, const int size,
                                icGXDevice* pDevice);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT Print(const icVector2Int& v2ScreenPos, const icColor& color,
                           const char* buff);

    virtual ICRESULT Printf(const icVector2Int& v2ScreenPos, const icColor& color,
                            const char* buff, ...);

private:

    GLint   m_Base;
    GLuint m_Texture;
    float m_Advance[96];
    int m_Size;
};

#endif //__IC_FONT_GL_H__
#endif //ICGL