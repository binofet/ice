#include "ice/physics/world3d.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::fprintf(stderr, "Character3D failed at %d: %s\n", __LINE__, #condition);          \
			return 1;                                                                              \
		}                                                                                          \
	} while (false)

namespace {
using namespace ice::physics;
using ice::math::vec3;
ice::expected<body_id, diagnostic> box(world3d &world, vec3 position, vec3 half_extent,
                                       ice::math::quat rotation = {1, 0, 0, 0}) {
	body3d_description description;
	description.motion            = motion_type::static_body;
	description.position          = position;
	description.shape.half_extent = half_extent;
	description.rotation          = rotation;
	return world.create_body(description);
}
bool ticks(world3d &world, unsigned count) {
	for (unsigned tick = 0; tick < count; ++tick)
		if (auto result = world.step(); !result) {
			std::fprintf(stderr, "Character tick failed: %s\n", result.error().message.c_str());
			return false;
		}
	return true;
}
bool close(vec3 first, vec3 second, float tolerance = .001f) {
	return glm::length(first - second) < tolerance;
}

int validation_and_handles() {
	world3d_description settings;
	settings.max_bodies     = 4;
	settings.max_characters = 2;
	auto world = world3d::create(settings), other = world3d::create(settings);
	CHECK(world && other);
	const float nan      = std::numeric_limits<float>::quiet_NaN(),
	            infinity = std::numeric_limits<float>::infinity();
	std::vector<character3d_description> invalid;
	auto                                 add = [&](auto change) {
        character3d_description description;
        change(description);
        invalid.push_back(description);
	};
	add([&](auto &d) { d.position.x = nan; });
	add([&](auto &d) { d.linear_velocity.y = infinity; });
	add([](auto &d) { d.radius = 0; });
	add([](auto &d) { d.height = .1f; });
	add([](auto &d) { d.max_slope_angle = 0; });
	add([](auto &d) { d.max_slope_angle = glm::pi<float>(); });
	add([](auto &d) { d.step_height = -1; });
	add([](auto &d) { d.support_distance = 100; });
	add([](auto &d) { d.jump_speed = -1; });
	add([](auto &d) { d.mass = 0; });
	add([](auto &d) { d.max_push_force = -1; });
	add([](auto &d) { d.max_contacts = 0; });
	add([](auto &d) { d.max_contacts = 65537; });
	for (const auto &description : invalid)
		CHECK(!world->create_character(description) && !world->body_count() &&
		      !world->character_count() && !world->tick());
	character3d_description description;
	description.position = {0, 2, 0};
	auto first           = world->create_character(description);
	CHECK(first && first->slot == 1 && first->generation == 1);
	auto initial = world->inspect_character(*first);
	CHECK(initial && initial->id == *first && close(initial->position, description.position));
	CHECK(initial->ground_state == character_ground_state::in_air && !initial->jump_pending);
	CHECK(initial->proxy.owner == world->identity() && world->inspect(initial->proxy));
	CHECK(world->body_count() == 1 && world->character_count() == 1);
	CHECK(!other->inspect_character(*first) && !other->erase_character(*first));
	CHECK(!other->set_character_intent(*first, {}) && !other->cancel_character_intent(*first));
	CHECK(!other->teleport_character(*first, {0, 0, 0}));
	CHECK(!world->erase(initial->proxy) &&
	      !world->set_pose(initial->proxy, {4, 0, 0}, {1, 0, 0, 0}));
	CHECK(!world->apply_impulse(initial->proxy, {0, 1, 0}));
	CHECK(!world->set_character_intent(*first, {{nan, 0, 0}, true}));
	CHECK(!world->set_character_intent(*first, {{0, 1, 0}, true}));
	CHECK(!world->teleport_character(*first, {0, infinity, 0}));
	CHECK(!world->teleport_character(*first, {0, 0, 0}, {infinity, 0, 0}));
	CHECK(close(world->inspect_character(*first)->position, initial->position));
	CHECK(!world->inspect_character(*first)->jump_pending && !world->tick());
	auto second = world->create_character(description);
	CHECK(second && !world->create_character(description));
	CHECK(world->body_count() == 2 && world->character_count() == 2);
	CHECK(world->erase_character(*first));
	CHECK(!world->inspect(initial->proxy) && !world->inspect_character(*first));
	CHECK(!world->set_character_intent(*first, {}) && !world->teleport_character(*first, {}));
	CHECK(!world->cancel_character_intent(*first) && !world->erase_character(*first));
	auto replacement = world->create_character(description);
	CHECK(replacement && replacement->slot == first->slot &&
	      replacement->generation != first->generation);
	CHECK(world->inspect_character(*replacement)->proxy != initial->proxy);
	const auto lifetime = world->identity();
	auto       moved    = std::move(*world);
	CHECK(moved.identity() == lifetime && moved.inspect_character(*replacement));
	CHECK(!world->create_character() && !world->inspect_character(*replacement) &&
	      !world->erase_character(*replacement) && !world->character_count());
	auto old_other = other->create_character();
	CHECK(old_other);
	*other = std::move(moved);
	// The public moved-from world contract is unavailable, so these queries must fail safely.
	// NOLINTBEGIN(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
	CHECK(other->inspect_character(*replacement) && !other->inspect_character(*old_other) &&
	      !moved.inspect_character(*replacement));
	// NOLINTEND(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
	settings.max_characters = 0;
	auto disabled           = world3d::create(settings);
	CHECK(disabled && !disabled->create_character());
	settings.max_characters = 65537;
	CHECK(!world3d::create(settings));
	settings.max_characters = 2;
	settings.max_bodies     = 1;
	auto limited            = world3d::create(settings);
	CHECK(limited && limited->create_character() && !limited->create_character());
	for (auto gravity : {vec3(1, -9, 0), vec3(0, 9, 0), vec3(0, -9, 1)}) {
		settings.gravity = gravity;
		auto unsupported = world3d::create(settings);
		CHECK(unsupported && !unsupported->create_character());
	}
	return 0;
}

int prepared_shape_lifetimes() {
	world3d_description settings;
	settings.gravity        = {};
	settings.max_bodies     = 2;
	settings.max_characters = 1;
	auto world              = world3d::create(settings);
	CHECK(world);
	// Both the ordinary capsule and its zero-cylinder sphere must survive preparation,
	// native settings destruction and a simulation tick, then release on character erase.
	for (float height : {1.f, 2.f}) {
		character3d_description description;
		description.radius   = .5f;
		description.height   = height;
		description.position = {0, 2, 0};
		for (unsigned repeat = 0; repeat < 32; ++repeat) {
			auto character = world->create_character(description);
			CHECK(character && ticks(*world, 1));
			auto state = world->inspect_character(*character);
			CHECK(state);
			auto hit = world->ray_cast({0, 10, 0}, {0, -10, 0});
			CHECK(hit && *hit && (*hit)->body == state->proxy);
			CHECK(world->erase_character(*character));
			CHECK(!world->body_count() && !world->character_count());
			hit = world->ray_cast({0, 10, 0}, {0, -10, 0});
			CHECK(hit && !*hit);
		}
	}
	// The body adapter borrows the same native Result while settings retain the shape.
	for (auto kind : {shape3d_kind::sphere, shape3d_kind::capsule, shape3d_kind::box}) {
		body3d_description description;
		description.shape.kind        = kind;
		description.shape.half_height = 0;
		description.position          = {0, 2, 0};
		auto body                     = world->create_body(description);
		CHECK(body && ticks(*world, 1));
		auto hit = world->ray_cast({0, 10, 0}, {0, -10, 0});
		CHECK(hit && *hit && (*hit)->body == *body);
		CHECK(world->erase(*body) && !world->body_count());
	}
	return 0;
}

int ground_jump_and_queries() {
	world3d_description settings;
	settings.max_bodies = 8;
	auto world          = world3d::create(settings);
	CHECK(world);
	auto floor = box(*world, {0, -.5f, 0}, {20, .5f, 20});
	CHECK(floor);
	character3d_description description;
	description.position = {0, 3, 0};
	auto character       = world->create_character(description);
	CHECK(character);
	auto initial = world->inspect_character(*character);
	CHECK(initial);
	auto ray = world->ray_cast({0, 8, 0}, {0, -10, 0});
	CHECK(ray && *ray && (*ray)->body == initial->proxy);
	const std::array ignored{initial->proxy};
	ray = world->ray_cast({0, 8, 0}, {0, -10, 0}, ignored);
	CHECK(ray && *ray && (*ray)->body == *floor);
	const std::array invalid{body_id{world->identity(), 0, 1}};
	CHECK(!world->ray_cast({0, 8, 0}, {0, -10, 0}, invalid));
	const std::vector<body_id> excessive(settings.max_bodies + 1, initial->proxy);
	CHECK(!world->ray_cast({0, 8, 0}, {0, -10, 0}, excessive));
	CHECK(ticks(*world, 240));
	auto grounded = world->inspect_character(*character);
	CHECK(grounded && std::abs(grounded->position.y) < .03f);
	CHECK(grounded->ground_state == character_ground_state::on_ground &&
	      grounded->ground_body == *floor && grounded->ground_normal.y > .99f);
	CHECK(!grounded->contacts_truncated);
	// Render/input frames can replace horizontal input without consuming a queued jump.
	CHECK(world->set_character_intent(*character, {{0, 0, 0}, true}));
	for (unsigned frame = 0; frame < 10; ++frame)
		CHECK(world->set_character_intent(*character, {}) &&
		      world->inspect_character(*character)->jump_pending);
	CHECK(world->inspect_character(*character)->position == grounded->position);
	CHECK(world->cancel_character_intent(*character));
	CHECK(!world->inspect_character(*character)->jump_pending && ticks(*world, 1));
	CHECK(std::abs(world->inspect_character(*character)->position.y) < .03f);
	CHECK(world->set_character_intent(*character, {{0, 0, 0}, true}) && ticks(*world, 1));
	CHECK(!world->inspect_character(*character)->jump_pending &&
	      world->inspect_character(*character)->linear_velocity.y > 5);
	float peak = 0;
	for (unsigned tick = 0; tick < 300; ++tick) {
		CHECK(ticks(*world, 1));
		peak = std::max(peak, world->inspect_character(*character)->position.y);
	}
	CHECK(peak > 1 && peak < 2 &&
	      std::abs(world->inspect_character(*character)->position.y) < .03f);
	CHECK(world->set_character_intent(*character, {{2, 0, 0}, false}) && ticks(*world, 120));
	CHECK(std::abs(world->inspect_character(*character)->position.x - 2) < .03f);
	CHECK(world->set_character_intent(*character, {{2, 0, 0}, true}));
	CHECK(world->teleport_character(*character, {4, 3, 0}, {0, -1, 0}));
	auto teleported = world->inspect_character(*character);
	CHECK(teleported && teleported->position == vec3(4, 3, 0) &&
	      teleported->linear_velocity == vec3(0, -1, 0) && !teleported->jump_pending);
	CHECK(teleported->ground_state == character_ground_state::in_air &&
	      !teleported->ground_body.owner);
	CHECK(ticks(*world, 120));
	CHECK(std::abs(world->inspect_character(*character)->position.x - 4) < .03f);
	CHECK(std::abs(world->inspect_character(*character)->position.y) < .03f);
	const auto retired_proxy = teleported->proxy;
	CHECK(world->erase_character(*character));
	const std::array retired{retired_proxy};
	CHECK(!world->ray_cast({0, 8, 0}, {0, -10, 0}, retired));
	CHECK(ticks(*world, 1));
	return 0;
}

int wall_ceiling_and_steps() {
	auto world = world3d::create();
	CHECK(world && box(*world, {0, -.5f, 0}, {20, .5f, 20}));
	CHECK(box(*world, {2, 2, 0}, {.15f, 2, 5}));
	auto character = world->create_character();
	CHECK(character && world->set_character_intent(*character, {{3, 0, 1}, false}) &&
	      ticks(*world, 180));
	auto sliding = world->inspect_character(*character);
	CHECK(sliding && sliding->position.x > 1.3f && sliding->position.x < 1.6f &&
	      sliding->position.z > 1.2f && std::abs(sliding->position.y) < .05f);
	CHECK(world->erase_character(*character));
	CHECK(box(*world, {-4, 2.3f, 0}, {2, .1f, 2}));
	character3d_description description;
	description.position = {-4, 0, 0};
	character            = world->create_character(description);
	CHECK(character && ticks(*world, 2) &&
	      world->set_character_intent(*character, {{0, 0, 0}, true}));
	float peak            = 0;
	bool  ceiling_stopped = false;
	for (unsigned tick = 0; tick < 120; ++tick) {
		CHECK(ticks(*world, 1));
		auto current = world->inspect_character(*character);
		peak         = std::max(peak, current->position.y);
		if (current->position.y > .3f && current->linear_velocity.y <= 0)
			ceiling_stopped = true;
	}
	CHECK(peak > .25f && peak < .45f && ceiling_stopped);
	CHECK(std::abs(world->inspect_character(*character)->position.y) < .05f);
	for (float step_height : {.35f, 0.f}) {
		auto stairs = world3d::create();
		CHECK(stairs && box(*stairs, {0, -.5f, 0}, {10, .5f, 10}));
		CHECK(box(*stairs, {0, .125f, 0}, {.75f, .125f, 2}));
		character3d_description climber;
		climber.position    = {-2, 0, 0};
		climber.step_height = step_height;
		auto id             = stairs->create_character(climber);
		CHECK(id && stairs->set_character_intent(*id, {{2, 0, 0}, false}) && ticks(*stairs, 120));
		auto result = stairs->inspect_character(*id);
		if (step_height > 0)
			CHECK(result->position.x > -.2f && std::abs(result->position.y - .25f) < .04f);
		else
			CHECK(result->position.x < -1 && std::abs(result->position.y) < .04f);
	}
	return 0;
}

int slopes_and_characters() {
	for (float degrees : {20.f, 60.f}) {
		auto world = world3d::create();
		CHECK(world && box(*world, {0, -.5f, 0}, {20, .5f, 20}));
		const float angle = glm::radians(degrees);
		CHECK(box(*world, {0, 3 * std::sin(angle) - .1f * std::cos(angle), 0}, {3, .1f, 2},
		          glm::angleAxis(angle, vec3(0, 0, 1))));
		character3d_description description;
		description.position    = {-3 * std::cos(angle) - .1f * std::sin(angle) - .8f, 0, 0};
		description.step_height = 0;
		auto character          = world->create_character(description);
		CHECK(character && world->set_character_intent(*character, {{2, 0, 0}, false}) &&
		      ticks(*world, 240));
		auto result = world->inspect_character(*character);
		if (degrees < 50)
			CHECK(result->position.y > .7f && result->ground_normal.y > .8f);
		else
			CHECK(result->position.y < .4f && result->position.x < -1.2f);
	}
	world3d_description settings;
	settings.max_bodies     = 8;
	settings.max_characters = 2;
	auto world              = world3d::create(settings);
	CHECK(world && box(*world, {0, -.5f, 0}, {20, .5f, 20}));
	character3d_description a_description, b_description;
	a_description.position.x = -1;
	b_description.position.x = 1;
	auto first               = world->create_character(a_description),
	     second              = world->create_character(b_description);
	CHECK(first && second && world->set_character_intent(*first, {{3, 0, 0}, false}) &&
	      ticks(*world, 120));
	auto a = world->inspect_character(*first), b = world->inspect_character(*second);
	// Characters may push one another; preserve their separation/order rather than
	// requiring the initially idle character to behave like an immovable wall.
	CHECK(a && b && b->position.x - a->position.x > .55f && b->position.x >= 1);
	// Other motors can cache a retired virtual-character pointer until the next fixed tick.
	CHECK(world->erase_character(*second));
	b_description.position.x = 8;
	for (unsigned index = 0; index < 600; ++index) {
		auto replacement = world->create_character(b_description);
		CHECK(replacement && world->erase_character(*replacement));
	}
	CHECK(ticks(*world, 120));
	CHECK(world->inspect_character(*first)->position.x > 2);
	CHECK(world->character_count() == 1 && world->body_count() == 2);
	return 0;
}

int pushing_and_contact_retirement() {
	auto world = world3d::create();
	CHECK(world && box(*world, {0, -.5f, 0}, {20, .5f, 20}));
	body3d_description pushable;
	pushable.position          = {1, .31f, 0};
	pushable.shape.half_extent = {.3f, .3f, .3f};
	pushable.mass              = 2;
	pushable.friction          = .2f;
	auto pushed                = world->create_body(pushable);
	auto character             = world->create_character();
	CHECK(pushed && character && world->set_character_intent(*character, {{2, 0, 0}, false}) &&
	      ticks(*world, 180));
	CHECK(world->inspect(*pushed)->position.x > 2 &&
	      world->inspect_character(*character)->position.x > 1);
	// Character proxies must trigger stationary as well as simulated sensor volumes.
	for (auto motion : {motion_type::static_body, motion_type::dynamic}) {
		world3d_description settings;
		settings.gravity        = {0, 0, 0};
		settings.max_bodies     = 2;
		settings.max_characters = 1;
		auto churn              = world3d::create(settings);
		CHECK(churn);
		body3d_description sensor;
		sensor.motion            = motion;
		sensor.sensor            = true;
		sensor.position          = {0, .9f, 0};
		sensor.shape.half_extent = {1, 1, 1};
		auto zone                = churn->create_body(sensor);
		auto original            = churn->create_character();
		CHECK(zone && original);
		const auto old_proxy = churn->inspect_character(*original)->proxy;
		CHECK(ticks(*churn, 1));
		bool old_begin = false;
		for (const auto &event : churn->contacts())
			old_begin |= event.phase == contact_phase::begin && event.sensor &&
			             (event.first == old_proxy || event.second == old_proxy);
		CHECK(old_begin && churn->erase_character(*original));
		character_id current;
		for (unsigned index = 0; index < 600; ++index) {
			auto replacement = churn->create_character();
			CHECK(replacement);
			if (index < 599)
				CHECK(churn->erase_character(*replacement));
			else
				current = *replacement;
		}
		const auto new_proxy = churn->inspect_character(current)->proxy;
		CHECK(new_proxy != old_proxy && !churn->inspect(old_proxy));
		CHECK(churn->body_count() == 2 && churn->character_count() == 1 && ticks(*churn, 1));
		bool old_end = false, new_begin = false;
		for (const auto &event : churn->contacts()) {
			CHECK(event.first.owner == churn->identity() &&
			      event.second.owner == churn->identity());
			old_end |= event.phase == contact_phase::end &&
			           (event.first == old_proxy || event.second == old_proxy);
			new_begin |= event.phase == contact_phase::begin &&
			             (event.first == new_proxy || event.second == new_proxy);
		}
		CHECK(old_end && new_begin);
		CHECK(churn->erase_character(current));
	}
	return 0;
}
} // namespace
int main() {
	CHECK(validation_and_handles() == 0);
	CHECK(prepared_shape_lifetimes() == 0);
	CHECK(ground_jump_and_queries() == 0);
	CHECK(wall_ceiling_and_steps() == 0);
	CHECK(slopes_and_characters() == 0);
	CHECK(pushing_and_contact_retirement() == 0);
	std::puts("Character3D passed: motor, collision, inputs, queries, budgets and lifetime safety");
}
