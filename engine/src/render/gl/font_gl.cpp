namespace ice::render::gl {
#ifdef __APPLE__
} // namespace ice::render::gl
#include <CoreText/CoreText.h>
#include <vector>
namespace ice::render::gl {
#endif
} // namespace ice::render::gl

#include "ice/render/gl/font_gl.h"
namespace ice::render::gl {

#ifdef ICGL
} // namespace ice::render::gl

#include "ice/render/gl/graphics_device.h"
namespace ice::render::gl {
#ifdef X11
} // namespace ice::render::gl
#include <GL/glx.h>
#include <X11/Xlib.h>
namespace ice::render::gl {
#endif

#ifdef WIN32
} // namespace ice::render::gl
#include <windows.h>
namespace ice::render::gl {
#endif

/*! Default c'tor
 **/
font_gl::font_gl() {
	base_    = -1;
	texture_ = 0;
	size_    = 24;
}

/*! Default d'tor
 *
 *  Releases initialized font (if any)
 *
 **/
font_gl::~font_gl(void) {
	if (texture_)
		glDeleteTextures(1, &texture_);
	if (base_ > -1)
		glDeleteLists(base_, 96);
}

/// Create font resources using a borrowed graphics device. size is the logical glyph height;
/// file_name selects a native font family. Release before destroying the device/context.
ice::result font_gl::initialize(const char *file_name [[maybe_unused]], const int size,
                                ice::render::graphics_device *device) {
	gx_device_ = device;
#ifdef WIN32
	base_ = glGenLists(256);

	HFONT old_font;

	HFONT font = CreateFont(size,                        // Height
	                        0,                           // Width
	                        0,                           // Angle of Escapement
	                        0,                           // Orientation Angle
	                        FW_BOLD,                     // Font Weight
	                        FALSE,                       // Italic
	                        FALSE,                       // Underline
	                        FALSE,                       // Strikeout
	                        ANSI_CHARSET,                // Charater set
	                        OUT_TT_PRECIS,               // Output Precision
	                        CLIP_DEFAULT_PRECIS,         // Clipping Precision
	                        ANTIALIASED_QUALITY,         // Output Quality
	                        FF_DONTCARE | DEFAULT_PITCH, // Famimly and Pitch
	                        file_name);

	if (!font)
		return IC_FAIL_GEN;

	graphics_device *glDevice = (graphics_device *)gx_device_;

	HDC hDC = glDevice->get_device_context();

	if (!hDC)
		return IC_FAIL_GEN;

	old_font = (HFONT)SelectObject(hDC, font);
	wglUseFontBitmaps(hDC, 32, 96, base_);
	SelectObject(hDC, old_font);
	DeleteObject(font);
#elif defined(__APPLE__)
	size_                           = size;
	const int                  cell = 64, width = 1024, height = 512;
	std::vector<unsigned char> pixels(width * height * 4, 0);
	CGColorSpaceRef            color_space = CGColorSpaceCreateDeviceRGB();
	CGContextRef context = CGBitmapContextCreate(pixels.data(), width, height, 8, width * 4,
	                                             color_space, kCGImageAlphaPremultipliedLast);
	CGColorSpaceRelease(color_space);
	CFStringRef name = CFStringCreateWithCString(nullptr, file_name, kCFStringEncodingUTF8);
	CTFontRef   font = CTFontCreateWithName(name, size, nullptr);
	CFRelease(name);
	if (!context || !font) {
		if (context)
			CGContextRelease(context);
		if (font)
			CFRelease(font);
		return IC_FAIL_GEN;
	}
	CGContextSetRGBFillColor(context, 1, 1, 1, 1);
	for (int i = 0; i < 96; ++i) {
		UniChar character = i + 32;
		CGGlyph glyph;
		CTFontGetGlyphsForCharacters(font, &character, &glyph, 1);
		CGSize advance;
		CTFontGetAdvancesForGlyphs(font, kCTFontOrientationHorizontal, &glyph, &advance, 1);
		advance_[i]      = advance.width;
		CGPoint position = CGPointMake((i % 16) * cell + 4, (i / 16) * cell + 16);
		CTFontDrawGlyphs(font, &glyph, &position, 1, context);
	}
	CFRelease(font);
	CGContextRelease(context);
	// CoreGraphics stores the top scanline first; OpenGL's atlas starts at the bottom.
	for (int row = 0; row < height / 2; ++row)
		for (int color = 0; color < width * 4; ++color)
			std::swap(pixels[row * width * 4 + color],
			          pixels[(height - 1 - row) * width * 4 + color]);
	glGenTextures(1, &texture_);
	glBindTexture(GL_TEXTURE_2D, texture_);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
	             pixels.data());
	glBindTexture(GL_TEXTURE_2D, 0);
#elif defined(X11)
	Display *display = XOpenDisplay(nullptr);
	if (!display)
		return IC_FAIL_GEN;
	char pattern[128];
	snprintf(pattern, sizeof(pattern), "-misc-fixed-medium-r-normal--%d-*-*-*-*-*-iso8859-1", size);
	XFontStruct *font = XLoadQueryFont(display, pattern);
	if (!font)
		font = XLoadQueryFont(display, "10x20");
	if (!font)
		font = XLoadQueryFont(display, "fixed");
	if (!font) {
		XCloseDisplay(display);
		return IC_FAIL_GEN;
	}
	base_ = glGenLists(96);
	glXUseXFont(font->fid, 32, 96, base_);
	XFreeFont(display, font);
	XCloseDisplay(display);
#endif

	return IC_OK;
}
//                         const icGXDevice* const pDevice, bool bold)

/// Draw borrowed text at a logical screen position in the current font context.
ice::result font_gl::print(const ice::math::ivec2 &screen_pos, const ice::math::color &color,
                           const char *buff) {
#ifdef __APPLE__
	if (!buff || !texture_)
		return IC_FAIL_GEN;
	glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TEXTURE_BIT | GL_CURRENT_BIT |
	             GL_DEPTH_BUFFER_BIT);
	GLint matrix_mode;
	glGetIntegerv(GL_MATRIX_MODE, &matrix_mode);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadMatrixf(glm::value_ptr(gx_device_->get_ortho()));
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texture_);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glColor4f(color.red_float(), color.green_float(), color.blue_float(), color.alpha_float());
	float x = screen_pos.x, y = screen_pos.y;
	for (const unsigned char *c = (const unsigned char *)buff; *c; ++c) {
		if (*c == '\n') {
			x = screen_pos.x;
			y -= size_;
			continue;
		}
		if (*c < 32 || *c > 127)
			continue;
		int   i = *c - 32;
		float u = (i % 16) * 64.0f / 1024.0f, v = (i / 16) * 64.0f / 512.0f;
		float left = x - 4, bottom = y - 16;
		glBegin(GL_QUADS);
		glTexCoord2f(u, v);
		glVertex2f(left, bottom);
		glTexCoord2f(u + 64.0f / 1024, v);
		glVertex2f(left + 64, bottom);
		glTexCoord2f(u + 64.0f / 1024, v + 64.0f / 512);
		glVertex2f(left + 64, bottom + 64);
		glTexCoord2f(u, v + 64.0f / 512);
		glVertex2f(left, bottom + 64);
		glEnd();
		x += advance_[i];
	}
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(matrix_mode);
	glPopAttrib();
	return IC_OK;
#endif
	if (base_ > -1) {
		glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDisable(GL_LIGHTING);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_TEXTURE_2D);
		ice::math::mat4 temp;
		(temp) = ice::math::mat4(1);

		gx_device_->set_world_transform(temp);
		gx_device_->set_view_matrix(temp);
		gx_device_->set_projection(gx_device_->get_ortho());

		glColor3f(color.red_float(), color.green_float(), color.blue_float()); //,color.A());
		glRasterPos2i(screen_pos.x, screen_pos.y);

		glPushAttrib(GL_LIST_BIT);
		glListBase(base_ - 32);
		glCallLists(strlen(buff), GL_UNSIGNED_BYTE, buff);
		glPopAttrib();
		glPopAttrib();
		return IC_OK;
	}
	return IC_FAIL_GEN;
}

/// Draw printf-formatted text at a logical screen position in the current font context.
ice::result font_gl::printf(const ice::math::ivec2 &screen_pos, const ice::math::color &color,
                            const char *buff, ...) {
	if (base_ > -1 || texture_) {
		char    temp[512];
		va_list ap;

		va_start(ap, buff);
#ifdef WIN32
		vsprintf_s(temp, 512, buff, ap);
#else
		vsnprintf(temp, sizeof(temp), buff, ap);
#endif
		va_end(ap);
		return print(screen_pos, color, temp);
	}
	return IC_FAIL_GEN;
}
//                     const char* buff, ...)

#endif // ICGL
} // namespace ice::render::gl
