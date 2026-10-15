#include "ice/physics/world2d.h"
#include <cmath>
#include <iostream>
#include <limits>

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::cerr << "Check failed at " << __LINE__ << ": " #condition << '\n';                \
			return 1;                                                                              \
		}                                                                                          \
	} while (false)

using namespace ice::physics;
namespace {
bool close(float first, float second, float tolerance = 0.001f) {
	return std::abs(first - second) < tolerance;
}
bool involves(const contact_event &event, body_id first, body_id second) {
	return (event.first == first && event.second == second) ||
	       (event.first == second && event.second == first);
}
} // namespace

int main() {
	const auto          nan = std::numeric_limits<float>::quiet_NaN();
	world2d_description invalid;
	invalid.gravity.x = nan;
	CHECK(!world2d::create(invalid));
	invalid           = {};
	invalid.time_step = 0;
	CHECK(!world2d::create(invalid));
	invalid          = {};
	invalid.substeps = 17;
	CHECK(!world2d::create(invalid));
	invalid            = {};
	invalid.max_bodies = 65537;
	CHECK(!world2d::create(invalid));
	invalid                    = {};
	invalid.max_contact_events = 0;
	CHECK(!world2d::create(invalid));

	// A falling circle settles on a static box; all geometry is in meters.
	auto world = world2d::create();
	CHECK(world && world->identity() && world->tick() == 0 && world->body_count() == 0);
	body2d_description floor;
	floor.motion            = motion_type::static_body;
	floor.position          = {0, -0.5f};
	floor.shape.half_extent = {10, 0.5f};
	auto ground             = world->create_body(floor);
	CHECK(ground && !world->apply_impulse(*ground, {0, 1}));
	body2d_description ball;
	ball.shape.kind = shape2d_kind::circle;
	ball.position   = {0, 5};
	ball.mass       = 2;
	auto falling    = world->create_body(ball);
	CHECK(falling && world->apply_impulse(*falling, {4, 0}));
	CHECK(close(world->inspect(*falling)->linear_velocity.x, 2));
	CHECK(world->set_pose(*falling, ball.position, 0));
	bool touched = false;
	for (int index = 0; index < 480; ++index) {
		auto report = world->step();
		CHECK(report && report->tick == std::uint64_t(index + 1) && !report->contacts_truncated);
		for (const auto &event : world->contacts())
			touched |= event.phase == contact_phase::begin && involves(event, *ground, *falling);
	}
	auto settled = world->inspect(*falling);
	CHECK(touched && settled && close(settled->position.y, 0.5f, 0.06f));
	CHECK(std::abs(settled->linear_velocity.y) < 0.1f);
	auto hit = world->ray_cast({0, 3}, {0, -6});
	CHECK(hit && *hit && (**hit).body == *falling && close((**hit).point.y, 1, 0.06f));
	CHECK((**hit).normal.y > 0.9f && (**hit).fraction >= 0 && (**hit).fraction <= 1);
	CHECK(!world->ray_cast({0, 0}, {0, 0}) && !world->ray_cast({nan, 0}, {1, 0}));
	CHECK(!world->set_pose(*falling, {nan, 0}, 0));
	CHECK(!world->apply_impulse(*falling, {nan, 0}));
	CHECK(!world->apply_impulse(*falling, {1000000, 0}));
	CHECK(close(world->inspect(*falling)->position.y, settled->position.y));
	auto malformed       = ball;
	malformed.shape.kind = static_cast<shape2d_kind>(99);
	CHECK(!world->create_body(malformed) && world->body_count() == 2);
	malformed              = ball;
	malformed.shape.radius = 0;
	CHECK(!world->create_body(malformed));
	malformed      = ball;
	malformed.mass = 0;
	CHECK(!world->create_body(malformed));

	world2d_description zero_gravity;
	zero_gravity.gravity    = {0, 0};
	zero_gravity.max_bodies = 8;
	auto sensor_world       = world2d::create(zero_gravity);
	CHECK(sensor_world && sensor_world->identity() != world->identity());
	CHECK(!sensor_world->inspect(*falling));
	body2d_description sensor;
	sensor.motion            = motion_type::static_body;
	sensor.sensor            = true;
	sensor.shape.half_extent = {2, 2};
	auto trigger             = sensor_world->create_body(sensor);
	ball.position            = {0, 0};
	auto visitor             = sensor_world->create_body(ball);
	CHECK(trigger && visitor && sensor_world->step());
	bool entered = false;
	for (const auto &event : sensor_world->contacts())
		entered |= event.sensor && event.phase == contact_phase::begin &&
		           involves(event, *trigger, *visitor);
	CHECK(entered && close(sensor_world->inspect(*visitor)->position.y, 0));
	hit = sensor_world->ray_cast({-4, 0}, {8, 0});
	CHECK(hit && *hit && (**hit).body == *visitor);
	const auto borrowed    = sensor_world->contacts();
	const auto first_event = borrowed.front();
	CHECK(sensor_world->erase(*visitor) && !sensor_world->inspect(*visitor));
	CHECK(borrowed.front().first == first_event.first &&
	      borrowed.front().second == first_event.second);

	// Exercise native 16-bit shape-generation wrap between ticks. The engine's
	// 64-bit slot generation and retained old end mapping must never alias the new body.
	ball.position = {10, 0};
	for (int index = 0; index < 65535; ++index) {
		auto transient = sensor_world->create_body(ball);
		CHECK(transient && sensor_world->erase(*transient));
	}
	auto replacement = sensor_world->create_body(ball);
	CHECK(replacement && replacement->slot == visitor->slot && *replacement != *visitor);
	CHECK(!sensor_world->set_pose(*visitor, {0, 0}, 0) && !sensor_world->erase(*visitor));
	CHECK(sensor_world->step());
	bool exited = false;
	for (const auto &event : sensor_world->contacts()) {
		exited |= event.sensor && event.phase == contact_phase::end &&
		          involves(event, *trigger, *visitor);
		CHECK(!(event.phase == contact_phase::end && involves(event, *trigger, *replacement)));
	}
	CHECK(exited);

	body2d_description kinematic;
	kinematic.motion          = motion_type::kinematic;
	kinematic.position        = {20, 0};
	kinematic.linear_velocity = {1, 0};
	auto mover                = sensor_world->create_body(kinematic);
	CHECK(mover && !sensor_world->apply_impulse(*mover, {1, 0}));
	CHECK(sensor_world->step());
	CHECK(close(sensor_world->inspect(*mover)->position.x, 20 + sensor_world->time_step()));
	CHECK(sensor_world->set_pose(*mover, {21, 0}, 1, false));
	CHECK(close(sensor_world->inspect(*mover)->linear_velocity.x, 1));
	CHECK(sensor_world->set_pose(*mover, {21, 0}, 1));
	CHECK(close(sensor_world->inspect(*mover)->linear_velocity.x, 0));

	body2d_description capsule;
	capsule.motion            = motion_type::static_body;
	capsule.shape.kind        = shape2d_kind::capsule;
	capsule.shape.half_height = 1;
	capsule.position          = {30, 0};
	auto pill                 = sensor_world->create_body(capsule);
	CHECK(pill);
	hit = sensor_world->ray_cast({30, 3}, {0, -6});
	CHECK(hit && *hit && (**hit).body == *pill && close((**hit).point.y, 1.5f));
	capsule.position          = {40, 0};
	capsule.shape.half_height = 0;
	CHECK(sensor_world->create_body(capsule));

	// Overflow is visible and bounded; it never invalidates body ownership.
	zero_gravity.max_contact_events = 1;
	auto limited                    = world2d::create(zero_gravity);
	CHECK(limited && limited->create_body(sensor));
	ball.position = {0, 0};
	for (int index = 0; index < 3; ++index) {
		ball.position.x = float(index) * 0.1f;
		CHECK(limited->create_body(ball));
	}
	auto overflow = limited->step();
	CHECK(overflow && overflow->contacts_truncated && limited->contacts().size() == 1);
	CHECK(limited->body_count() == 4);
	zero_gravity.max_bodies = 1;
	auto one_body           = world2d::create(zero_gravity);
	CHECK(one_body && one_body->create_body(ball) && !one_body->create_body(ball));

	const auto identity = sensor_world->identity();
	world2d    moved(std::move(*sensor_world));
	CHECK(moved.identity() == identity && moved.inspect(*replacement));
	// The documented moved-from world is unavailable; queries cannot dereference its pimpl.
	// NOLINTBEGIN(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
	CHECK(sensor_world->identity() == 0 && !sensor_world->step() &&
	      !sensor_world->inspect(*replacement));
	// NOLINTEND(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
	CHECK(!world->inspect(*replacement));
	std::cout
	    << "Box2D world passed: settling, impulses, queries, sensors, fixed ticks, bounded events, "
	       "stale/foreign IDs, native generation wrap and move ownership\n";
}
