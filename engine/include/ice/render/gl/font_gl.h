#pragma once

#include "ice/globals.h"
namespace ice::render::gl {

#ifdef ICGL

#ifdef WIN32
} // namespace ice::render::gl
#include <GL/gl.h>
#include <windows.h>
namespace ice::render::gl {
#elif defined(__APPLE__)
} // namespace ice::render::gl
#include <OpenGL/gl.h>
namespace ice::render::gl {
#else
} // namespace ice::render::gl
#include <GL/gl.h>
#include <GL/glu.h>
namespace ice::render::gl {
#endif
} // namespace ice::render::gl

#include "ice/assets/content_loader.h"
#include "ice/render/font.h"
#include "ice/render/gl/graphics_device.h"
namespace ice::render::gl {

/// Owned compatibility-GL font atlas; drawing requires the same live current context used for
/// initialization.
class font_gl : public ice::render::font {
public:
	font_gl(void);
	virtual ~font_gl(void);

	virtual ice::result initialize(const char *file_name, const int size,
	                               ice::render::graphics_device *device);

	virtual ice::result print(const ice::math::ivec2 &screen_pos, const ice::math::color &color,
	                          const char *buff);

	virtual ice::result printf(const ice::math::ivec2 &screen_pos, const ice::math::color &color,
	                           const char *buff, ...);

private:
	GLint  base_;
	GLuint texture_;
	float  advance_[96];
	int    size_;
};

#endif //__IC_FONT_GL_H__
} // namespace ice::render::gl
