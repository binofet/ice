#include "ice/physics/world3d.h"
#include <cmath>
#include <cstdio>
#include <limits>

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::fprintf(stderr, "Physics3D failed at %d: %s\n", __LINE__, #condition);            \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)
int main() {
	using namespace ice::physics;
	auto bad      = world3d_description{};
	bad.time_step = std::numeric_limits<float>::quiet_NaN();
	CHECK(!world3d::create(bad));
	world3d_description settings;
	settings.max_bodies         = 8;
	settings.max_contact_events = 64;
	auto world                  = world3d::create(settings);
	auto other                  = world3d::create(settings);
	CHECK(world && other && world->identity() != other->identity());
	body3d_description ground;
	ground.motion            = motion_type::static_body;
	ground.position          = {0, -.5f, 0};
	ground.shape.half_extent = {10, .5f, 10};
	auto floor               = world->create_body(ground);
	CHECK(floor);
	body3d_description ball;
	ball.position     = {0, 3, 0};
	ball.shape.kind   = shape3d_kind::sphere;
	ball.shape.radius = .5f;
	ball.continuous   = true;
	auto body         = world->create_body(ball);
	CHECK(body && !other->inspect(*body));
	auto invalid     = ball;
	invalid.rotation = {0, 0, 0, 0};
	CHECK(!world->create_body(invalid) && world->body_count() == 2);
	CHECK(!world->set_pose(*body, {0, std::numeric_limits<float>::infinity(), 0}, {1, 0, 0, 0}));
	CHECK(!world->apply_impulse(*floor, {1, 0, 0}));
	auto ray = world->ray_cast({0, 6, 0}, {0, -10, 0});
	CHECK(ray && *ray && (*ray)->body == *body && std::abs((*ray)->point.y - 3.5f) < .01f);
	CHECK(!world->ray_cast({0, 0, 0}, {0, 0, 0}));
	bool contact = false;
	for (int i = 0; i < 360; ++i) {
		CHECK(world->step());
		for (const auto &event : world->contacts()) {
			CHECK(event.first.owner == world->identity() &&
			      event.second.owner == world->identity());
			contact |= event.phase == contact_phase::begin;
		}
	}
	auto resting = world->inspect(*body);
	CHECK(contact && resting && resting->position.y > .45f && resting->position.y < .6f);
	CHECK(world->apply_impulse(*body, {0, 3, 0}));
	CHECK(world->step());
	auto rising = world->inspect(*body);
	CHECK(rising && rising->linear_velocity.y > 0);
	CHECK(world->set_pose(*body, {2, 4, 0}, {1, 0, 0, 0}));
	auto placed = world->inspect(*body);
	CHECK(placed && placed->position == ice::math::vec3(2, 4, 0) &&
	      glm::length(placed->linear_velocity) == 0);
	auto retired = *body;
	CHECK(world->erase(*body));
	auto replacement = world->create_body(ball);
	CHECK(replacement && replacement->slot == retired.slot &&
	      replacement->generation != retired.generation);
	CHECK(!world->inspect(retired) && !world->erase(retired) &&
	      !world->apply_impulse(retired, {1, 0, 0}));
	auto identity = world->identity();
	auto moved    = std::move(*world);
	CHECK(moved.identity() == identity && moved.inspect(*replacement) &&
	      !world->inspect(*replacement));
	// Fresh identical worlds integrate the same fixed input sequence on this build/platform.
	auto first = world3d::create(settings), second = world3d::create(settings);
	CHECK(first && second);
	auto a = first->create_body(ball), b = second->create_body(ball);
	CHECK(a && b);
	for (int i = 0; i < 30; ++i) {
		CHECK(first->step());
		CHECK(second->step());
	}
	CHECK(first->inspect(*a)->position == second->inspect(*b)->position);
	// A thin surface catches a fast body through continuous linear casting.
	world3d_description ccd_settings;
	ccd_settings.gravity = {0, 0, 0};
	auto ccd             = world3d::create(ccd_settings);
	CHECK(ccd);
	ground.position          = {0, 0, 0};
	ground.shape.half_extent = {.02f, 2, 2};
	CHECK(ccd->create_body(ground));
	ball.position        = {-2, 0, 0};
	ball.linear_velocity = {500, 0, 0};
	ball.shape.radius    = .1f;
	ball.restitution     = 0;
	auto projectile      = ccd->create_body(ball);
	CHECK(projectile && ccd->step());
	CHECK(ccd->inspect(*projectile)->position.x < .2f);
	// A native Jolt ID has eight sequence bits. Hundreds of edits between updates must
	// still produce the old body's end and the replacement's begin with distinct ICE IDs.
	world3d_description churn_settings;
	churn_settings.gravity    = {0, 0, 0};
	churn_settings.max_bodies = 2;
	auto churn                = world3d::create(churn_settings);
	CHECK(churn);
	body3d_description sensor;
	sensor.motion           = motion_type::static_body;
	sensor.sensor           = true;
	auto               zone = churn->create_body(sensor);
	body3d_description overlap;
	overlap.shape.kind = shape3d_kind::sphere;
	auto original      = churn->create_body(overlap);
	CHECK(zone && original && churn->step() && !churn->contacts().empty());
	CHECK(churn->erase(*original));
	for (int i = 0; i < 512; ++i) {
		auto transient = churn->create_body(overlap);
		CHECK(transient && churn->erase(*transient));
	}
	auto current = churn->create_body(overlap);
	CHECK(current && churn->step());
	bool old_end = false, new_begin = false;
	for (const auto &event : churn->contacts()) {
		CHECK(event.sensor);
		old_end |= event.phase == contact_phase::end &&
		           (event.first == *original || event.second == *original);
		new_begin |= event.phase == contact_phase::begin &&
		             (event.first == *current || event.second == *current);
	}
	CHECK(old_end && new_begin);
	CHECK(churn->erase(*current) && churn->step());
	CHECK(churn->contacts().size() == 1);
	CHECK(churn->contacts()[0].phase == contact_phase::end &&
	      (churn->contacts()[0].first == *current || churn->contacts()[0].second == *current));
}
