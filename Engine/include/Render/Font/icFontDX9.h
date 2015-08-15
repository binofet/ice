#ifdef WIN32
#ifndef __IC_FONT_DX9_H__
#define __IC_FONT_DX9_H__

#include "DirectX/Include/d3d9.h"
#include "DirectX/Include/d3dx9tex.h"

#include "icFont.h"
#include "Core/GXDevice/icGXDeviceDX9.h"
#include "Content/icContentLoader.h"

class icFontDX9 : icFontI
{
public:
	//////////////////////////////////////////////////////////////////////////
	// LIFECYCLE
	virtual ICRESULT Initialize(const char* szFileName, const int size,
								class icGXDevice* pDevice);

	//////////////////////////////////////////////////////////////////////////
	// OPERATIONS
	virtual ICRESULT Print(const icVector2Int& v2ScreenPos, const icColor& color,
						   const char* buff);

	virtual ICRESULT Printf(const icVector2Int& v2ScreenPos, const icColor& color,
							const char* buff, ...);

private:
	ID3DXFont* _font; //font
};

#endif //__IC_FONT_DX9_H__
#endif // WIN32
