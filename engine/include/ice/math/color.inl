#pragma once

#include "ice/math/color.h"
namespace ice::math {

/*! Default Constructor
 **/
FORCE_INLINE color::color(void) {}

/*! Overloaded Constructor, initializes color
 **/
FORCE_INLINE color::color(ice::uchar red, ice::uchar green, ice::uchar blue, ice::uchar alpha) {
	r = red;
	g = green;
	b = blue;
	a = alpha;
}

/*! Default Destructor
 **/
FORCE_INLINE color::~color(void) {}

/*! Sets color
 **/
FORCE_INLINE void color::set(ice::uchar red, ice::uchar green, ice::uchar blue, ice::uchar alpha) {
	r = red;
	g = green;
	b = blue;
	a = alpha;
}

/*! Sets color
 **/
FORCE_INLINE void color::set_rgba(ice::uint color) {
	a = (ice::uchar)(color >> 24);
	r = (ice::uchar)(color >> 16);
	g = (ice::uchar)(color >> 8);
	b = (ice::uchar)(color);
}

/*! Coverts color to unsigned int
 **/
FORCE_INLINE ice::uint color::r8_g8_b8_a8(void) const {
	return ((r << 24) & 0xFF000000) | ((g << 16) & 0x00FF0000) | ((b << 8) & 0x0000FF00) |
	       (a & 0x000000FF);
}

/*! Coverts color to unsigned int
 **/
FORCE_INLINE ice::uint color::a8_r8_g8_b8(void) const {
	return ((a << 24) & 0xFF000000) | ((r << 16) & 0x00FF0000) | ((g << 8) & 0x0000FF00) |
	       (b & 0x000000FF);
}

/*! Overloaded assignment operator
 *
 *    @param[in]    color            Color to copy (unsigned)
 *    @returns    const icColor&    This
 *
 **/
FORCE_INLINE const color &color::operator=(ice::uint color) {
	r = (ice::uchar)(color >> 24);
	g = (ice::uchar)(color >> 16);
	b = (ice::uchar)(color >> 8);
	a = (ice::uchar)(color);
	return *this;
}

/*! Overloaded equality operator
 **/
FORCE_INLINE bool color::operator==(const color &rhs) const {
	return (r == rhs.r && g == rhs.g && b == rhs.b && a == rhs.a);
}

/*! Overloaded blue-equality operator
 **/
FORCE_INLINE bool color::operator!=(const color &rhs) const {
	return (r != rhs.r || g != rhs.g || b != rhs.b || a != rhs.a);
}

/*! Returns floating point red value
 *
 *    @returns const float    Red value as a float
 *
 **/
FORCE_INLINE float color::red_float(void) const { return r / 255.0f; }

/*! Returns floating point green value
 *
 *    @returns const float    Green value as a float
 *
 **/
FORCE_INLINE float color::green_float(void) const { return g / 255.0f; }

/*! Returns floating point blue value
 *
 *    @returns const float    Blue value as a float
 *
 **/
FORCE_INLINE float color::blue_float(void) const { return b / 255.0f; }

/*! Returns floating point alpha value
 *
 *    @returns const float    Alpha value as a float
 *
 **/
FORCE_INLINE float color::alpha_float(void) const { return a / 255.0f; }

} // namespace ice::math
