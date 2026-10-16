#pragma once

#include "ice/physics/types.h"

namespace ice::physics {
/// World-owned character lifetime; native pointers and Jolt identities never escape.
struct character_id {
	/// Non-reusing process-local world identity; zero is invalid.
	std::uint64_t owner = 0;
	/// One-based character slot; zero is invalid.
	std::uint32_t slot = 0;
	/// Non-wrapping slot lifetime, checked before every operation.
	std::uint64_t generation = 0;
	/// Compare identity without inspecting mutable simulation state.
	bool operator==(const character_id &other) const = default;
};
/// Configuration for a +Y-up capsule motor with its origin at the character's feet.
/// Creation requires world gravity parallel to -Y or zero; camera orientation is separate.
struct character3d_description {
	/// Initial feet position in world meters; each component lies in [-1000000,1000000].
	math::vec3 position{0};
	/// Initial motor velocity in m/s; each component lies in [-10000,10000].
	math::vec3 linear_velocity{0};
	/// Capsule radius in [0.01,10000] meters.
	float radius = .3f;
	/// Full capsule height, including hemispheres, in [2*radius,20000] meters.
	float height = 1.8f;
	/// Maximum walkable slope in radians, in [1,89] degrees.
	float max_slope_angle = glm::radians(50.f);
	/// Maximum automatic stair climb, in [0,height] meters; zero disables it.
	float step_height = .35f;
	/// Maximum downward floor adhesion, in [0,height] meters; zero disables it.
	float support_distance = .1f;
	/// Upward speed added to walkable support for one jump, in [0,10000] m/s.
	float jump_speed = 5.5f;
	/// Character mass used for support forces, in [0.001,1000000] kilograms.
	float mass = 80;
	/// Maximum force applied when pushing a rigid body, in [0,1000000000] newtons.
	float max_push_force = 100;
	/// Shape-query contact budget in [1,65536]; truncation is observable in the snapshot.
	std::uint32_t max_contacts = 1024;
};
/// Latest horizontal velocity plus a jump request, independent of input devices and cameras.
struct character3d_intent {
	/// Desired world X/Z velocity in m/s, each in [-10000,10000]; Y must be zero.
	math::vec3 horizontal_velocity{0};
	/// Latch one jump attempt until the next fixed tick; false does not cancel a queued request.
	bool jump = false;
};
/// Collision support reported by the most recent character update.
enum class character_ground_state {
	/// Supported by a walkable surface.
	on_ground,
	/// Touching a surface steeper than the configured walkable slope.
	on_steep_ground,
	/// Touching geometry that does not support the character.
	not_supported,
	/// No supporting contact, or contacts are awaiting refresh after creation/teleport.
	in_air
};
/// Owned readback; support is refreshed by fixed ticks rather than render/input updates.
struct character3d_snapshot {
	/// Engine character identity for this lifetime.
	character_id id;
	/// Read-only, slightly inset kinematic collision proxy, included in world body_count().
	body_id proxy;
	/// Feet position in world meters.
	math::vec3 position{0};
	/// Motor velocity in m/s; stair/floor snapping displacement is not additional momentum.
	math::vec3 linear_velocity{0};
	/// Support classification from the last completed character update.
	character_ground_state ground_state = character_ground_state::in_air;
	/// World support point; zero when support is awaiting refresh.
	math::vec3 ground_position{0};
	/// World support normal; zero when support is awaiting refresh.
	math::vec3 ground_normal{0};
	/// World velocity at the support point; zero when support is awaiting refresh.
	math::vec3 ground_velocity{0};
	/// Supporting rigid body, when still alive; zero for virtual-character or retired support.
	body_id ground_body;
	/// True when a jump attempt is queued for the next fixed tick.
	bool jump_pending = false;
	/// The most recent character shape query exceeded its contact budget.
	bool contacts_truncated = false;
};
} // namespace ice::physics
