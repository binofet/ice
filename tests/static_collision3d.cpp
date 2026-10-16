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
			std::fprintf(stderr, "Static collision3D failed at %d: %s\n", __LINE__, #condition);   \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

namespace {
using namespace ice::physics;
using ice::math::vec3;

// Constructed before Jolt registration and populated afterward: this owner is destroyed
// after the function-static registry owner, exercising shared registration lifetime at exit.
cooked_static_shape teardown_owner;

constexpr std::array<std::uint32_t, 6> quad_indices{0, 1, 2, 0, 2, 3};
const std::array<vec3, 4>              quad_vertices{vec3{0, 0, -2}, vec3{0, 0, 2}, vec3{4, 0, 2},
                                        vec3{4, 0, -2}};

body3d_description surface(const cooked_static_shape &shape, vec3 position = {0, 0, 0}) {
	body3d_description result;
	result.motion       = motion_type::static_body;
	result.static_shape = shape;
	result.position     = position;
	result.friction     = 1;
	result.restitution  = 0;
	return result;
}

int validation_and_pre_world_cooking() {
	CHECK(!cooked_static_shape{} && cooked_static_shape{}.identity() == 0 &&
	      cooked_static_shape{}.statistics().cooked_bytes == 0);
	CHECK(!cooked_static_shape::cook_mesh({}));
	CHECK(!cooked_static_shape::cook_mesh({quad_vertices, std::span(quad_indices).first(5)}));
	auto indices = quad_indices;
	indices[5]   = 4;
	CHECK(!cooked_static_shape::cook_mesh({quad_vertices, indices}));
	indices    = quad_indices;
	indices[1] = indices[0];
	CHECK(!cooked_static_shape::cook_mesh({quad_vertices, indices}));
	auto vertices = quad_vertices;
	vertices[1]   = {0, 0, 0};
	vertices[2]   = {0, 0, 2};
	CHECK(!cooked_static_shape::cook_mesh({vertices, quad_indices}));
	for (float invalid : {std::numeric_limits<float>::quiet_NaN(),
	                      std::numeric_limits<float>::infinity(), 1000001.f}) {
		vertices      = quad_vertices;
		vertices[2].z = invalid;
		CHECK(!cooked_static_shape::cook_mesh({vertices, quad_indices}));
	}
	std::array<float, 16>     heights{};
	heightfield3d_description terrain{heights, 4};
	CHECK(!cooked_static_shape::cook_heightfield({}));
	CHECK(!cooked_static_shape::cook_heightfield({std::span(heights).first(15), 4}));
	CHECK(!cooked_static_shape::cook_heightfield({heights, 3}));
	CHECK(!cooked_static_shape::cook_heightfield({{}, std::numeric_limits<std::uint32_t>::max()}));
	CHECK(!cooked_static_shape::cook_heightfield({{}, 16383}));
	for (float invalid :
	     {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
	      std::numeric_limits<float>::max(), 1000001.f}) {
		heights[5] = invalid;
		CHECK(!cooked_static_shape::cook_heightfield(terrain));
	}
	heights[5] = 0;
	for (vec3 invalid : {vec3{0, 1, 1}, vec3{1, -1, 1}, vec3{1, 1, .0001f},
	                     vec3{1, std::numeric_limits<float>::quiet_NaN(), 1}}) {
		terrain.scale = invalid;
		CHECK(!cooked_static_shape::cook_heightfield(terrain));
	}
	terrain.scale  = {1, 1, 1};
	terrain.offset = {1000000, 0, 0};
	CHECK(!cooked_static_shape::cook_heightfield(terrain));
	terrain.offset = {0, 1000001, 0};
	CHECK(!cooked_static_shape::cook_heightfield(terrain));
	terrain.offset = {0, 0, 0};
	// Both cookers initialize the backend themselves, with no previously created world.
	auto mesh  = cooked_static_shape::cook_mesh({quad_vertices, quad_indices});
	auto field = cooked_static_shape::cook_heightfield(terrain);
	CHECK(mesh && field && *mesh && *field && mesh->identity() != field->identity());
	CHECK(mesh->statistics().source_vertices == 4 && mesh->statistics().source_triangles == 2 &&
	      mesh->statistics().cooked_bytes > 0);
	CHECK(field->statistics().source_vertices == 16 && field->statistics().source_triangles == 18 &&
	      field->statistics().cooked_bytes > 0);
	teardown_owner = *mesh;
	return 0;
}

int reuse_lifetime_and_static_policy() {
	auto vertices = std::vector<vec3>(quad_vertices.begin(), quad_vertices.end());
	auto indices  = std::vector<std::uint32_t>(quad_indices.begin(), quad_indices.end());
	auto cooked   = cooked_static_shape::cook_mesh({vertices, indices});
	CHECK(cooked);
	auto       first_owner = *cooked;
	const auto identity    = first_owner.identity();
	const auto statistics  = first_owner.statistics();
	auto       moved_owner = std::move(first_owner);
	CHECK(moved_owner.identity() == identity);
	vertices.assign(vertices.size(), vec3(std::numeric_limits<float>::quiet_NaN()));
	indices.assign(indices.size(), std::numeric_limits<std::uint32_t>::max());
	std::vector<vec3>().swap(vertices);
	std::vector<std::uint32_t>().swap(indices);
	world3d_description settings;
	settings.max_bodies = 4;
	auto world          = world3d::create(settings);
	CHECK(world);
	auto description   = surface(moved_owner);
	description.motion = motion_type::dynamic;
	CHECK(!world->create_body(description) && world->body_count() == 0);
	description.motion = motion_type::kinematic;
	CHECK(!world->create_body(description) && world->body_count() == 0);
	description.motion = motion_type::static_body;
	description.sensor = true;
	CHECK(!world->create_body(description) && world->body_count() == 0);
	description.sensor   = false;
	description.position = {1000000, 0, 0};
	CHECK(!world->create_body(description) && world->body_count() == 0);
	description.position = {0, 0, 0};
	// Primitive fields are unused when a cooked resource is supplied.
	description.shape.radius = std::numeric_limits<float>::quiet_NaN();
	description.shape.kind   = static_cast<shape3d_kind>(99);
	auto floor               = world->create_body(description);
	CHECK(floor && floor->slot == 1 && floor->generation == 1 &&
	      world->inspect(*floor)->static_shape_identity == identity);
	CHECK(!world->set_pose(*floor, {1000000, 0, 0}, {1, 0, 0, 0}));
	CHECK(world->inspect(*floor)->position == vec3(0));
	{
		auto other = world3d::create(settings);
		CHECK(other);
		auto reused = other->create_body(surface(moved_owner, {0, 2, 0}));
		CHECK(reused && other->inspect(*reused)->static_shape_identity == identity);
		auto ray = other->ray_cast({2, 6, 0}, {0, -8, 0});
		CHECK(ray && *ray && std::abs((*ray)->point.y - 2) < .001f);
	}
	CHECK(moved_owner.statistics().cooked_bytes == statistics.cooked_bytes &&
	      moved_owner.statistics().cook_nanoseconds == statistics.cook_nanoseconds);
	// Retiring one world does not retire the shared geometry or its registration owner.
	auto copied = moved_owner;
	moved_owner = {};
	CHECK(copied.identity() == identity);
	description.static_shape = {};
	*cooked                  = {};
	copied                   = {};
	// The body is now the only native owner of this particular geometry.
	world3d relocated = std::move(*world);
	CHECK(!world->identity() && relocated.inspect(*floor)->static_shape_identity == identity);
	auto ray = relocated.ray_cast({2, 5, 0}, {0, -10, 0});
	CHECK(ray && *ray && (*ray)->body == *floor && (*ray)->normal.y > .999f);
	const auto rotation = glm::angleAxis(glm::radians(-90.f), vec3{0, 0, 1});
	CHECK(relocated.set_pose(*floor, {3, 0, 0}, rotation));
	auto wall = relocated.ray_cast({10, -2, 0}, {-20, 0, 0});
	CHECK(wall && *wall && std::abs((*wall)->point.x - 3) < .001f && (*wall)->normal.x > .999f);
	CHECK(relocated.erase(*floor) && !relocated.inspect(*floor));
	return 0;
}

float terrain_height(float x, float z) {
	return std::clamp((x + 2) * .25f, 0.f, 1.f) + std::abs(z) * .025f;
}

int terrain_support(bool mesh_geometry) {
	constexpr unsigned         count = 17;
	std::vector<float>         heights;
	std::vector<vec3>          vertices;
	std::vector<std::uint32_t> indices;
	for (unsigned z = 0; z < count; ++z)
		for (unsigned x = 0; x < count; ++x) {
			float world_x = float(x) - 8, world_z = float(z) - 8;
			heights.push_back(terrain_height(world_x, world_z));
			vertices.emplace_back(world_x, heights.back(), world_z);
		}
	for (unsigned z = 0; z + 1 < count; ++z)
		for (unsigned x = 0; x + 1 < count; ++x) {
			auto a = z * count + x, b = a + count, c = b + 1, d = a + 1;
			indices.insert(indices.end(), {a, b, c, a, c, d});
		}
	auto cooked =
	    mesh_geometry
	        ? cooked_static_shape::cook_mesh({vertices, indices})
	        : cooked_static_shape::cook_heightfield({heights, count, {-8, 0, -8}, {1, 1, 1}});
	CHECK(cooked);
	world3d_description settings;
	settings.max_bodies = 8;
	auto world          = world3d::create(settings);
	CHECK(world);
	auto floor = world->create_body(surface(*cooked));
	CHECK(floor);
	for (vec3 point : {vec3{-5.3f, 5, 2.2f}, vec3{-.3f, 5, -1.7f}, vec3{5.4f, 5, 3.1f}}) {
		auto ray = world->ray_cast(point, {0, -10, 0});
		CHECK(ray && *ray && (*ray)->body == *floor && (*ray)->normal.y > .96f &&
		      std::abs((*ray)->point.y - terrain_height(point.x, point.z)) < .002f);
	}
	// Rays query either face, retaining the authored front normal rather than treating the
	// entire volume below a terrain sample as solid with an immediate fraction-zero hit.
	auto backface = world->ray_cast({0, -3, 0}, {0, 6, 0});
	CHECK(backface && *backface && (*backface)->fraction > .4f && (*backface)->normal.y > .96f);
	body3d_description box;
	box.position                    = {5, 4, 3};
	box.shape.half_extent           = {.4f, .4f, .4f};
	box.friction                    = 1;
	box.restitution                 = 0;
	auto                    resting = world->create_body(box);
	character3d_description character;
	character.position = {-6, 2, 0};
	auto walker        = world->create_character(character);
	CHECK(resting && walker);
	for (unsigned tick = 0; tick < 180; ++tick)
		CHECK(world->step());
	auto supported = world->inspect_character(*walker);
	CHECK(supported && supported->ground_state == character_ground_state::on_ground &&
	      supported->ground_body == *floor && std::abs(supported->position.y) < .05f);
	CHECK(world->set_character_intent(*walker, {{3, 0, 0}, false}));
	float previous_y = supported->position.y;
	for (unsigned tick = 0; tick < 480; ++tick) {
		CHECK(world->step());
		auto current = world->inspect_character(*walker);
		CHECK(current && current->position.y >= -.05f &&
		      std::abs(current->position.y - previous_y) < .08f);
		previous_y = current->position.y;
	}
	auto moved = world->inspect_character(*walker);
	if (moved)
		std::printf("%s terrain walk: %.4f %.4f %.4f, ground=%d\n",
		            mesh_geometry ? "mesh" : "heightfield", double(moved->position.x),
		            double(moved->position.y), double(moved->position.z), int(moved->ground_state));
	// Contact projection spends some requested horizontal motion climbing the slope.
	CHECK(moved && moved->position.x > 5.5f && moved->position.x < 6.2f &&
	      std::abs(moved->position.y - 1) < .05f &&
	      moved->ground_state == character_ground_state::on_ground && moved->ground_body == *floor);
	auto body = world->inspect(*resting);
	CHECK(body && body->position.x > 4.8f && body->position.x < 5.2f &&
	      std::abs(body->position.y - (terrain_height(5, 3) + .4f)) < .05f &&
	      glm::length(body->linear_velocity) < .05f);
	return 0;
}

int separate_tile_seams(bool mesh_geometry) {
	std::array<float, 25> heights{};
	auto                  cooked = mesh_geometry
	                                   ? cooked_static_shape::cook_mesh({quad_vertices, quad_indices})
	                                   : cooked_static_shape::cook_heightfield({heights, 5, {0, 0, -2}, {1, 1, 1}});
	CHECK(cooked);
	world3d_description settings;
	settings.max_bodies = 4;
	auto world          = world3d::create(settings);
	CHECK(world);
	auto left  = world->create_body(surface(*cooked, {-4, 0, 0}));
	auto right = world->create_body(surface(*cooked));
	CHECK(left && right &&
	      world->inspect(*left)->static_shape_identity ==
	          world->inspect(*right)->static_shape_identity);
	character3d_description character;
	character.position = {-3, 1, .13f};
	auto walker        = world->create_character(character);
	CHECK(walker);
	for (unsigned tick = 0; tick < 120; ++tick)
		CHECK(world->step());
	CHECK(world->set_character_intent(*walker, {{1.5f, 0, 0}, false}));
	for (unsigned tick = 0; tick < 480; ++tick) {
		CHECK(world->step());
		auto current = world->inspect_character(*walker);
		CHECK(current && std::abs(current->position.y) < .05f);
	}
	auto moved = world->inspect_character(*walker);
	CHECK(moved && moved->position.x > 2.9f && moved->position.x < 3.1f &&
	      moved->ground_state == character_ground_state::on_ground && moved->ground_body == *right);
	for (float x : {-.001f, 0.f, .001f}) {
		auto ray = world->ray_cast({x, 2, -.17f}, {0, -4, 0});
		CHECK(ray && *ray && std::abs((*ray)->point.y) < .001f);
	}
	return 0;
}

int heightfield_diagonal_and_mesh_holes() {
	std::array<float, 16> heights{};
	heights[5] = 2;
	auto field = cooked_static_shape::cook_heightfield({heights, 4});
	CHECK(field);
	auto world = world3d::create();
	CHECK(world);
	auto terrain = world->create_body(surface(*field));
	CHECK(terrain);
	for (vec3 point : {vec3{.25f, 4, .75f}, vec3{.75f, 4, .25f}, vec3{.5f, 4, .5f}}) {
		auto       ray      = world->ray_cast(point, {0, -8, 0});
		const auto expected = 2 * std::min(point.x, point.z);
		CHECK(ray && *ray && std::abs((*ray)->point.y - expected) < .002f);
	}
	// Height samples were copied; changing source data cannot change the cooked surface.
	heights.fill(100);
	auto retained = world->ray_cast({.5f, 4, .5f}, {0, -8, 0});
	CHECK(retained && *retained && std::abs((*retained)->point.y - 1) < .002f);
	auto outside = world->ray_cast({3.5f, 4, 1}, {0, -8, 0});
	CHECK(outside && !*outside);
	CHECK(world->erase(*terrain));
	// Omitting a triangle is a legitimate mesh hole, independent of unsupported height sentinels.
	auto mesh = cooked_static_shape::cook_mesh({quad_vertices, std::span(quad_indices).first(3)});
	CHECK(mesh);
	auto half = world->create_body(surface(*mesh));
	CHECK(half);
	auto covered = world->ray_cast({1, 3, 1}, {0, -6, 0});
	auto hole    = world->ray_cast({3, 3, -1}, {0, -6, 0});
	CHECK(covered && *covered && hole && !*hole);
	return 0;
}

int triangle_front_policy() {
	auto reversed = quad_indices;
	std::swap(reversed[1], reversed[2]);
	std::swap(reversed[4], reversed[5]);
	auto mesh = cooked_static_shape::cook_mesh({quad_vertices, reversed});
	CHECK(mesh);
	auto world = world3d::create();
	CHECK(world);
	auto floor = world->create_body(surface(*mesh));
	CHECK(floor);
	auto ray = world->ray_cast({2, 3, 0}, {0, -6, 0});
	CHECK(ray && *ray && (*ray)->normal.y < -.999f);
	body3d_description sphere;
	sphere.shape.kind            = shape3d_kind::sphere;
	sphere.position              = {1, 2, 0};
	auto                    body = world->create_body(sphere);
	character3d_description character;
	character.position = {3, 2, 0};
	auto walker        = world->create_character(character);
	CHECK(body && walker);
	for (unsigned tick = 0; tick < 180; ++tick)
		CHECK(world->step());
	CHECK(world->inspect(*body)->position.y < -2 &&
	      world->inspect_character(*walker)->position.y < -2);
	return 0;
}
} // namespace

int main() {
	if (validation_and_pre_world_cooking() || reuse_lifetime_and_static_policy() ||
	    terrain_support(true) || terrain_support(false) || separate_tile_seams(true) ||
	    separate_tile_seams(false) || heightfield_diagonal_and_mesh_holes() ||
	    triangle_front_policy())
		return 1;
	std::puts("Static collision3D passed: pre-world cooking, owned/reused immutable geometry, "
	          "validation, "
	          "varied terrain rays/resting bodies/characters, shared edges/seams and mesh holes");
}
