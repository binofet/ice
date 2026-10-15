#pragma once

#include "ice/math/vec2.h"
namespace ice::math {

/*! 2D collision information
 **/
struct collision_info2 {
	/// Contact point in the queried shapes coordinate system.
	vec2 hit_point;
	/// Surface/collision normal in the coordinate space documented by the record.
	vec2 normal;
	/// Pixel depth or collision penetration, in units defined by the owning record.
	ice::math::real depth;
};

/*! 2D Circle Shape
 **/
struct circle2 {
	/// Position in the coordinate space documented by the owning record.
	vec2 pos;
	/// Circle radius in shape coordinates.
	ice::math::real radius;
};

/*! 2D Axis Aligned Bounding Box
 **/
struct aabb2 {
	/// Return the axis-aligned box radial extent at the supplied angle in radians.
	ice::math::real get_radius(ice::math::real theta) const {
		ice::math::real alpha = atan2(extents.y, extents.x);
		if (theta < alpha || theta >= (2.0f * IC_PI - alpha))
			return extents.x / cos(theta);
		if (theta < (IC_PI - alpha))
			return extents.y / cos(theta - IC_PI_HALF);
		if (theta < (IC_PI + alpha))
			return extents.x / cos(theta - IC_PI);

		return extents.x / cos(theta - (3.0f * IC_PI_HALF));
	};

	/// Position in the coordinate space documented by the owning record.
	vec2 pos;
	/// Axis-aligned half extents in shape-local coordinates.
	vec2 extents;
};

/*! 2D Object Oriented Bounding Box
 **/
struct oobb2 {
	/// Position in the coordinate space documented by the owning record.
	vec2 pos;
	/// Axis-aligned half extents in shape-local coordinates.
	vec2 extents;
	/// Shape rotation in radians.
	ice::math::real rot;
};

/*! Determines if point is inside AABB
 *
 *  @param          p           Point to test versus box
 *  @param          box         Box to test versus point
 *  @returns        bool        true if point is inside box
 **/
FORCE_INLINE bool point_in_aabb2(const vec2 &p, const aabb2 &box) {
	if (p.x > box.pos.x + box.extents.x)
		return false;
	if (p.x < box.pos.x - box.extents.x)
		return false;
	if (p.y > box.pos.y + box.extents.y)
		return false;
	if (p.y < box.pos.y - box.extents.y)
		return false;
	return true;
};

/*! Determines if point is inside circle
 *
 *  @param          p           Poing to test versus circle
 *  @param          circle      Circle to test versus point
 *  @returns        bool        true if point is inside circle
 **/
FORCE_INLINE bool point_in_circle2(const vec2 &p, const circle2 &circle) {
	return (glm::distance(p, circle.pos) <= circle.radius);
};

/*! Intersect test for circle + axis aligned box
 *
 *  @param      circle      Position and radius
 *  @param      box         Position and extents
 *  @param[out] info       Pointer to CollisionInfo struct
 *  @returns    bool        True if the objects are in collision
 **/
FORCE_INLINE bool circle_intersects_aabb2(const circle2 &circle, const aabb2 &box,
                                          collision_info2 *info) {
	vec2            vc2b        = box.pos - circle.pos;
	ice::math::real dist_sqr    = glm::dot(vc2b, vc2b);  //! dist2 between objects
	ice::math::real ang_between = atan2(vc2b.y, vc2b.x); //! ang between objects

	/// clamp to positive angles
	while (ang_between < 0.0f)
		ang_between += 2.0f * IC_PI;

	/// check if box and circle collide
	ice::math::real boxradius   = box.get_radius(ang_between);
	ice::math::real opposit_ang = ang_between - IC_PI;

	/// clamp to positive angles
	while (opposit_ang < 0.0f)
		opposit_ang += 2.0f * IC_PI;

	/// icReal angle = ang_between * 180.0f / IC_PI;
	/// icReal box_rad_sqr = boxradius*boxradius;
	ice::math::real remaining = sqrt(dist_sqr) - circle.radius - boxradius;

	/// check corners first
	vec2 corners[4];

	corners[0] = vec2(box.pos.x - box.extents.x, box.pos.y - box.extents.y);
	corners[1] = vec2(box.pos.x - box.extents.x, box.pos.y + box.extents.y);
	corners[2] = vec2(box.pos.x + box.extents.x, box.pos.y + box.extents.y);
	corners[3] = vec2(box.pos.x + box.extents.x, box.pos.y - box.extents.y);

	if (circle.pos.x < corners[0].x && circle.pos.y < corners[0].y &&
	    point_in_circle2(corners[0], circle)) {
		// todo get the hit point
		info->depth  = abs(circle.radius - glm::distance(corners[0], circle.pos));
		info->normal = (circle.pos - corners[0]);
		info->normal = glm::normalize(info->normal);
		return true;
	} else if (circle.pos.x < corners[1].x && circle.pos.y > corners[1].y &&
	           point_in_circle2(corners[1], circle)) {
		// todo get the hit point
		info->depth  = abs(circle.radius - glm::distance(corners[1], circle.pos));
		info->normal = glm::normalize(circle.pos - corners[1]);
		return true;
	} else if (circle.pos.x > corners[2].x && circle.pos.y > corners[2].y &&
	           point_in_circle2(corners[2], circle)) {
		// todo get the hit point
		info->depth  = abs(circle.radius - glm::distance(corners[2], circle.pos));
		info->normal = glm::normalize(circle.pos - corners[2]);
		return true;
	} else if (circle.pos.x > corners[3].x && circle.pos.y < corners[3].y &&
	           point_in_circle2(corners[3], circle)) {
		// todo get the hit point
		info->depth  = abs(circle.radius - glm::distance(corners[3], circle.pos));
		info->normal = glm::normalize(circle.pos - corners[3]);
		return true;
	}

	/// check edge planes
	if (remaining <= 0.0f) {
		// todo get the hit point
		info->depth  = abs(remaining);
		info->normal = circle.pos - box.pos;

		// if (abs(pInfo->normal.x) > abs(pInfo->normal.y))
		//     pInfo->normal.y = 0.0f;
		// else
		//     pInfo->normal.x = 0.0f;

		info->normal = glm::normalize(info->normal);

		return true;
	}

	return false;
}
//                              const ic2DAABB box,
//                              ic2DCollisionInfo* pInfo)

} // namespace ice::math
