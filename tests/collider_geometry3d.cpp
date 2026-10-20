#include "ice/physics/world3d.h"
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
std::size_t checks = 0;
void        check(bool value, int line) {
    ++checks;
    if (!value)
        throw std::runtime_error("Collider readback check " + std::to_string(line));
}
#define CHECK(value) check(bool(value), __LINE__)
bool near(const ice::math::vec3 &a, const ice::math::vec3 &b, float tolerance = .0002f) {
	return glm::length(a - b) <= tolerance;
}
} // namespace
int main() {
	try {
		using namespace ice::physics;
		using ice::math::vec3;
		auto made = world3d::create({});
		CHECK(made);
		auto               world = std::move(*made);
		body3d_description body;
		body.motion            = motion_type::static_body;
		body.shape.half_extent = {2, .02f, 1};
		body.position          = {3, 4, 5};
		body.sensor            = true;
		auto box               = world.create_body(body);
		CHECK(box);
		const auto epoch    = world.mutation_epoch();
		auto       captured = world.capture_collider(*box, {});
		CHECK(captured && captured->sensor && near(captured->position, body.position));
		const auto *actual_box = std::get_if<collider_box3d>(&captured->shape);
		CHECK(actual_box && actual_box->half_extent == body.shape.half_extent);
		CHECK(std::abs(actual_box->convex_radius - .002f) < .000001f);
		CHECK(world.mutation_epoch() == epoch && world.tick() == 0);
		body.sensor            = false;
		body.shape.kind        = shape3d_kind::capsule;
		body.shape.radius      = .35f;
		body.shape.half_height = .7f;
		auto capsule           = world.create_body(body);
		CHECK(capsule);
		auto capsule_copy = world.capture_collider(*capsule, {});
		CHECK(capsule_copy);
		const auto *actual_capsule = std::get_if<collider_capsule3d>(&capsule_copy->shape);
		CHECK(actual_capsule && actual_capsule->radius == .35f &&
		      actual_capsule->half_height == .7f);
		body.shape.half_height = 0;
		auto zero_capsule      = world.create_body(body);
		CHECK(zero_capsule);
		auto sphere_copy = world.capture_collider(*zero_capsule, {});
		CHECK(sphere_copy && std::get_if<collider_sphere3d>(&sphere_copy->shape));

		const std::array<vec3, 4>          vertices{{{4, 1, 2}, {6, 1, 2}, {6, 1, 4}, {4, 1, 4}}};
		const std::array<std::uint32_t, 6> indices{0, 2, 1, 0, 3, 2};
		auto cooked = cooked_static_shape::cook_mesh({vertices, indices});
		CHECK(cooked);
		body.static_shape = *cooked;
		body.position     = {10, 2, 0};
		body.rotation     = glm::angleAxis(glm::radians(90.f), vec3(0, 1, 0));
		auto mesh         = world.create_body(body);
		CHECK(mesh);
		auto too_small = world.capture_collider(*mesh, {1, 1024});
		CHECK(too_small && std::get<collider_unavailable3d>(too_small->shape).reason ==
		                       collider_unsupported3d::budget);
		auto exact = world.capture_collider(*mesh, {2, 6 * sizeof(vec3)});
		CHECK(exact);
		const auto *surface = std::get_if<collider_triangles3d>(&exact->shape);
		CHECK(surface && surface->vertices.size() == 6);
		for (const auto &v : surface->vertices)
			CHECK(v.x >= 3.9998f && v.x <= 6.0002f && v.z >= 1.9998f && v.z <= 4.0002f &&
			      std::abs(v.y - 1.f) < .0002f);
		auto hit = world.ray_cast(body.position + body.rotation * vec3(5, 5, 3),
		                          body.rotation * vec3(0, -10, 0));
		CHECK(hit && *hit && (*hit)->body == *mesh);
		CHECK(near((*hit)->point, exact->position + exact->rotation * vec3(5, 1, 3)));
		const auto owned = surface->vertices;
		CHECK(world.erase(*mesh));
		CHECK(!world.capture_collider(*mesh, {2, 1024}));
		CHECK(surface->vertices == owned);
		auto foreign = world3d::create({});
		CHECK(foreign && !foreign->capture_collider(*box, {}));
		body.static_shape = {};
		body.shape.kind   = shape3d_kind::plane;
		auto plane        = world.create_body(body);
		CHECK(plane);
		auto plane_copy = world.capture_collider(*plane, {});
		CHECK(plane_copy && std::get<collider_unavailable3d>(plane_copy->shape).reason ==
		                        collider_unsupported3d::plane);
		std::array<float, 16> heights{};
		auto terrain = cooked_static_shape::cook_heightfield({heights, 4, {0, 0, 0}, {1, 1, 1}});
		CHECK(terrain);
		body.static_shape = *terrain;
		body.rotation     = {1, 0, 0, 0};
		auto field        = world.create_body(body);
		CHECK(field);
		auto field_copy = world.capture_collider(*field, {1000, 100000});
		CHECK(field_copy && std::get<collider_unavailable3d>(field_copy->shape).reason ==
		                        collider_unsupported3d::heightfield);
		auto character = world.create_character({});
		CHECK(character);
		auto motor = world.inspect_character(*character);
		CHECK(motor);
		auto proxy = world.capture_collider(motor->proxy, {});
		CHECK(proxy && std::get<collider_unavailable3d>(proxy->shape).reason ==
		                   collider_unsupported3d::character_proxy);
		auto moved = std::move(world);
		CHECK(!world.capture_collider(*box, {}));
		CHECK(moved.capture_collider(*box, {}));
		std::cout << "Owned actual collider readback: " << checks << " checks passed\n";
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
