#pragma once
namespace ice::render {} // namespace ice::render
#include "ice/math/color.h"
#include "ice/math/vec3.h"
namespace ice::render {

/*! Provides an interface to a light object
 **/
class light {
public:
	/// Position in the coordinate space documented by the owning record.
	ice::math::vec3 pos;
	/// World-space light direction.
	ice::math::vec3 dir;

	/// Ambient light RGBA intensity.
	ice::math::color ambient;
	/// Diffuse light RGBA intensity.
	ice::math::color diffuse;
	/// Specular light RGBA intensity.
	ice::math::color specular;

	/// Spotlight concentration exponent, using legacy OpenGL semantics.
	ice::math::real spot_exponent;
	/// Spotlight cone cutoff angle in degrees.
	ice::math::real spot_cutoff;
	/// Constant coefficient of distance attenuation.
	ice::math::real const_atten;
	/// Linear coefficient of distance attenuation.
	ice::math::real linear_atten;
	/// Quadratic coefficient of distance attenuation.
	ice::math::real quad_atten;
};

} // namespace ice::render
