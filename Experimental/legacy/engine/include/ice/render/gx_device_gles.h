#pragma once

#include "ice/render/graphics_device.h"
namespace ice::render {

#ifdef ICGLES
} // namespace ice::render

#include <OpenGLES/ES1/gl.h>
#include <OpenGLES/ES1/glext.h>
namespace ice::render {

class gx_device_gles : public graphics_device {
public:
	//////////////////////////////////////////////////////////////////////
	// LIFECYCLE
	gx_device_gles(void);
	virtual ~gx_device_gles(void);

	virtual ice::result Cleanup(void);
	virtual ice::result Init(void *hWnd, bool bFullScreen);

	//////////////////////////////////////////////////////////////////////
	// OPERATIONS
	virtual ice::result Update(void);

	virtual void FrameBegin(void);
	virtual void FrameEnd(void);

	virtual ice::result DrawVertBuf(class vert_buffer *pVB);

	virtual ice::result DrawIndexedVertBuf(class vert_buffer *pVB, class index_buf *pIB);

	virtual ice::result CreateVB(class vert_buffer **ppVB, struct vert_def *pVertDef);
	virtual ice::result ReleaseVB(class vert_buffer *pVB);

	virtual ice::result CreateIB(class index_buf **ppIB, struct index_def *pIndexDef);
	virtual ice::result ReleaseIB(class index_buf *pIB);

	virtual ice::result CreateTex(class texture **ppTex, struct tex_def *pTexDef);
	virtual ice::result ReleaseTex(class texture *pTex);

	//////////////////////////////////////////////////////////////////////
	// ACCESS
	virtual ice::result SetCullMode(ice::render::IC_CULL_MODE cMode_e);

private:
};

#endif //__IC_GDEVICE_GLES_H__

} // namespace ice::render
