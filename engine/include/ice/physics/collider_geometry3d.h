#pragma once
#include "ice/physics/types.h"
#include <cstddef>
#include <variant>
#include <vector>

namespace ice::physics {
/// Output storage/work budget for a readback, not a physics or authored-scene limit.
struct collider_geometry3d_limits {
	std::size_t triangles = 0, bytes = 0;
};
struct collider_box3d {
	math::vec3 half_extent{0};
	/// Native rounded-box radius; half_extent includes this radius, rather than adding it.
	float convex_radius = 0;
};
struct collider_sphere3d {
	float radius = 0;
};
struct collider_capsule3d {
	/// Local Y cylinder half length excludes the two hemispheres.
	float radius = 0, half_height = 0;
};
struct collider_triangles3d {
	/// Native cooked triangles in body-origin-local meters; consecutive triples retain winding.
	std::vector<math::vec3> vertices;
};
enum class collider_unsupported3d { plane, heightfield, character_proxy, shape_type, budget };
struct collider_unavailable3d {
	collider_unsupported3d reason = collider_unsupported3d::shape_type;
};
/// Owned actual native geometry and accepted rigid pose. No native/cook/body owner is retained.
/// This is a current owner-thread readback, not permission, occupancy or an event observation.
struct collider_geometry3d {
	math::vec3  position{0};
	math::quat  rotation{1, 0, 0, 0};
	motion_type motion = motion_type::static_body;
	bool        sensor = false;
	std::variant<collider_unavailable3d, collider_box3d, collider_sphere3d, collider_capsule3d,
	             collider_triangles3d>
	    shape;
};
} // namespace ice::physics
