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

namespace {
using namespace ice::physics;

int plane_queries_and_validation() {
	world3d_description settings;
	settings.max_bodies = 2;
	auto world          = world3d::create(settings);
	CHECK(world);
	body3d_description plane;
	plane.shape.kind = shape3d_kind::plane;
	CHECK(!world->create_body(plane));
	plane.motion = motion_type::kinematic;
	CHECK(!world->create_body(plane) && !world->body_count() && !world->tick());
	plane.motion   = motion_type::static_body;
	plane.rotation = {0, 0, 0, 0};
	CHECK(!world->create_body(plane));
	plane.rotation = {1, 0, 0, 0};
	auto floor     = world->create_body(plane);
	CHECK(floor && floor->slot == 1 && floor->generation == 1 && world->body_count() == 1);
	for (auto point : {ice::math::vec3{20, 10, -20}, ice::math::vec3{20000, 10, -30000},
	                   ice::math::vec3{1000000, 10, -1000000}}) {
		auto ray = world->ray_cast(point, {0, -20, 0});
		CHECK(ray && *ray && (*ray)->body == *floor && std::abs((*ray)->point.y) < .001f &&
		      std::abs((*ray)->fraction - .5f) < .001f && (*ray)->normal.y > .999f);
	}
	// The negative half-space is solid, rather than a zero-thickness triangle surface.
	auto inside = world->ray_cast({20000, -2, -30000}, {0, 4, 0});
	CHECK(inside && *inside && (*inside)->fraction == 0 && (*inside)->normal.y > .999f);
	// Translation and yaw can put valid world points far from the plane's local origin.
	CHECK(world->set_pose(*floor, {1000000, 0, 1000000},
	                      glm::angleAxis(glm::radians(45.f), ice::math::vec3(0, 1, 0))));
	auto opposite = world->ray_cast({-1000000, 10, -1000000}, {0, -20, 0});
	CHECK(opposite && *opposite && (*opposite)->body == *floor &&
	      std::abs((*opposite)->point.y) < .01f);
	// Body rotation defines the plane normal, including a vertical wall.
	CHECK(world->set_pose(*floor, {3, 4, 5},
	                      glm::angleAxis(glm::radians(-90.f), ice::math::vec3(0, 0, 1))));
	auto wall = world->ray_cast({10, 10000, -20000}, {-20, 0, 0});
	CHECK(wall && *wall && (*wall)->body == *floor && std::abs((*wall)->point.x - 3) < .01f &&
	      (*wall)->normal.x > .999f);
	return 0;
}

int plane_bodies_and_characters() {
	world3d_description settings;
	settings.max_bodies = 8;
	auto world          = world3d::create(settings);
	CHECK(world);
	body3d_description plane;
	plane.motion             = motion_type::static_body;
	plane.shape.kind         = shape3d_kind::plane;
	auto               floor = world->create_body(plane);
	body3d_description sphere;
	sphere.shape.kind            = shape3d_kind::sphere;
	sphere.position              = {30000, 3, -40000};
	auto                    ball = world->create_body(sphere);
	character3d_description nearby, distant;
	nearby.position  = {6, 2, 0};
	distant.position = {-25000, 2, 35000};
	auto walker = world->create_character(nearby), far = world->create_character(distant);
	CHECK(floor && ball && walker && far);
	CHECK(world->set_character_intent(*walker, {{4, 0, 0}, false}));
	for (unsigned tick = 0; tick < 360; ++tick)
		CHECK(world->step());
	auto body  = world->inspect(*ball);
	auto moved = world->inspect_character(*walker), supported = world->inspect_character(*far);
	CHECK(body && std::abs(body->position.y - .5f) < .05f);
	CHECK(moved && moved->position.x > 17 && std::abs(moved->position.y) < .04f &&
	      moved->ground_state == character_ground_state::on_ground && moved->ground_body == *floor);
	CHECK(supported && std::abs(supported->position.y) < .04f &&
	      supported->ground_state == character_ground_state::on_ground &&
	      supported->ground_body == *floor && supported->ground_normal.y > .999f);
	return 0;
}

int plane_sensors() {
	world3d_description settings;
	settings.gravity    = {0, 0, 0};
	settings.max_bodies = 2;
	auto world          = world3d::create(settings);
	CHECK(world);
	body3d_description plane;
	plane.motion            = motion_type::static_body;
	plane.shape.kind        = shape3d_kind::plane;
	plane.sensor            = true;
	auto               zone = world->create_body(plane);
	body3d_description sphere;
	sphere.shape.kind = shape3d_kind::sphere;
	sphere.position   = {20000, -.25f, -30000};
	auto ball         = world->create_body(sphere);
	CHECK(zone && ball && world->step());
	bool begin = false;
	for (const auto &event : world->contacts())
		begin |= event.sensor && event.phase == contact_phase::begin &&
		         (event.first == *zone || event.second == *zone);
	CHECK(begin && world->inspect(*ball)->position == sphere.position);
	auto ray = world->ray_cast({100, 10, 100}, {0, -20, 0});
	CHECK(ray && !*ray); // Ordinary queries exclude sensors, including plane half-spaces.
	CHECK(world->erase(*zone) && world->step());
	bool end = false;
	for (const auto &event : world->contacts())
		end |= event.sensor && event.phase == contact_phase::end &&
		       (event.first == *zone || event.second == *zone);
	CHECK(end && !world->inspect(*zone));
	return 0;
}
} // namespace

int main() {
	using namespace ice::physics;
	CHECK(plane_queries_and_validation() == 0);
	CHECK(plane_bodies_and_characters() == 0);
	CHECK(plane_sensors() == 0);
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
