
#include "Core/GXDevice/icGXDeviceGLES.h"

#ifdef ICGLES

#include "Render/VertBuffer/icVertBufferGLES.h"
#include "Render/IndexBuffer/icIndexBuf.h"
#include "Render/IndexBuffer/icIndexBufGLES.h"



#include <string.h>
#include <stdlib.h>

/*! Default c'tor
**/
icGXDeviceGLES::icGXDeviceGLES( void )
{
}// END FUNCTION icGXDeviceGLES( void )


/*! Default d'tor
**/
icGXDeviceGLES::~icGXDeviceGLES( void )
{
    Cleanup();
}// END FUNCTION ~icGXDeviceGLES( void )


/*! Cleans up the engine
 *
 *    @returns    ICRESULT    Result of engine cleanup
 *
**/
ICRESULT icGXDeviceGLES::Cleanup( void )
{
    return IC_FAIL_GEN;
}// END FUNCTION Cleanup( void )


/*! Initializes the engine
 *
 *    @param[in]    hWnd            Pointer to the window
 *    @param[in]    bFullScreen        Will we render to the full screen
 *    @returns    ICRESULT        Result of initialization
**/
ICRESULT icGXDeviceGLES::Init( void* hWnd, bool bFullScreen )
{
    m_Cullmode_e = IC_CULL_ANTICLOCK;
    glCullFace(GL_BACK);

    return IC_OK;
}// END FUNCTION Init( void* hWnd, bool bFullScreen )


/*! Updates the engine
 *
 *    @returns    ICRESULT    Result of the update
 *
**/
ICRESULT icGXDeviceGLES::Update( void )
{
    return IC_FAIL_GEN;
}// END FUNCTION Update( void )


/*! Starts the render frame
**/
void icGXDeviceGLES::FrameBegin( void )
{
//    glClear(GL_DEPTH);
}// END FUNCTION FrameBegin( void )


/*! Ends the render frame
**/
void icGXDeviceGLES::FrameEnd( void )
{
}// END FUNCTION FrameEnd( void )


/*! Draws a vertex buffer
 *
 *    @param[in]    pVB            Pointer to the vertex buffer
 *    @returns    ICRESULT    Result of the draw call
 *
**/
ICRESULT icGXDeviceGLES::DrawVertBuf( class icVertBuffer* pVB )
{
    GLenum mode=0;
    icVertDef* vertDef = pVB->GetVertDef();
    switch(vertDef->primType)
    {
    case IC_PRIM_PLIST: mode = GL_POINTS; break;
    case IC_PRIM_LSTRIP: mode = GL_LINE_STRIP; break;
    case IC_PRIM_LLIST: mode = GL_LINES; break;
    case IC_PRIM_TFAN: mode = GL_TRIANGLE_FAN; break;
    case IC_PRIM_TLIST: mode = GL_TRIANGLES; break;
    case IC_PRIM_TSTRIP: mode = GL_TRIANGLE_STRIP; break;
    }
    glDrawArrays(mode, 0, vertDef->numPrims);
    return IC_FAIL_GEN;
}// END FUNCTION DrawVertBuf( class icVertBuffer* pVB )


/*! Draws an indexed vertex buffer
 *
 *    @param[in]    pVB            Pointer to the vertex buffer
 *    @param[in]    pIB            Pointer to the index buffer
 *    @returns    ICRESULT    Result of the draw call
 *
**/
ICRESULT icGXDeviceGLES::DrawIndexedVertBuf( class icVertBuffer* pVB,
                                           class icIndexBuf* pIB )
{
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER,
                  (GLuint)*(pVB->GetVertBuff()) );

    glDrawElements( GL_TRIANGLES,
                    (GLsizei)*(pIB->GetIndexBuffer()),
                    GL_UNSIGNED_SHORT,
                    (void *)NULL );
    return IC_OK;
}// END FUNCTION DrawIndexedVertBuf( class icVertBuffer* pVB,
 //                                     class icIndexBuf* pIB )


/*! Creates a vertex buffer
 *
 *    @param[out]        ppVB        Pointer for the vertex buffer
 *    @param[in]        pVertDef    Vertex buffer creation information
 *    @returns        ICRESULT    Result of vertex buffer creation
 *
**/
ICRESULT icGXDeviceGLES::CreateVB( class icVertBuffer** ppVB,
                                 struct icVertDef* pVertDef )
{
    assert(ppVB);
    assert(pVertDef);

    icVertBuffer* vertBuf = new icVertBufferGLES();
    if (vertBuf)
    {
        void* mem = malloc(pVertDef->uiNumVerts * pVertDef->uiVertSize);
        if (mem)
        {
            *(vertBuf->GetVertBuff()) = mem;
            *(ppVB) = vertBuf;
            memcpy(vertBuf->GetVertDef(), pVertDef, sizeof(icVertDef));
        }
        return IC_FAIL_OUT_OF_MEM;
    }
    return IC_FAIL_OUT_OF_MEM;
}// END FUNCTION CreateVB( class icVertBuffer** ppVB,
 //                           struct icVertDef* pVertDef )


/*! Releases a vertex buffer
 *
 *    @param[in]    pVB            Pointer to vertex buffer
 *    @returns    ICRESULT    Result of releasing the vb
 *
**/
ICRESULT icGXDeviceGLES::ReleaseVB( class icVertBuffer* pVB )
{
    assert(pVB);
    delete (icVertBufferGLES*)pVB;
    return IC_OK;
}// END FUNCTION ReleaseVB( class icVertBuffer* pVB )


/*! Creates an index buffer
 *
 *    @param[out]    ppIB        Pointer for the vertex buffer
 *    @param[in]    pIndexDef    Index buffer creation information
 *    @returns    ICRESULT    Result of index buffer creation
 *
**/
ICRESULT icGXDeviceGLES::CreateIB( class icIndexBuf** ppIB,
                                 struct icIndexDef* pIndexDef )
{
    assert(ppIB);
    assert(pIndexDef);
    icIndexBuf* index = new icIndexBufGLES();
    if (index)
    {
        uint indSize = 2;
        if (pIndexDef->indexSize == IC_INDEX_SIZE_32)
            indSize = 4;
        void* mem = malloc(pIndexDef->numIndex * indSize);
        if (mem)
        {
            *(index->GetIndexBuffer()) = mem;
            *(ppIB) = index;
            memcpy(index->GetIndexDef(), pIndexDef, sizeof(icIndexBuf));
        }
        return IC_FAIL_OUT_OF_MEM;
    }
    return IC_FAIL_OUT_OF_MEM;
}// END FUNCTION CreateIB( class icIndexBuf** ppIB,
 //                           struct icIndexDef* pIndexDef )


/*! Releases an index buffer
 *
 *    @param[in]    pIB            Pointer to index buffer
 *    @returns    ICRESULT    Result of releasing the ib
 *
**/
ICRESULT icGXDeviceGLES::ReleaseIB( class icIndexBuf* pIB )
{
    assert(pIB);
    delete (icIndexBufGLES*)pIB;
    return IC_OK;
}// END FUNCTION ReleaseIB( class icIndexBuf* pIB )


/*! Creates a texture
 *
 *    @param[out]        ppTex        Pointer for texture
 *    @param[in]        pTexDef        Texture creation information
 *    @returns        ICRESULT    Result of texture creation
 *
**/
ICRESULT icGXDeviceGLES::CreateTex( class icTexture** ppTex,
                                  struct icTexDef* pTexDef )
{
    return IC_FAIL_GEN;
}// END FUNCTION CreateTex( class icTexture** ppTex,
 //                            struct icTexDef* pTexDef )


/*! Releases a texture
 *
 *    @param[in]        pTex        Pointer to texture
 *    @returns        ICRESULT    Result of releasing the texture
 *
**/
ICRESULT icGXDeviceGLES::ReleaseTex( class icTexture* pTex )
{
    return IC_FAIL_GEN;
}// END FUNCTION ReleaseTex( class icTexture* pTex )


/*! Sets the cull mode
 *
 *    @param            cMode_e        Cullmode
 *    @returns        ICRESULT    Result of setting the cullmode
 *
**/
ICRESULT icGXDeviceGLES::SetCullMode( IC_CULL_MODE cMode_e )
{
    if (m_Cullmode_e != cMode_e)
    {
        if (cMode_e == IC_CULL_CLOCKWISE)
            glCullFace(GL_FRONT);
        else if (cMode_e == IC_CULL_ANTICLOCK)
            glCullFace(GL_BACK);
        m_Cullmode_e = cMode_e;
        return IC_OK;
    }
    return IC_WARN_GEN;
}// END FUNCTION SetCullMode( IC_CULL_MODE cMode_e )

#endif //#ifdef ICGLES

