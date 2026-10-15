#pragma once

#include "ice/globals.h"
#include "ice/render/graphics_device.h"

#include "ice/math/ivec2.h"
namespace ice::render {

/// Font family name and logical glyph size.
struct font_params {
	/// Owned/borrowed family name as specified by the enclosing font parameter type.
	std::string font_name;
	/// Number of valid entries/bytes as specified by the owning type.
	int size;
};

/*! Provides an interface for font objects which use operating specific
 *  and device specific constructs to create a renderable font.  i.e.
 *  this interface is not to be used for bitmap fonts.
 **/
class font {
public:
	/// Release the font through its concrete backend.
	virtual ~font() = default;

	/// Initialize the explicit subsystem state before loading or using its resources.
	virtual ice::result initialize(const char *file_name, const int size,
	                               class graphics_device *device) = 0;

	/// Draw the supplied text at a logical screen position using the current font context; text is
	/// borrowed for the call.
	virtual ice::result print(const ice::math::ivec2 &screen_pos, const ice::math::color &color,
	                          const char *buff) = 0;

	/// Format/draw legacy text; format arguments follow printf conventions.
	virtual ice::result printf(const ice::math::ivec2 &screen_pos, const ice::math::color &color,
	                           const char *buff, ...) = 0;

protected:
	class graphics_device *gx_device_;
};

} // namespace ice::render
