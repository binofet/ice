#pragma once
#include "ice/core/expected.h"
#include "ice/math/types.h"
#include "ice/render/types.h"

namespace ice::assets {
/// Procedural indexed primitives; triangle is a double-sided display primitive.
enum class primitive_kind { box, sphere, capsule, pyramid, triangle, plane };
/// Bounded CPU geometry recipe, independent of a GPU or physics world.
struct primitive_description {
	/// Geometry to construct.
	primitive_kind kind = primitive_kind::box;
	/// Box/pyramid half sizes; plane uses X/Z. Each used component is strictly positive.
	math::vec3 half_extent{0.5f};
	/// Sphere/capsule radius in meters.
	float radius = 0.5f;
	/// Capsule straight cylinder half length, excluding hemispheres.
	float half_height = 0.5f;
	/// Revolved primitives use [3,128] segments around local Y.
	std::uint32_t segments = 32;
	/// Sphere uses [2,128] latitude sections; capsule uses this many per hemisphere.
	std::uint32_t rings = 16;
};
/// Create owned position/normal vertices and outward, counterclockwise triangle indices.
/// Reject nonfinite dimensions and invalid subdivision budgets before allocating geometry.
ice::expected<render::mesh_data, render::diagnostic>
make_primitive(const primitive_description &description);
} // namespace ice::assets
