#pragma once

#include "ice/math/common_math.h"
namespace ice::math {

/// Packed four-byte RGBA color; floating-point accessors normalize each channel to [0,1].
class color {
public:
	color(void);
	color(ice::uchar red, ice::uchar green, ice::uchar blue, ice::uchar alpha = 255);
	~color(void);

	void set(ice::uchar red, ice::uchar green, ice::uchar blue, ice::uchar alpha = 255);

	void set_rgba(ice::uint color);

	ice::uint r8_g8_b8_a8(void) const;
	ice::uint a8_r8_g8_b8(void) const;

	float red_float(void) const;
	float green_float(void) const;
	float blue_float(void) const;
	float alpha_float(void) const;

	/// OPERATORS
	const color &operator=(ice::uint color);
	bool         operator==(const color &rhs) const;
	bool         operator!=(const color &rhs) const;

	/// Red channel as an unsigned byte in [0,255].
	ice::uchar r;
	/// Green channel as an unsigned byte in [0,255].
	ice::uchar g;
	/// Blue channel as an unsigned byte in [0,255].
	ice::uchar b;
	/// Alpha channel as an unsigned byte in [0,255].
	ice::uchar a;

	/// Predefined RGBA palette value; channel bytes are immutable.
	static color black;
	/// Predefined RGBA palette value; channel bytes are immutable.
	static color blue;
	/// Predefined RGBA palette value; channel bytes are immutable.
	static color green;
	/// Predefined RGBA palette value; channel bytes are immutable.
	static color red;
	/// Predefined RGBA palette value; channel bytes are immutable.
	static color magenta;
	/// Predefined RGBA palette value; channel bytes are immutable.
	static color yellow;
	/// Predefined RGBA palette value; channel bytes are immutable.
	static color brown;
	/// Predefined RGBA palette value; channel bytes are immutable.
	static color lgrey;
	/// Predefined RGBA palette value; channel bytes are immutable.
	static color dgrey;
	/// Predefined RGBA palette value; channel bytes are immutable.
	static color bblue;
	/// Predefined RGBA palette value; channel bytes are immutable.
	static color bgreen;
	/// Predefined RGBA palette value; channel bytes are immutable.
	static color bred;
	/// Predefined RGBA palette value; channel bytes are immutable.
	static color bmagenta;
	/// Predefined RGBA palette value; channel bytes are immutable.
	static color byellow;
	/// Predefined RGBA palette value; channel bytes are immutable.
	static color white;
};
} // namespace ice::math

#include "ice/math/color.inl"
