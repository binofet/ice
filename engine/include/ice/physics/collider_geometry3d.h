#pragma once
#include "ice/physics/types.h"
#include <cstddef>
#include <variant>
#include <vector>

namespace ice::physics {
/// Output storage/work budget for a readback, not a physics or authored-scene limit.
struct collider_geometry3d_limits {
	/// Maximum triangles emitted when a native shape requires triangle extraction.
	std::size_t triangles = 0;
	/// Byte allowance for extracted triangle positions; zero prevents mesh extraction.
	std::size_t bytes = 0;
};
/// Native box dimensions in the body's local coordinate space.
struct collider_box3d {
	/// Half extents in meters, with the native convex radius already included.
	math::vec3 half_extent{0};
	/// Native rounded-box radius; half_extent includes this radius, rather than adding it.
	float convex_radius = 0;
};
/// Native sphere dimensions, centered on the body origin.
struct collider_sphere3d {
	/// Sphere radius in meters.
	float radius = 0;
};
/// Native capsule aligned with the body's local Y axis.
struct collider_capsule3d {
	/// Radius of the cylinder and both hemispherical caps, in meters.
	float radius = 0;
	/// Cylinder half length along local Y, excluding the two hemispheres.
	float half_height = 0;
};
/// Complete native cooked triangle surface, rather than an authored mesh or simplified proxy.
struct collider_triangles3d {
	/// Native cooked triangles in body-origin-local meters; consecutive triples retain winding.
	std::vector<math::vec3> vertices;
};
/// Why this readback supplies no shape payload while retaining accepted body metadata.
enum class collider_unsupported3d { plane, heightfield, character_proxy, shape_type, budget };
/// Explicit unavailable shape alternative; it never stands for an empty valid collider.
struct collider_unavailable3d {
	/// Unsupported native shape category or exhausted triangle-output budget.
	collider_unsupported3d reason = collider_unsupported3d::shape_type;
};
/// Owned actual native geometry and accepted rigid pose. No native/cook/body owner is retained.
/// This is a current owner-thread readback, not permission, occupancy or an event observation.
struct collider_geometry3d {
	/// Accepted body-origin position in world meters, distinct from the center of mass.
	math::vec3 position{0};
	/// Accepted local-to-world body rotation.
	math::quat rotation{1, 0, 0, 0};
	/// Current engine motion policy of the body being inspected.
	motion_type motion = motion_type::static_body;
	/// Whether the native body is a sensor; this is not an overlap/event observation.
	bool sensor = false;
	/// Body-local native dimensions/triangles, or an explicit reason the payload is unavailable.
	std::variant<collider_unavailable3d, collider_box3d, collider_sphere3d, collider_capsule3d,
	             collider_triangles3d>
	    shape;
};
} // namespace ice::physics
