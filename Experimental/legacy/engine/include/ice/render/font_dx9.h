#pragma once
namespace ice::render {
#if defined(WIN32) && defined(ICDX9)
#ifndef __IC_FONT_DX9_H__
#define __IC_FONT_DX9_H__
} // namespace ice::render

#include "DirectX/Include/d3d9.h"
#include "DirectX/Include/d3dx9tex.h"

#include "ice/assets/content_loader.h"
#include "ice/render/font.h"
#include "ice/render/gx_device_dx9.h"
namespace ice::render {

class font_dx9 : font {
public:
	//////////////////////////////////////////////////////////////////////////
	// LIFECYCLE
	virtual ice::result Initialize(const char *szFileName, const int size,
	                               class graphics_device *pDevice);

	//////////////////////////////////////////////////////////////////////////
	// OPERATIONS
	virtual ice::result Print(const ice::math::ivec2 &v2ScreenPos, const ice::math::color &color,
	                          const char *buff);

	virtual ice::result Printf(const ice::math::ivec2 &v2ScreenPos, const ice::math::color &color,
	                           const char *buff, ...);

private:
	ID3DXFont *_font; // font
};

#endif //__IC_FONT_DX9_H__
#endif // WIN32

} // namespace ice::render
