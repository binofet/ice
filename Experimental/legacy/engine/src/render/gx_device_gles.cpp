
#include "ice/render/gx_device_gles.h"
namespace ice::render {

#ifdef ICGLES
} // namespace ice::render

#include "ice/render/index_buf.h"
#include "ice/render/index_buf_gles.h"
#include "ice/render/vert_buffer_gles.h"

#include <stdlib.h>
#include <string.h>
namespace ice::render {

/*! Default c'tor
 **/
gx_device_gles::gx_device_gles(void) {} // END FUNCTION icGXDeviceGLES( void )

/*! Default d'tor
 **/
gx_device_gles::~gx_device_gles(void) { Cleanup(); } // END FUNCTION ~icGXDeviceGLES( void )

/*! Cleans up the engine
 *
 *    @returns    ICRESULT    Result of engine cleanup
 *
 **/
ice::result gx_device_gles::Cleanup(void) { return IC_FAIL_GEN; } // END FUNCTION Cleanup( void )

/*! Initializes the engine
 *
 *    @param[in]    hWnd            Pointer to the window
 *    @param[in]    bFullScreen        Will we render to the full screen
 *    @returns    ICRESULT        Result of initialization
 **/
ice::result gx_device_gles::Init(void *hWnd, bool bFullScreen) {
	m_Cullmode_e = ic_cull_anticlock;
	glCullFace(GL_BACK);

	return IC_OK;
} // END FUNCTION Init( void* hWnd, bool bFullScreen )

/*! Updates the engine
 *
 *    @returns    ICRESULT    Result of the update
 *
 **/
ice::result gx_device_gles::Update(void) { return IC_FAIL_GEN; } // END FUNCTION Update( void )

/*! Starts the render frame
 **/
void gx_device_gles::FrameBegin(void) {
	//    glClear(GL_DEPTH);
} // END FUNCTION FrameBegin( void )

/*! Ends the render frame
 **/
void gx_device_gles::FrameEnd(void) {} // END FUNCTION FrameEnd( void )

/*! Draws a vertex buffer
 *
 *    @param[in]    pVB            Pointer to the vertex buffer
 *    @returns    ICRESULT    Result of the draw call
 *
 **/
ice::result gx_device_gles::DrawVertBuf(class vert_buffer *pVB) {
	GLenum    mode    = 0;
	vert_def *vertDef = pVB->GetVertDef();
	switch (vertDef->primType) {
	case ic_prim_plist:
		mode = GL_POINTS;
		break;
	case IC_PRIM_LSTRIP:
		mode = GL_LINE_STRIP;
		break;
	case IC_PRIM_LLIST:
		mode = GL_LINES;
		break;
	case IC_PRIM_TFAN:
		mode = GL_TRIANGLE_FAN;
		break;
	case IC_PRIM_TLIST:
		mode = GL_TRIANGLES;
		break;
	case IC_PRIM_TSTRIP:
		mode = GL_TRIANGLE_STRIP;
		break;
	}
	glDrawArrays(mode, 0, vertDef->numPrims);
	return IC_FAIL_GEN;
} // END FUNCTION DrawVertBuf( class icVertBuffer* pVB )

/*! Draws an indexed vertex buffer
 *
 *    @param[in]    pVB            Pointer to the vertex buffer
 *    @param[in]    pIB            Pointer to the index buffer
 *    @returns    ICRESULT    Result of the draw call
 *
 **/
ice::result gx_device_gles::DrawIndexedVertBuf(class vert_buffer *pVB, class index_buf *pIB) {
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint) * (pVB->GetVertBuff()));

	glDrawElements(GL_TRIANGLES, (GLsizei) * (pIB->GetIndexBuffer()), GL_UNSIGNED_SHORT,
	               (void *)NULL);
	return IC_OK;
} // END FUNCTION DrawIndexedVertBuf( class icVertBuffer* pVB,
  //                                     class icIndexBuf* pIB )

/*! Creates a vertex buffer
 *
 *    @param[out]        ppVB        Pointer for the vertex buffer
 *    @param[in]        pVertDef    Vertex buffer creation information
 *    @returns        ICRESULT    Result of vertex buffer creation
 *
 **/
ice::result gx_device_gles::CreateVB(class vert_buffer **ppVB, struct vert_def *pVertDef) {
	assert(ppVB);
	assert(pVertDef);

	vert_buffer *vertBuf = new vert_buffer_gles();
	if (vertBuf) {
		void *mem = malloc(pVertDef->uiNumVerts * pVertDef->uiVertSize);
		if (mem) {
			*(vertBuf->GetVertBuff()) = mem;
			*(ppVB)                   = vertBuf;
			memcpy(vertBuf->GetVertDef(), pVertDef, sizeof(vert_def));
		}
		return IC_FAIL_OUT_OF_MEM;
	}
	return IC_FAIL_OUT_OF_MEM;
} // END FUNCTION CreateVB( class icVertBuffer** ppVB,
  //                           struct icVertDef* pVertDef )

/*! Releases a vertex buffer
 *
 *    @param[in]    pVB            Pointer to vertex buffer
 *    @returns    ICRESULT    Result of releasing the vb
 *
 **/
ice::result gx_device_gles::ReleaseVB(class vert_buffer *pVB) {
	assert(pVB);
	delete (vert_buffer_gles *)pVB;
	return IC_OK;
} // END FUNCTION ReleaseVB( class icVertBuffer* pVB )

/*! Creates an index buffer
 *
 *    @param[out]    ppIB        Pointer for the vertex buffer
 *    @param[in]    pIndexDef    Index buffer creation information
 *    @returns    ICRESULT    Result of index buffer creation
 *
 **/
ice::result gx_device_gles::CreateIB(class index_buf **ppIB, struct index_def *pIndexDef) {
	assert(ppIB);
	assert(pIndexDef);
	index_buf *index = new index_buf_gles();
	if (index) {
		ice::uint indSize = 2;
		if (pIndexDef->indexSize == ic_index_size_32)
			indSize = 4;
		void *mem = malloc(pIndexDef->numIndex * indSize);
		if (mem) {
			*(index->GetIndexBuffer()) = mem;
			*(ppIB)                    = index;
			memcpy(index->GetIndexDef(), pIndexDef, sizeof(index_buf));
		}
		return IC_FAIL_OUT_OF_MEM;
	}
	return IC_FAIL_OUT_OF_MEM;
} // END FUNCTION CreateIB( class icIndexBuf** ppIB,
  //                           struct icIndexDef* pIndexDef )

/*! Releases an index buffer
 *
 *    @param[in]    pIB            Pointer to index buffer
 *    @returns    ICRESULT    Result of releasing the ib
 *
 **/
ice::result gx_device_gles::ReleaseIB(class index_buf *pIB) {
	assert(pIB);
	delete (index_buf_gles *)pIB;
	return IC_OK;
} // END FUNCTION ReleaseIB( class icIndexBuf* pIB )

/*! Creates a texture
 *
 *    @param[out]        ppTex        Pointer for texture
 *    @param[in]        pTexDef        Texture creation information
 *    @returns        ICRESULT    Result of texture creation
 *
 **/
ice::result gx_device_gles::CreateTex(class texture **ppTex, struct tex_def *pTexDef) {
	return IC_FAIL_GEN;
} // END FUNCTION CreateTex( class icTexture** ppTex,
  //                            struct icTexDef* pTexDef )

/*! Releases a texture
 *
 *    @param[in]        pTex        Pointer to texture
 *    @returns        ICRESULT    Result of releasing the texture
 *
 **/
ice::result gx_device_gles::ReleaseTex(class texture *pTex) {
	return IC_FAIL_GEN;
} // END FUNCTION ReleaseTex( class icTexture* pTex )

/*! Sets the cull mode
 *
 *    @param            cMode_e        Cullmode
 *    @returns        ICRESULT    Result of setting the cullmode
 *
 **/
ice::result gx_device_gles::SetCullMode(ice::render::IC_CULL_MODE cMode_e) {
	if (m_Cullmode_e != cMode_e) {
		if (cMode_e == ic_cull_clockwise)
			glCullFace(GL_FRONT);
		else if (cMode_e == ic_cull_anticlock)
			glCullFace(GL_BACK);
		m_Cullmode_e = cMode_e;
		return IC_OK;
	}
	return IC_WARN_GEN;
} // END FUNCTION SetCullMode( IC_CULL_MODE cMode_e )

#endif // #ifdef ICGLES

} // namespace ice::render
