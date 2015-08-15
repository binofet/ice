#include "Render/Font/icFontDX9.h"

#ifdef ICDX9

ICRESULT icFontDX9::Initialize(const char* szFileName, const int size,
                               class icGXDevice* pDevice)
{
	//_font = NULL;
	//icGXDeviceDX9* pDevice = (icGXDeviceDX9*)pContent->GetDevice();
	//if (pDevice)
	//{
	//	D3DXCreateFont(pDevice->GetDevice(),
	//	               size,
	//	               0,
	//	               FW_NORMAL,
	//	               1,
	//	               false,
	//	               DEFAULT_CHARSET,
	//	               OUT_DEFAULT_PRECIS,
	//	               ANTIALIASED_QUALITY,
	//	               DEFAULT_PITCH|FF_DONTCARE,
	//	               "Arial",
	//	               &_font);

	//	return _font ? IC_OK : IC_FAIL_GEN;
	//}
	return IC_FAIL_NO_DEVICE;
}

ICRESULT icFontDX9::Print(const icVector2Int& v2ScreenPos,
                          const icColor& color,
                          const char* buff)
{
	//int width, height;
	//RECT font_rect = {0,0,width,height}; //sets the size of our font surface      rect

	////set the destination for our text, by moving the font rect.
	//SetRect(&font_rect, //our font rect
	//        v2ScreenPos.x, //Left
	//        v2ScreenPos.y, //Top
	//        width, //width
	//        height //height
	//);

	////draw our text
	//_font->DrawText(NULL, //pSprite
	//                text.c_str(), //pString
	//                -1, //Count
	//                &font_rect, //pRect
	//                DT_LEFT|DT_NOCLIP,//Format,
	//                color.ARGB());

	return IC_OK;
}

ICRESULT icFontDX9::Printf(const icVector2Int& v2ScreenPos,
                           const icColor& color,
                           const char* buff, ...)
{
	return IC_OK;
}

#endif // ICDX9