#pragma once

#include "ice/math/mat4.h"
#include "ice/math/vec2.h"
namespace ice::math {

/*! Provides an axis aligned bounding box object
 **/
class aabb {
public:
	/// Construct inactive state; initialize explicitly before active use.
	FORCE_INLINE aabb(void) {
		max = vec2(-ICREAL_MAX);
		min = vec2(ICREAL_MAX);
	};

	/// Reset to an empty axis-aligned box; subsequent add operations grow its extent.
	FORCE_INLINE void set_empty(void) {
		max = vec2(-ICREAL_MAX);
		min = vec2(ICREAL_MAX);
	};

	/*! re-evaluate the aabb with new point
	 *
	 *      If the given point is outside the aabb, the aabb is expanded
	 *
	 *  @param      point         New point to consider
	 **/
	FORCE_INLINE void add(const vec2 &point) {
		if (point.x > max.x)
			max.x = point.x;
		if (point.x < min.x)
			min.x = point.x;
		if (point.y > max.y)
			max.y = point.y;
		if (point.y < min.y)
			min.y = point.y;
	};

	/*! re-evaluate the aabb with exisiting aabb
	 *
	 *      If the given aabb is outside the existing aabb,
	 *      the aabb is expanded.
	 *
	 *  @param      aabb         New point to consider
	 **/
	FORCE_INLINE void add(const aabb &aabb) {
		if (aabb.max.x > max.x)
			max.x = aabb.max.x;
		if (aabb.min.x < min.x)
			min.x = aabb.min.x;
		if (aabb.max.y > max.y)
			max.y = aabb.max.y;
		if (aabb.min.y < min.y)
			min.y = aabb.min.y;
	};

	/*  AABB - POINT intersection test
	 *
	 *  @param      pos       Point to test intersection with
	 *  @returns    bool        true if in intersection
	 **/
	/// Test inclusive overlap with a point or axis-aligned box in the same 2D coordinate space.
	FORCE_INLINE bool intersect(const vec2 &pos) {
		/// edge separation test
		if (max.x < pos.x || min.x > pos.x)
			return false;
		if (max.y < pos.y || min.y > pos.y)
			return false;

		return true;
	};

	/*  AABB - AABB intersection test
	 *
	 *  @param      aabb        AABB to test intersection with
	 *  @returns    bool        true if in intersection
	 **/
	/// Test inclusive overlap with a point or axis-aligned box in the same 2D coordinate space.
	FORCE_INLINE bool intersect(const aabb &aabb) {
		/// edge separation test
		if (max.x < aabb.min.x || min.x > aabb.max.x)
			return false;
		if (max.y < aabb.min.y || min.y > aabb.max.y)
			return false;

		return true;
	};

	/*  AABB - OOBB intersection test
	 *
	 *  @param      oobb        OOBB to test intersection with
	 *  @returns    bool        true if in intersection
	 **/
	/// Legacy oriented 2D box parameters; oriented intersection support is not implemented.
	/// Legacy oriented 2D box parameters; oriented intersection tests are currently unsupported.
	FORCE_INLINE bool intersect(const class oobb &oobb) { return false; };

	/// Minimum corner of this axis-aligned 2D box.
	vec2 min;
	/// Maximum corner of this axis-aligned 2D box.
	vec2 max;
};

/// Legacy oriented 2D box parameters; intersection support is currently unavailable.
class oobb {
public:
	/// icOOBB(void)
	//{
	//
	// };

	/// FORCE_INLINE void Add(const icVector2& v2Point)
	//{
	// };

	/// FORCE_INLINE void Add(const icAABox& box);
	//{
	// };

	/// Oriented-box rotation in radians.
	float angle;
	/// Half extent along the oriented box local axes.
	vec2 half_width;
};

} // namespace ice::math
