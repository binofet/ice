#include "ice/render/font_dx9.h"
namespace ice::render {

#ifdef ICDX9

ice::result font_dx9::Initialize(const char *szFileName, const int size,
                                 class graphics_device *pDevice) {
	//_font = NULL;
	// icGXDeviceDX9* pDevice = (icGXDeviceDX9*)pContent->GetDevice();
	// if (pDevice)
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

ice::result font_dx9::Print(const ice::math::ivec2 &v2ScreenPos, const ice::math::color &color,
                            const char *buff) {
	// int width, height;
	// RECT font_rect = {0,0,width,height}; //sets the size of our font surface      rect

	////set the destination for our text, by moving the font rect.
	// SetRect(&font_rect, //our font rect
	//         v2ScreenPos.x, //Left
	//         v2ScreenPos.y, //Top
	//         width, //width
	//         height //height
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

ice::result font_dx9::Printf(const ice::math::ivec2 &v2ScreenPos, const ice::math::color &color,
                             const char *buff, ...) {
	return IC_OK;
}

#endif // ICDX9
} // namespace ice::render
