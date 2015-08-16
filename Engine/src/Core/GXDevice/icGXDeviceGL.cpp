

#include "Core/GXDevice/icGXDeviceGL.h"
#include "Core/Application/Window/icWindow.h"

#include "Content/icImage.h"

#ifdef ICGL

#include "Render/VertBuffer/icVertBufferGL.h"
#include "Render/IndexBuffer/icIndexBufGL.h"
#include "Render/Texture/icTextureGL.h"
#include "Render/Font/icFontGL.h"

#ifdef WIN32
#include "gl/glu.h"
#include "Core/Application/Window/icWindowWIN.h"
#elif defined(__APPLE__) || defined(__APPLE_CC__)
#include <Carbon/Carbon.h>
#include <OpenGL/glu.h>
#include "Core/Application/Window/icWindowOSX.h"
#elif (defined(X11))
#include <GL/glu.h>
#endif


/*! Default Constructor
**/
icGXDeviceGL::icGXDeviceGL( void )
{
#ifdef WIN32
    m_hDC = NULL;
#endif

    m_bHardwareBuffers = false;
    m_colClear = icColor::BBLUE;
}// END FUNCTION icGXDeviceGL( void )


/*! Default Destructor
**/
icGXDeviceGL::~icGXDeviceGL( void )
{
    Cleanup();
}// END FUNCTION ~icGXDeviceGL( void )


/*! Cleans up the device, frees any allocated memory
 *
 *    @returns    ICRESULT    Result of cleanup
 *
**/
ICRESULT icGXDeviceGL::Cleanup( void )
{
    ICRESULT res = IC_OK;
#ifdef WIN32
    if (m_hDC)
    {
        if (!wglMakeCurrent(m_hDC, NULL))
            res = IC_FAIL_GEN;
        //if (ReleaseDC((HWND)m_pMainWindow, m_hDC)==0)
        //    res = IC_FAIL_GEN;
    }

    if (m_hRC)
    {
        if (!wglDeleteContext(m_hRC))
            res = IC_FAIL_GEN;
    }
    m_hDC = NULL;
    m_hRC = NULL;

#elif defined(__APPLE__) || defined(__APPLE_CC__)

    // Unbind to context
    aglSetCurrentContext(NULL);
    
    // Remove drawable
    aglSetWindowRef(m_aglContext, NULL);
    aglDestroyContext(m_aglContext);

#elif (defined(X11))
#endif

    return res;
}// END FUNCTION Cleanup( void )


/*! Initializes the openGL device
 *
 *    @param[in]    hWnd        Pointer to the window
 *    @returns    ICRESULT    Status of initialization
 *
**/
ICRESULT icGXDeviceGL::Init( icWindow* const pWindow )
{
    // we can only create a device with a handle to the window
    if ( pWindow )
    {
        m_pMainWindow = pWindow;
        return CreateDevice();
    }

    return IC_FAIL_GEN;
}// END FUNCTION Init( void* hWnd )


/*! Creates the openGL device
 *
 *    @returns    ICRESULT    Result of device creation
 *
**/
ICRESULT icGXDeviceGL::CreateDevice()
{
#if (defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW32__))
    HWND hwnd = ((icWindowWIN*)m_pMainWindow)->GetHWND();
    m_hDC = GetDC(hwnd);

    int nPixelFormat;

    static PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),    // Size of this structure
        1,                                // Version of this structure
        PFD_DRAW_TO_WINDOW |            // Draw to window (can be bitmap)
        PFD_SUPPORT_OPENGL |            // Support openGL calls in window
        PFD_DOUBLEBUFFER |                // Double-buffered mode
        PFD_TYPE_RGBA,                    // RGBA color mode
        32,                                // color depth
        0,0,0,0,0,0,                    // not used to select mode
        0,0,                            // not used to select mode
        0,0,0,0,0,                        // not used to select mode
        16,                                // Size of depth buffer
        0,                                // not used here
        0,                                // not used here
        0,                                // not used here
        0,                                // not used here
        0,0,0 };                        // not used here

    // choose pixel format
    nPixelFormat = ChoosePixelFormat( m_hDC, &pfd );

    // set the pixel format
    SetPixelFormat(m_hDC, nPixelFormat, &pfd);

    m_hRC = wglCreateContext(m_hDC);
    wglMakeCurrent(m_hDC, m_hRC);

    icWindowDef windef;
    m_pMainWindow->GetParams(&windef);

    m_iWidth = windef.i16Width;
    m_iHeight = windef.i16Height;

    // Set the viewport to be the entire window
    glViewport(0, 0, m_iWidth, m_iHeight);

    icCreateOrthoOffset(0.0f,(icReal)m_iWidth,0.0f,(icReal)m_iHeight,
                        -1.0f,1.0f,m_screenOrtho);

    // obtain information about the clients opengl version/capabilities
    m_psGLVendor = (const char *)glGetString(GL_VENDOR);
    m_psGLRender = (const char *)glGetString(GL_RENDERER);
    m_psGLLIB = (const char*)glGetString(GL_VERSION);
    m_psGLExt = (const char*)glGetString(GL_EXTENSIONS);
    m_psGLShaderVer = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);

    // LOAD EXTENSIONS
    wglSwapIntervalEXT = (SWAPINTEXT)wglGetProcAddress("wglSwapIntervalEXT");

    m_bHardwareBuffers = 
        IsExtensionSupported("GL_ARB_vertex_buffer_object", m_psGLExt);
    if (m_bHardwareBuffers)
    {
        icVertBuffGL::m_bHardware = true;
        icIndexBufGL::m_bHardware = true;
        // TODO: verify these function pointers come back!
        glGenBuffers = (PFNGLGENBUFFERS)wglGetProcAddress("glGenBuffers");
        if (!glGenBuffers)
            glGenBuffers = (PFNGLGENBUFFERS)wglGetProcAddress("glGenBuffersARB");
        glBindBuffer = (PFNGLBINDBUFFER)wglGetProcAddress("glBindBuffer");
        if (!glBindBuffer)
            glBindBuffer = (PFNGLBINDBUFFER)wglGetProcAddress("glBindBufferARB");
        glBufferData = (PFNGLBUFFERDATA)wglGetProcAddress("glBufferData");
		if (!glBufferData)
			glBufferData = (PFNGLBUFFERDATA)wglGetProcAddress("glBufferDataARB");
        glDeleteBuffers = 
            (PFNGLDELETEBUFFERS)wglGetProcAddress("glDeleteBuffers");
        if (!glDeleteBuffers)
            glDeleteBuffers = 
                (PFNGLDELETEBUFFERS)wglGetProcAddress("glDeleteBuffersARB");
        glMapBuffer = (PFNGLMAPBUFFER)wglGetProcAddress("glMapBuffer");
		if (!glMapBuffer)
			glMapBuffer = (PFNGLMAPBUFFER)wglGetProcAddress("glMapBufferARB");
		glUnmapBuffer = (PFNGLUNMAPBUFFER)wglGetProcAddress("glUnmapBuffer");
        if (!glUnmapBuffer)
			glUnmapBuffer = (PFNGLUNMAPBUFFER)wglGetProcAddress("glUnmapBufferARB");
    }

    // TODO:
    // this needs to be optional, disables vertical sync
    if (wglSwapIntervalEXT)
        wglSwapIntervalEXT(0);


    
    //GLfloat ambient[] = {1.0f,1.0f,1.0f,1.0f};
    GLfloat fNoLight[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    GLfloat fLowLight[] = { 0.25f, 0.25f, 0.25f, 1.0f };
    GLfloat fBrightLight[] = { 1.0f, 1.0f, 1.0f, 1.0f };

    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, fNoLight);
    glLightfv(GL_LIGHT0, GL_AMBIENT, fLowLight);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, fBrightLight);
    glLightfv(GL_LIGHT0, GL_SPECULAR, fBrightLight);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
    glMaterialfv(GL_FRONT, GL_SPECULAR, fBrightLight);
    glMateriali(GL_FRONT, GL_SHININESS, 128);


    // todo, what if this fails?
    return IC_OK;
#elif (defined(__APPLE__))
    // TODO: expose this
    static GLint glAttributes[] = {      
        AGL_DOUBLEBUFFER, GL_TRUE,  // Double buffered
        AGL_RGBA,         GL_TRUE,  // Four component color
        AGL_DEPTH_SIZE,   16,       // 16-bit (at least) depth buffer
    AGL_NONE };                 // Terminator
    
    // Initialize OpenGL, set to one and only window
    // Choose pixelformat based on attribute list
    AGLPixelFormat openGLFormat = 
        aglChoosePixelFormat(NULL, 0, glAttributes);
    if(openGLFormat == NULL)
        return false;
    
    // HACK
    m_bHardwareBuffers = true;

    
    // Create the context
    m_aglContext = aglCreateContext(openGLFormat, NULL);
    if(m_aglContext == NULL)
        return IC_FAIL_CREATE_DEVICE;
    
    // No longer needed
    aglDestroyPixelFormat(openGLFormat);    
    
    // Point to window and make current
    aglSetWindowRef(m_aglContext, ((icWindowOSX*)m_pMainWindow)->GetRef());
    aglSetCurrentContext(m_aglContext);
    
    
    GLfloat fNoLight[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    GLfloat fLowLight[] = { 0.25f, 0.25f, 0.25f, 1.0f };
    GLfloat fBrightLight[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, fNoLight);
    glLightfv(GL_LIGHT0, GL_AMBIENT, fLowLight);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, fBrightLight);
    glLightfv(GL_LIGHT0, GL_SPECULAR, fBrightLight);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
    glMaterialfv(GL_FRONT, GL_SPECULAR, fBrightLight);
    glMateriali(GL_FRONT, GL_SHININESS, 128);
    
    return IC_OK;
#elif (defined(LINUX))
    return IC_FAIL_CREATE_DEVICE;
#else
    return IC_FAIL_CREATE_DEVICE;
#endif
}// END FUNCTION CreateDevice( void )


/*! Updates various stats in the engine
 *
 *    @returns    ICRESULT    Result of update
 *
**/
ICRESULT icGXDeviceGL::Update( void )
{
    return icGXDevice::Update();
}// END FUNCTION Update( void )


void icGXDeviceGL::ChangeSize(ushort sizeX, ushort sizeY)
{
    m_iWidth = (int)sizeX;
    m_iHeight = (int)sizeY;

    // Set the viewport to be the entire window
    glViewport(0, 0, m_iWidth, m_iHeight);

    icCreateOrthoOffset(0.0f,(icReal)m_iWidth,0.0f,(icReal)m_iHeight,
                        -1.0f,1.0f,m_screenOrtho);
}

/*! Clears the backbuffer and starts a new render frame
**/
void icGXDeviceGL::FrameBegin( void )
{
#if (defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW32__))
    HWND hwnd = ((icWindowWIN*)m_pMainWindow)->GetHWND();
    InvalidateRect(hwnd,NULL,FALSE);
#elif defined(__APPLE__)
    icWindowOSX* pWind = (icWindowOSX*)m_pMainWindow;
    //InvalWindowRgn(pWind->GetRef(),  NULL); // force redrow
#endif
    glClearColor(m_colClear.R(), m_colClear.G(),
                 m_colClear.B(), m_colClear.A());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


    GLfloat ambient[] = {m_colAmbient.R(),m_colAmbient.G(),m_colAmbient.B(),m_colAmbient.A()};
    GLfloat fNoLight[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    GLfloat fLowLight[] = { 0.25f, 0.25f, 0.25f, 1.0f };
    GLfloat fBrightLight[] = { 1.0f, 1.0f, 1.0f, 1.0f };

    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, fNoLight);
    glLightfv(GL_LIGHT0, GL_AMBIENT, fLowLight);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, fBrightLight);
    glLightfv(GL_LIGHT0, GL_SPECULAR, fBrightLight);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    static GLfloat fLightPos[4]   = { 0.0f, 20.0f, 0.0f, 1.0f };  // Point source
    //static icReal ang = 0.0f;

    //ang += 0.01f;
    //if (ang >= 2 * IC_PI)
    //    ang -= IC_PI;

    //fLightPos[0] = 20.0f * icCos(ang);
    //fLightPos[2] = 20.0f * icSin(ang);

    glLightfv(GL_LIGHT0, GL_POSITION, fLightPos);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
    glMaterialfv(GL_FRONT, GL_SPECULAR, fBrightLight);
    glMateriali(GL_FRONT, GL_SHININESS, 129);
}// END FUNCTION FrameBegin( void )


/*! Presents the current frame to the display
 *
 *    @param    bSwap    Set true to swap buffers
**/
void icGXDeviceGL::FrameEnd( bool bSwap )
{
#if (defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW32__))
    if (bSwap)
    {
        SwapBuffers(m_hDC);

        icWindowWIN* win = (icWindowWIN*)m_pMainWindow;
        ValidateRect(win->GetHWND(), NULL);
    }
    else
        glFinish();

#elif defined(__APPLE__)
    if (bSwap)
        aglSwapBuffers (m_aglContext);
    else
        glFlush();
    
#endif
    icGXDevice::FrameEnd(bSwap);
}// END FUNCTION FrameEnd( void )


/*! Renders a vertex buffer to the current frame
 *
 *  @param[in]    pVB             Pointer to the vertex buffer
 *  @returns      ICRESULT        Result of rendering the vert buffer
**/
ICRESULT icGXDeviceGL::DrawVertBuf(class icVertBuffer* pVB, uint offset, uint count)
{
#ifdef WIN32
    if (m_hDC)
#else
    if (m_aglContext)
#endif
    {
        count = (count) ? count : pVB->GetVertDef()->uiNumVerts-offset;

        if (!pVB)
            return IC_OK;

        if (pVB->IsLocked())
            return IC_WARN_ALREADY_LOCKED_VB;

        icVertBuffGL* glVB = (icVertBuffGL*)pVB;
        icVertDef* vd = pVB->GetVertDef();
        uint pos_sz = (vd->vertType > IC_VERT_2DCOL_DIF) ? 3 : 2;

        // TODO: save theses states so we don't needlessly change them
        glEnableClientState(GL_VERTEX_ARRAY);

        if (vd->vertType == IC_VERT_NRMCOL)
            glEnableClientState(GL_NORMAL_ARRAY);
        if (vd->vertType == IC_VERT_2DCOL || vd->vertType == IC_VERT_2DCOL_DIF ||
            vd->vertType == IC_VERT_NRMCOL || vd->vertType == IC_VERT_COL)
            glEnableClientState(GL_COLOR_ARRAY);
        if (vd->vertType != IC_VERT_2DCOL && vd->vertType == IC_VERT_2DCOL_DIF &&
            vd->vertType != IC_VERT_COL)
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);

        if (m_bHardwareBuffers)
        {
            glBindBuffer(GL_ARRAY_BUFFER, glVB->GetName());
            if (vd->vertType == IC_VERT_DIF)
            {
                glVertexPointer(pos_sz,GL_FLOAT,sizeof(ICVRT_DIF),(GLvoid*)NULL);
                glTexCoordPointer(2,GL_FLOAT, sizeof(ICVRT_DIF),(GLvoid*)(sizeof(icReal)*pos_sz));
            }
            else if (vd->vertType == IC_VERT_2DCOL)
            {
                glVertexPointer(pos_sz,GL_FLOAT,sizeof(ICVRT2DCOL),(GLvoid*)NULL);
                glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(ICVRT2DCOL), (GLvoid*)(sizeof(icReal)*pos_sz));
            }
            else if (vd->vertType == IC_VERT_2DCOL_DIF)
            {
                glVertexPointer(pos_sz,GL_FLOAT,sizeof(ICVRT2DCOL_DIF),(GLvoid*)NULL);
                glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(ICVRT2DCOL_DIF), (GLvoid*)(sizeof(icReal)*pos_sz));
                glTexCoordPointer(2,GL_FLOAT, sizeof(ICVRT2DCOL_DIF),(GLvoid*)(sizeof(icReal)*pos_sz + sizeof(icColor)));
            }
            else if (vd->vertType == IC_VERT_NRMCOL)
            {
                glVertexPointer(pos_sz,GL_FLOAT,sizeof(ICVRTNRMCOL),(GLvoid*)NULL);
                glNormalPointer(GL_FLOAT,sizeof(ICVRTNRMCOL),(GLvoid*)(sizeof(icVector3)));
                glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(ICVRTNRMCOL),(GLvoid*)(sizeof(icVector3)*2));
            }
            else if (vd->vertType == IC_VERT_COL)
            {
                glVertexPointer(pos_sz,GL_FLOAT,sizeof(ICVRTCOL),(GLvoid*)NULL);
                glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(ICVRTCOL),(GLvoid*)(sizeof(icReal)*pos_sz));
            }
        }
        else
        {
            *(int*)0=0;
            void* vb = glVB->GetLocked();
            glVertexPointer(3,GL_FLOAT,0,vb);
        }

        // render the buffer
        glDrawArrays(_ICPRIM_TO_GL(vd->primType), offset, count);

        // TODO: save theses states so we don't needlessly change them
        glDisableClientState(GL_VERTEX_ARRAY);
        if (vd->vertType == IC_VERT_NRMCOL)
            glDisableClientState(GL_NORMAL_ARRAY);
        if (vd->vertType == IC_VERT_2DCOL || vd->vertType == IC_VERT_2DCOL_DIF ||
            vd->vertType == IC_VERT_NRMCOL || vd->vertType == IC_VERT_COL)
            glDisableClientState(GL_COLOR_ARRAY);
        if (vd->vertType != IC_VERT_2DCOL && vd->vertType == IC_VERT_2DCOL_DIF &&
            vd->vertType != IC_VERT_COL)
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }
    return IC_FAIL_NO_DEVICE;
}// END FUNCTION DrawVertBuf(icVertBuffer*, icMaterial*)


/*! Renders an indexed vertex buffer to the current frame
 *
 *  @param[in]    pVB             Pointer to the vertex buffer
 *  @param[in]    pIB             Pointer to the index buffer
 *  @returns      ICRESULT        Result of rendering the vert buffer
**/
ICRESULT icGXDeviceGL::DrawIndexedVertBuf(class icVertBuffer* pVB,
                                          class icIndexBuf* pIB)
{
#ifdef WIN32
    if (m_hDC)
#else
    if (m_aglContext)
#endif
    {
        icVertBuffGL* glVB = (icVertBuffGL*)pVB;
        icIndexBufGL* glIB = (icIndexBufGL*)pIB;

        icVertDef* pvd = glVB->GetVertDef();

        glEnableClientState(GL_VERTEX_ARRAY);
        if (pvd->vertType == IC_VERT_NRM_DIF ||
            pvd->vertType == IC_VERT_NRMCOL ||
            pvd->vertType == IC_VERT_NRMCOLSPC_DIF)
        {
            glEnable(GL_LIGHTING);
            glEnable(GL_LIGHT0);
            glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);
            glEnableClientState(GL_NORMAL_ARRAY);
        }

        if (pvd->vertType == IC_VERT_DIF || pvd->vertType == IC_VERT_NRM_DIF)
        {
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        }
        else if (pvd->vertType == IC_VERT_NRMCOL)
        {
            glEnableClientState(GL_COLOR_ARRAY);
        }

        icIndexDef* id = pIB->GetIndexDef();
        if (m_bHardwareBuffers)
        {
            uint pos_sz = (pvd->vertType > IC_VERT_2DCOL_DIF) ? 3 : 2;

            // bind the vertex buffer
            glBindBuffer(GL_ARRAY_BUFFER, glVB->GetName());

            glVertexPointer(pos_sz,GL_FLOAT,pvd->uiVertSize,NULL);
            
            if (pvd->vertType == IC_VERT_NRM_DIF ||
                pvd->vertType == IC_VERT_NRMCOL ||
                pvd->vertType == IC_VERT_NRMCOLSPC_DIF)
            {
                glNormalPointer(GL_FLOAT,pvd->uiVertSize,(GLvoid*)(sizeof(icReal)*pos_sz));
            }

            if (pvd->vertType == IC_VERT_DIF)
            {
                glTexCoordPointer(2,GL_FLOAT, pvd->uiVertSize,(GLvoid*)(sizeof(icReal)*pos_sz));
            }
            else if (pvd->vertType == IC_VERT_NRM_DIF)
            {
                glTexCoordPointer(2,GL_FLOAT, pvd->uiVertSize,(GLvoid*)(sizeof(icVector3) + sizeof(icReal)*pos_sz));
            }
            else if (pvd->vertType == IC_VERT_NRMCOL)
            {
                glColorPointer(3, GL_UNSIGNED_BYTE, pvd->uiVertSize, (GLvoid*)(sizeof(icVector3) + sizeof(icReal)*pos_sz));
            }

            // bind the index buffer
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glIB->GetName());
            
            if (id->indexSize == IC_INDEX_SIZE_32)
                glDrawElements(GL_TRIANGLES, id->numIndex, GL_UNSIGNED_INT, NULL);
            else
                glDrawElements(GL_TRIANGLES, id->numIndex, GL_UNSIGNED_SHORT, NULL);

            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        }
        else
        {
            // TODO: IMPLEMENT!
            void* vb = ((icVertBuffGL*)pVB)->GetLocked();
            glVertexPointer(3,GL_FLOAT,0,vb);

            void* ib = ((icIndexBufGL*)pIB)->GetLocked();
            glIndexPointer(GL_SHORT,0,ib);

            // RENDER?
            glDrawElements(GL_TRIANGLE_STRIP, id->numIndex, GL_UNSIGNED_BYTE, ib);
        }

        glDisableClientState(GL_VERTEX_ARRAY);
        if (pvd->vertType == IC_VERT_NRM_DIF ||
            pvd->vertType == IC_VERT_NRMCOL ||
            pvd->vertType == IC_VERT_NRMCOLSPC_DIF)
        {
            glDisable(GL_LIGHTING);
            glDisable(GL_LIGHT0);
            glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SINGLE_COLOR);
            glDisableClientState(GL_NORMAL_ARRAY);
        }

        if (pvd->vertType == IC_VERT_DIF || pvd->vertType == IC_VERT_NRM_DIF)
        {
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        }
        else if (pvd->vertType == IC_VERT_NRMCOL)
        {
            glDisableClientState(GL_COLOR_ARRAY);
        }
        return IC_OK;
    }
    return IC_FAIL_NO_DEVICE;
}// END FUNCTION DrawIndexedVertBuf(icVertBuffer*, icIndexBuf*, icMaterial*)

/*! Creates an openGL vertex buffer
 *
 *    @param[out]    ppVB        Pointer to ouput VB pointer
 *    @param[in]    pVertDef    Information about the VB
 *    @returns    ICRESULT    Result of creating VB
 *
**/
ICRESULT icGXDeviceGL::CreateVB(class icVertBuffer** ppVB,
                                struct icVertDef* pVertDef)
{
#ifdef WIN32
    if (m_hDC)
#else
    if (m_aglContext)
#endif
    {
        icVertBuffGL* vb = new icVertBuffGL();
        if (vb)
        {
            GLuint& buf_name = vb->GetName();
            GLenum usage = GL_STATIC_DRAW;//(pVertDef->usage == IC_VERT_USAGE_DYNAMIC)? GL_STREAM_DRAW : GL_STATIC_DRAW;

            if (m_bHardwareBuffers)
            {
                glGenBuffers(1,&buf_name);
                glBindBuffer(GL_ARRAY_BUFFER,buf_name);
                glBufferData(GL_ARRAY_BUFFER,
                             pVertDef->uiVertSize*pVertDef->uiNumVerts,
                             NULL, usage);
            }

            *ppVB = vb;
            memcpy(vb->GetVertDef(), pVertDef, sizeof(icVertDef));

            return IC_OK;
        }
        return IC_FAIL_OUT_OF_MEM;
    }
    return IC_FAIL_NO_DEVICE;
}// END FUNCTION CreateVB( class icVertBuffer** ppVB )


/*! Releases vertex buffer memory back to gpu
 *
 *  @param[in]    pVB            Pointer to VB to release
 *  @returns    ICRESULT    Result of releasing VB
**/
ICRESULT icGXDeviceGL::ReleaseVB(class icVertBuffer* pVB)
{
    if (pVB)
        delete pVB;

    return IC_OK;
}// END FUNCTION ReleaseVB(class icVertBuffer* pVB)


/*! Creates an openGl hardware index buffer
 *
 *    @param[out]   ppIB        Pointer to ouput IB pointer
 *    @param[in]    pIndexDef   Information about the IB
 *    @returns      ICRESULT    Result of creating IB
 *
**/
ICRESULT icGXDeviceGL::CreateIB(class icIndexBuf** ppIB,
                                struct icIndexDef* pIndexDef)
{
#ifdef WIN32
    if (m_hDC)
#else
    if (m_aglContext)
#endif
    {
        icIndexBufGL* ib = new icIndexBufGL();
        if (ib)
        {
            GLuint& buf_name = ib->GetName();
            GLenum usage = GL_STATIC_DRAW;//(pIndexDef->usage == IC_INDEX_USAGE_DYNAMIC) ?
                                        //GL_STREAM_DRAW : GL_STATIC_DRAW;

            GLsizeiptr size = (pIndexDef->indexSize == IC_INDEX_SIZE_16) ? 
                        2 * pIndexDef->numIndex : 4 * pIndexDef->numIndex;

            // TODO: FIX THIS
            if (m_bHardwareBuffers)
            {
                glGenBuffers(1,&buf_name);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,buf_name);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, NULL, usage);
            }

            *ppIB = ib;
            memcpy(ib->GetIndexDef(), pIndexDef, sizeof(icIndexDef));

            return IC_OK;
        }
        return IC_FAIL_OUT_OF_MEM;
    }
    return IC_FAIL_NO_DEVICE;
}// END FUNCTION CreateIB( icIndexBuf**, icIndexDef* )


/*! Releases index buffer from memory
 *
 *  @param[in]    pIB         Pointer to index buffer
 *  @returns      ICRESULT    Result of index buffer release
**/
ICRESULT icGXDeviceGL::ReleaseIB(class icIndexBuf* pIB)
{
    if (pIB)
        delete pIB;

    return IC_OK;
}// END FUNCTION ReleaseIB(class icIndexBuf* pIB)


/*! Creates an openGL texture
 *
 *    @param[out]   ppTex       Pointer to ouput texture pointer
 *    @param[in]    pTexDef     Information about the texture
 *    @returns      ICRESULT    Result of creating texture
 *
**/
ICRESULT icGXDeviceGL::CreateTex(class icTexture** ppTex,
                                 struct icTexDef* pTexDef)
{
#ifdef WIN32
    if (m_hDC)
#else
    if (m_aglContext)
#endif
    {
        icTextureGL* tex = new icTextureGL();
        if (tex)
        {
            GLuint& buf_name = tex->GetName();

            glGenTextures(1,&buf_name);
            glBindTexture(GL_TEXTURE_2D,buf_name); // needs exposed more

            *ppTex = tex;
            memcpy(tex->GetTexDef(), pTexDef, sizeof(icTexDef));

            return IC_OK;
        }
        return IC_FAIL_OUT_OF_MEM;
    }
    return IC_FAIL_NO_DEVICE;
}// END FUNCTION CreateTex(class icTextue**, struct icTexDef)


/*! Creates an icTexture from a file
 *
 *    @param[in/out]    ppTex       Pointer to ouput texture pointer
 *    @param[in]        szFile      Name of file to load as texture
 *    @returns          ICRESULT    Result of texture creation
 *
**/
ICRESULT icGXDeviceGL::CreateTexFromFile(class icTexture** ppTex,
                                         const char* szFile)
{
#ifdef WIN32
    if (m_hDC)
#else
    if (m_aglContext)
#endif
    {
        icTextureGL* tex = new icTextureGL();
        if (tex)
        {
            GLuint& buf_name = tex->GetName();

            glGenTextures(1,&buf_name);
            glBindTexture(GL_TEXTURE_2D,buf_name); // needs exposed more

            // get the actual image bits
            icImage image_loader;
            if (ICEFAIL(image_loader.ReadHeader(szFile)))
            {
                glDeleteTextures(1,&buf_name);
                delete tex;
                icWarningf("GXDevice could not read (%s) file header", szFile);
                return IC_FAIL_GEN;
            }

            // TODO: use PBOs so that we don't have to do this
            uint size = 0;
            image_loader.GetImageDataSize(&size);
            GLbyte* bits = (GLbyte*)malloc(size);
            if (!bits)
            {
                glDeleteTextures(1,&buf_name);
                delete tex;
                return IC_FAIL_OUT_OF_MEM;
            }
            image_loader.CopyImageBits(bits);

            _GL_TEX_INFO info;

            image_loader.GetGLTexInfo(&info);

            // Load texture, set filter and wrap modes
            glTexImage2D(GL_TEXTURE_2D, 0, info.comp,
                         info.width, info.height, 0,
                         info.comp, GL_UNSIGNED_BYTE, bits);
            gluBuild2DMipmaps(GL_TEXTURE_2D, info.comp,
                              info.width, info.height,
                             info.format, GL_UNSIGNED_BYTE, bits);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                            GL_LINEAR_MIPMAP_LINEAR);

            free(bits);

            *ppTex = tex;
            icTexDef* def = tex->GetTexDef();

            switch (info.depth)
            {
            case 1:def->format = ICTEX_FMT_LUM8; break;
            //case 2:def->format = ICTEX_FMT_RBGA; break;
            case 3:def->format = ICTEX_FMT_RBG24; break;
            case 4:def->format = ICTEX_FMT_RBGA32; break;
            default: icError("nope");
            }
            
            def->width = info.width;
            def->height = info.height;
            def->usage = 0;

#ifdef _MSC_VER
            strcpy_s(tex->m_szName, 256,szFile);
#else
            strcpy(tex->m_szName, szFile);
#endif

            return IC_OK;
        }
        return IC_FAIL_OUT_OF_MEM;
    }
    return IC_FAIL_NO_DEVICE;
}// END FUNCTION CreateTexFromFile(class icTexutre** ppTex, const char* szFile)


/*! Releases a texture from graphics memory
 *
 *    @param[in]    pTex        Pointer to texture to release
 *    returns       ICRESULT    Result of texture release
**/
ICRESULT icGXDeviceGL::ReleaseTex(class icTexture* pTex)
{
    GLuint& glname = ((icTextureGL*)pTex)->GetName();
    if (glname != 0xFFFFFFFF)
        glDeleteTextures(1, &glname);
    delete pTex;
    return IC_OK;
}// END FUNCTION ReleaseTex(class icTexture* pTex)

ICRESULT icGXDeviceGL::CreateFont(class icFontI** ppIFont,
                                   struct icFontParams* pFontParams)
{
	icFontGL* font = new icFontGL();
	if (ICEOK(font->Initialize(pFontParams->_fontName.ToChar(), pFontParams->_size, this)))
	{
		*ppIFont = font;
		return IC_OK;
	}
	return IC_FAIL_GEN;
}

ICRESULT icGXDeviceGL::ReleaseFont( class icFontI* ppIFont )
{
	delete ppIFont;
	return IC_OK;
}

/*! Toggles depth test
 *
 *  @param      bDepth      Boolean, true - enable depth testing
 *  @returns    ICRESULT    Success/failure of depth test state change
**/
ICRESULT icGXDeviceGL::EnableDepthTest(bool bDepth)
{
#ifdef WIN32
    if (m_hDC)
#else
    if (m_aglContext)
#endif
    {
        m_bDepthTest = bDepth;
        if (m_bDepthTest)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);

        // TODO: expose this function
        glDepthFunc(GL_LESS);

        return IC_OK;
    }
    return IC_FAIL_NO_DEVICE;
}// END FUNCTION EnableAlphaBlend(bool bAlphaBlend)


/*! Toggles fog, on or off
 *
 *  @param      bEnable         Set - true to enable
 *  @param      params          Fog parameters
 *  @returns    ICRESULT        Success/failure of operation
**/
ICRESULT icGXDeviceGL::EnableFog(bool bEnable, const icFogParams& params)
{
    if (m_bFogEnable == bEnable && params == m_FogParams)
        return IC_OK;

    m_bFogEnable = bEnable;

    const icColor& c = params.col;
    GLfloat fog_color[] = {c.R(),c.G(),c.B(),c.A()};

    if (bEnable)
    {
        glEnable(GL_FOG);                   // Turn Fog on
        glFogfv(GL_FOG_COLOR, fog_color);   // Set fog color to match background
        glFogf(GL_FOG_DENSITY, params.density);
        glFogf(GL_FOG_START, params.start);         // How far away does the fog start
        glFogf(GL_FOG_END, params.end);          // How far away does the fog stop
        switch(params.type)
        {
        case 1: glFogi(GL_FOG_MODE, GL_LINEAR); break;
        case 2: glFogi(GL_FOG_MODE, GL_EXP); break;
        case 3: glFogi(GL_FOG_MODE, GL_EXP2); break;
        default: glFogi(GL_FOG_MODE, GL_LINEAR);
        }
    }
    else
        glDisable(GL_FOG);

    return IC_OK;
}// END FUNCTION EnableFog(bool bEnable, const icFogParams& params)


/*! Toggles alpha blending
 *
 *  @param      bDepth      Boolean, true - enable alpha blending
 *  @returns    ICRESULT    Success/failure of alpha blending state change
**/
ICRESULT icGXDeviceGL::EnableAlphaBlend(bool bAlphaBlend)
{
    if (bAlphaBlend)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    else
    {
        glDisable(GL_BLEND);
    }
    return IC_OK;
}// END FUNCTION EnableAlphaBlend(bool bAlphaBlend)


/*! Sets the ambient light color
 *
 *    @param        col         Desired ambient color
 *    @returns      ICRESULT    Result of ambient color change request
**/
ICRESULT icGXDeviceGL::SetAmbientColor(const icColor& col)
{
    m_colAmbient = col;
    return IC_FAIL_NO_DEVICE;
}// END FUNCTION SetAmbientColor(const icColor& col)


/*! Sets the ambient light color
 *
 *  @param        col         Desired clear color
 *  @returns      ICRESULT    Result of clear color change request
**/
ICRESULT icGXDeviceGL::SetClearColor(const icColor& col)
{
    m_colClear = col;
    return IC_OK;
}// END FUNCTION SetClearColor(const icColor& col)


/*! Sets the device triangle cull mode
 *
 *    @param[in]    cMode_e     Tells the winding order for culling
 *    @returns      ICRESULT    Result of cull mode change request
**/
ICRESULT icGXDeviceGL::SetCullMode(IC_CULL_MODE cMode_e)
{
    if (cMode_e == IC_CULL_ANTICLOCK)
    {
        glEnable(GL_CULL_FACE);
        glFrontFace(GL_CCW);
    }
    else if (cMode_e == IC_CULL_CLOCKWISE)
    {
        glEnable(GL_CULL_FACE);
        glFrontFace(GL_CW);
    }
    else if (cMode_e == IC_CULL_OFF)
        glDisable(GL_CULL_FACE);
    else
        return IC_WARN_GEN; // invalid cull mode submitted
    return IC_OK;
}// END FUNCTION SetCullMode(IC_CULL_MODE cMode_e)


/*! Sets the projection matrix
 *
 *    @param        proj        Projection matrix
 *    @result       ICRESULT    Result of projection change
 *
**/
ICRESULT icGXDeviceGL::SetProjection(const icMatrix44& proj)
{
    glMatrixMode(GL_PROJECTION);

#ifdef IC_MATH_USE_DOUBLE
    glLoadMatrixd(proj);
#else
    glLoadMatrixf(proj);
#endif

    return IC_OK;
}// END FUNCTION SetProjection(const icMatrix44& proj)


/*! Sets the view matrix
 *
 *    @param        view        Current view matrix
 *    @returns      ICRESULT    Result of the transform change
 *
**/
ICRESULT icGXDeviceGL::SetViewMatrix(const icMatrix44& view)
{
    glMatrixMode(GL_MODELVIEW);

    icMatrix44 temp = view.Inverse();

    // set view matrix
    glPopMatrix();
#ifdef IC_MATH_USE_DOUBLE
    glLoadMatrixd(temp);
#else
    glLoadMatrixf(temp);
#endif
    glPushMatrix();

    return IC_OK;
}// END FUNCTION SetViewMatrix(const icMatrix44& view)


/*! Sets the world transformation
 *
 *    @param        world       World rotation/position
 *    @returns      ICRESULT    Result of the transform change
 *
**/
ICRESULT icGXDeviceGL::SetWorldTransform(const icMatrix44& world)
{
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPushMatrix();
#ifdef IC_MATH_USE_DOUBLE
    glMultMatrixd(world);
#else
    glMultMatrixf(world);
#endif

    return IC_OK;
}// END FUNCTION SetWorldTransform(const icTransform& world)


/*! Sets render texture by index
 *
 *  @param      index       Index of render texture to set
 *  @param      ptex        Texture to use
 *  @returns    ICRESULT    Result of setting render texture
**/
ICRESULT icGXDeviceGL::SetTexture(uint index, icTexture* pTex)
{
#ifdef WIN32
    if (m_hDC)
#else
    if (m_aglContext)
#endif
    {
        if (pTex)
        {
            glEnable(GL_TEXTURE_2D);
            if (pTex->HasAlpha())
            {
                glEnable(GL_BLEND);
                glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
            else
            {
                glDisable(GL_BLEND);
            }

            glBindTexture(GL_TEXTURE_2D, ((icTextureGL*)pTex)->GetName());
        }
        else
        {
            glDisable(GL_BLEND);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        return IC_OK;
    }
    return IC_FAIL_NO_DEVICE;
}// END FUNCTION SetTexture(uint index, icTexture* pTex)
#endif //#ifdef ICGL


