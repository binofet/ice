#pragma once

#include "ice/core/expected.h"
#include "ice/physics/character3d.h"
#include "ice/physics/types.h"
#include <memory>
#include <optional>
#include <span>

namespace ice::physics {
/// Native collision geometry supported by the first Jolt integration.
enum class shape3d_kind { sphere, box, capsule, plane };
/// Local collision geometry in meters; a capsule extends along local Y.
/// A plane is local Y=0 with -Y solid, transformed by the body pose; only static bodies accept it.
/// Plane broadphase bounds cover the finite engine coordinate domain, not mathematical infinity.
struct shape3d_description {
	/// Shape to prepare without mutating an active world.
	shape3d_kind kind = shape3d_kind::box;
	/// Box half sizes, strictly positive; ignored by other shapes.
	math::vec3 half_extent{0.5f};
	/// Sphere/capsule radius, strictly positive; ignored by boxes and planes.
	float radius = 0.5f;
	/// Capsule cylinder half length, excluding hemispheres; may be zero; ignored by other shapes.
	float half_height = 0.5f;
};
/// Fixed-step world settings. The caller decides when to step, pause and catch up.
struct world3d_description {
	/// Acceleration in meters per second squared.
	math::vec3 gravity{0, -9.81f, 0};
	/// Configured fixed tick duration, in [1/1000,1/15] seconds.
	float time_step = 1.0f / 120.0f;
	/// Collision substeps per tick, in [1,16].
	std::uint32_t substeps = 2;
	/// Simultaneously live body budget, in [1,65536].
	std::uint32_t max_bodies = 4096;
	/// Retained contact transitions per tick, in [1,65536].
	std::uint32_t max_contact_events = 8192;
	/// Character slot budget in [0,65536]; zero disables characters. Their proxies also consume
	/// max_bodies, so the effective character capacity is the smaller of these budgets.
	std::uint32_t max_characters = 64;
};
/// Owned body construction data; native shape/body pointers never escape the world.
struct body3d_description {
	/// Static, explicitly moved kinematic, or simulated dynamic motion.
	motion_type motion = motion_type::dynamic;
	/// Local collision shape anchored at the body's origin; a plane passes through that origin.
	shape3d_description shape;
	/// Initial world translation in meters.
	math::vec3 position{0};
	/// Initial unit quaternion, normalized after finite/nonzero validation.
	math::quat rotation{1, 0, 0, 0};
	/// Initial linear velocity in meters per second.
	math::vec3 linear_velocity{0};
	/// Initial angular velocity in radians per second.
	math::vec3 angular_velocity{0};
	/// Dynamic mass in kilograms; finite and strictly positive.
	float mass = 1;
	/// Coulomb friction in [0,10].
	float friction = 0.5f;
	/// Restitution in [0,1].
	float restitution = 0.1f;
	/// Sensors report overlap transitions without applying collision response.
	bool sensor = false;
	/// Enable continuous linear-cast collision for fast dynamic bodies.
	bool continuous = false;
};
/// Owned readback at a completed tick or after an explicit host edit.
struct body3d_snapshot {
	/// Engine identity of this body lifetime.
	body_id id;
	/// Registered motion policy.
	motion_type motion = motion_type::static_body;
	/// Current world translation in meters.
	math::vec3 position{0};
	/// Current unit orientation.
	math::quat rotation{1, 0, 0, 0};
	/// Current linear velocity in meters per second.
	math::vec3 linear_velocity{0};
	/// Current angular velocity in radians per second.
	math::vec3 angular_velocity{0};
	/// True when a dynamic body is inactive/sleeping.
	bool sleeping = false;
};
/// Closest nonsensor ray intersection; fraction lies in [0,1] along the displacement.
struct ray_hit3d {
	/// Engine identity of the intersected body.
	body_id body;
	/// Intersection position in world meters.
	math::vec3 point{0};
	/// Unit world surface normal.
	math::vec3 normal{0};
	/// Multiplier applied to the supplied ray displacement.
	float fraction = 0;
};
/// Explicit single-host-thread Jolt world, independent of windows and rendering.
/// Fixed-step integration is approximate and not a cross-platform lockstep guarantee.
/// A native step-budget error faults further stepping; inspection/destruction remain available.
/// Body operations validate handles before accessing Jolt. Allocation failure is not
/// crash-contained.
class world3d {
public:
	/// Validate settings and allocate the bounded native simulation/event storage.
	static ice::expected<world3d, diagnostic> create(const world3d_description &description = {});
	/// Retire all owned native bodies, collision shapes and world resources.
	~world3d();
	/// Transfer the world identity and all body lifetimes; the source becomes unavailable.
	world3d(world3d &&other) noexcept;
	/// Retire this world, then take the source world and its identities.
	world3d &operator=(world3d &&other) noexcept;
	/// A native world cannot be copied.
	world3d(const world3d &) = delete;
	/// Native bodies cannot be implicitly duplicated.
	world3d &operator=(const world3d &) = delete;
	/// Prepare a collision shape and body, then insert it; rejection preserves existing bodies.
	ice::expected<body_id, diagnostic> create_body(const body3d_description &description);
	/// Remove and retire a body; stale/foreign IDs cannot address its replacement.
	ice::expected<void, diagnostic> erase(body_id body);
	/// Copy current body state without advancing simulation.
	ice::expected<body3d_snapshot, diagnostic> inspect(body_id body) const;
	/// Place a body explicitly, optionally clearing velocity; wake dynamic bodies.
	ice::expected<void, diagnostic> set_pose(body_id body, const math::vec3 &position,
	                                         const math::quat &rotation,
	                                         bool              reset_velocity = true);
	/// Apply a center-of-mass impulse in kilogram-meters/second; only dynamic bodies accept it.
	ice::expected<void, diagnostic> apply_impulse(body_id body, const math::vec3 &impulse);
	/// Prepare a +Y-up capsule motor and protected collision proxy; failure preserves live actors.
	ice::expected<character_id, diagnostic>
	create_character(const character3d_description &description = {});
	/// Retire the character and its proxy; stale/foreign character IDs are rejected.
	ice::expected<void, diagnostic> erase_character(character_id character);
	/// Copy motor/support state without advancing simulation; available after world faults.
	ice::expected<character3d_snapshot, diagnostic> inspect_character(character_id character) const;
	/// Replace horizontal motion and latch jump with logical OR; no fixed tick is performed.
	/// A queued attempt is consumed once at the next tick, even when airborne or jumping disabled.
	ice::expected<void, diagnostic> set_character_intent(character_id              character,
	                                                     const character3d_intent &intent);
	/// Clear requested horizontal motion and any queued jump; retain physical vertical velocity.
	/// Available after world faults so releasing input never requires simulation recovery.
	ice::expected<void, diagnostic> cancel_character_intent(character_id character);
	/// Set feet position/velocity and clear input. Contact support is invalidated until the next
	/// tick. This is an explicit teleport, not a collision sweep or a kinematic platform API.
	ice::expected<void, diagnostic>
	teleport_character(character_id character, const math::vec3 &position,
	                   const math::vec3 &linear_velocity = math::vec3{0});
	/// Advance exactly one configured tick. Native errors may have partially advanced state and
	/// fault this world; no physical rollback is implied by a failure diagnostic.
	ice::expected<step_report, diagnostic> step();
	/// Borrow transitions since the preceding step, valid until the next step or world retirement.
	/// Events may name bodies explicitly retired since contact; inspect rejects those stale IDs.
	std::span<const contact_event> contacts() const;
	/// Query the closest nonsensor hit without stepping; a zero/invalid ray is rejected.
	/// Every ignored handle must be live in this world; at most max_bodies may be supplied.
	ice::expected<std::optional<ray_hit3d>, diagnostic>
	ray_cast(const math::vec3 &origin, const math::vec3 &displacement,
	         std::span<const body_id> ignored = {}) const;
	/// Process-local non-reusing world identity; zero for a moved-from world.
	std::uint64_t identity() const;
	/// Number of successfully completed fixed ticks.
	std::uint64_t tick() const;
	/// Configured seconds per fixed tick; zero for a moved-from world.
	float time_step() const;
	/// Number of currently live bodies.
	std::uint32_t body_count() const;
	/// Number of live characters, excluding retired native objects awaiting contact cleanup.
	std::uint32_t character_count() const;

private:
	struct implementation;
	explicit world3d(std::unique_ptr<implementation> state);
	std::unique_ptr<implementation> state_;
};
} // namespace ice::physics
