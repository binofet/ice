#pragma once

#include "ice/core/expected.h"
#include "ice/physics/types.h"
#include <memory>
#include <optional>
#include <span>

namespace ice::physics {
/// Collision geometry supported by the Box2D integration.
enum class shape2d_kind { circle, box, capsule };
/// Local collision geometry in meters; capsules extend along local Y.
struct shape2d_description {
	/// Shape to create at the body's origin.
	shape2d_kind kind = shape2d_kind::box;
	/// Box half sizes in [0.005,1000]; ignored by circles and capsules.
	math::vec2 half_extent{0.5f};
	/// Circle/capsule radius in [0.005,1000]; ignored by boxes.
	float radius = 0.5f;
	/// Capsule segment half length in [0,1000], excluding its circular ends.
	float half_height = 0.5f;
};
/// Fixed-step settings, independent of render frames or an automatic clock.
struct world2d_description {
	/// Acceleration in meters/second squared; finite components bounded to 10000.
	math::vec2 gravity{0, -9.81f};
	/// Configured duration in [1/1000,1/15] seconds.
	float time_step = 1.0f / 120.0f;
	/// Solver substeps per tick, in [1,16].
	std::uint32_t substeps = 4;
	/// Simultaneously live bodies, in [1,65536].
	std::uint32_t max_bodies = 4096;
	/// Retained transitions per tick, in [1,65536].
	std::uint32_t max_contact_events = 8192;
};
/// One centered shape and owned body construction data, using meters/kilograms/seconds.
struct body2d_description {
	/// Static, explicitly moved kinematic, or simulated dynamic motion.
	motion_type motion = motion_type::dynamic;
	/// Centered local collision geometry.
	shape2d_description shape;
	/// Initial translation; finite components bounded to 1000000 meters.
	math::vec2 position{0};
	/// Finite initial radians; normalized before native conversion.
	float rotation = 0;
	/// Initial meters/second; finite components bounded to 10000.
	math::vec2 linear_velocity{0};
	/// Initial radians/second; finite and bounded to 10000.
	float angular_velocity = 0;
	/// Dynamic mass in [0.0001,1000000] kilograms.
	float mass = 1;
	/// Coulomb friction in [0,10].
	float friction = 0.5f;
	/// Restitution in [0,1].
	float restitution = 0.1f;
	/// Overlap transitions without collision response.
	bool sensor = false;
	/// Enable Box2D bullet collision handling for fast dynamic bodies.
	bool continuous = false;
};
/// Owned body readback, with no reference to native body/shape storage.
struct body2d_snapshot {
	/// Engine identity of this body lifetime.
	body_id id;
	/// Registered motion policy.
	motion_type motion = motion_type::static_body;
	/// Current world translation in meters.
	math::vec2 position{0};
	/// Current radians in [-pi,pi].
	float rotation = 0;
	/// Current meters/second.
	math::vec2 linear_velocity{0};
	/// Current radians/second.
	float angular_velocity = 0;
	/// Whether a dynamic body is inactive/sleeping.
	bool sleeping = false;
};
/// Closest nonsensor intersection along a finite displacement.
struct ray_hit2d {
	/// Engine identity of the intersected body.
	body_id body;
	/// Intersection point in world meters.
	math::vec2 point{0};
	/// World surface normal, or zero for an initial overlap.
	math::vec2 normal{0};
	/// Multiplier in [0,1] along the supplied displacement.
	float fraction = 0;
};
/// Explicit single-host-thread Box2D world, independent of windows and rendering.
/// Fixed-step integration is approximate, not a cross-platform lockstep guarantee.
/// Native allocation/assertion failures are not crash-contained. Event storage and engine
/// lifetime maps are allocated at creation; the native simulation may allocate while stepping.
class world2d {
public:
	/// Validate settings and allocate native simulation, body slots and bounded event storage.
	static ice::expected<world2d, diagnostic> create(const world2d_description &description = {});
	/// Retire this native world and every remaining body/shape.
	~world2d();
	/// Transfer all lifetimes; the source becomes unavailable with identity zero.
	world2d(world2d &&other) noexcept;
	/// Retire this world and transfer the source's lifetimes.
	world2d &operator=(world2d &&other) noexcept;
	/// A world cannot be copied.
	world2d(const world2d &) = delete;
	/// Body ownership cannot be implicitly duplicated.
	world2d &operator=(const world2d &) = delete;
	/// Validate and create one body/shape. Rejection preserves existing bodies and events.
	ice::expected<body_id, diagnostic> create_body(const body2d_description &description);
	/// Retire a body and queue its contact ends for the next step, preserving old engine IDs.
	ice::expected<void, diagnostic> erase(body_id body);
	/// Copy current body state without stepping; stale/foreign handles are rejected.
	ice::expected<body2d_snapshot, diagnostic> inspect(body_id body) const;
	/// Teleport a body, optionally clear velocity, and wake dynamic bodies.
	ice::expected<void, diagnostic> set_pose(body_id body, const math::vec2 &position,
	                                         float rotation, bool reset_velocity = true);
	/// Apply a center-of-mass impulse to a dynamic body. Reject a resulting component
	/// speed above 10000 meters/second before changing native state.
	ice::expected<void, diagnostic> apply_impulse(body_id body, const math::vec2 &impulse);
	/// Advance one configured tick. Nonfinite native output faults future stepping;
	/// a failed native step is not rolled back. Tick overflow is rejected before stepping.
	ice::expected<step_report, diagnostic> step();
	/// Borrow transitions until the next step or world retirement. Explicit body retirement
	/// does not invalidate this span; events can name stale IDs that inspect() rejects.
	std::span<const contact_event> contacts() const;
	/// Query the closest nonsensor hit. Reject zero rays, nonfinite inputs, and endpoints
	/// outside the finite +/-1000000-meter query budget. No step is performed.
	ice::expected<std::optional<ray_hit2d>, diagnostic>
	ray_cast(const math::vec2 &origin, const math::vec2 &displacement) const;
	/// Unique process-local world lifetime; zero for a moved-from world.
	std::uint64_t identity() const;
	/// Number of successfully completed fixed ticks.
	std::uint64_t tick() const;
	/// Configured seconds per tick; zero for a moved-from world.
	float time_step() const;
	/// Number of currently live bodies.
	std::uint32_t body_count() const;

private:
	struct implementation;
	explicit world2d(std::unique_ptr<implementation> state);
	std::unique_ptr<implementation> state_;
};
} // namespace ice::physics
