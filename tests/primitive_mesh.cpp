#include "ice/assets/primitive_mesh.h"
#include <cmath>
#include <cstdio>
#include <limits>

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::fprintf(stderr, "Primitive check failed at %d: %s\n", __LINE__, #condition);      \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)
int main() {
	using namespace ice::assets;
	for (auto kind : {primitive_kind::box, primitive_kind::sphere, primitive_kind::capsule,
	                  primitive_kind::pyramid, primitive_kind::triangle, primitive_kind::plane}) {
		primitive_description recipe;
		recipe.kind = kind;
		auto result = make_primitive(recipe);
		CHECK(result && !result->vertices.empty() && result->indices.size() % 3 == 0);
		for (const auto &vertex : result->vertices) {
			ice::math::vec3 normal(vertex.normal[0], vertex.normal[1], vertex.normal[2]);
			CHECK(std::isfinite(glm::length(normal)) && std::abs(glm::length(normal) - 1) < .0001f);
		}
		for (std::size_t i = 0; i < result->indices.size(); i += 3) {
			ice::math::vec3 points[3], normals(0);
			for (unsigned j = 0; j < 3; ++j) {
				auto index = result->indices[i + j];
				CHECK(index < result->vertices.size());
				const auto &vertex = result->vertices[index];
				points[j]          = {vertex.position[0], vertex.position[1], vertex.position[2]};
				normals += ice::math::vec3(vertex.normal[0], vertex.normal[1], vertex.normal[2]);
			}
			CHECK(glm::dot(glm::cross(points[1] - points[0], points[2] - points[0]), normals) >
			      1.e-8f);
		}
	}
	primitive_description invalid;
	invalid.radius = std::numeric_limits<float>::infinity();
	CHECK(!make_primitive(invalid));
	invalid          = {};
	invalid.segments = 129;
	CHECK(!make_primitive(invalid));
	primitive_description capsule;
	capsule.kind = primitive_kind::capsule;
	auto result  = make_primitive(capsule);
	CHECK(result);
	float minimum = 0, maximum = 0;
	for (const auto &vertex : result->vertices) {
		minimum = std::min(minimum, vertex.position[1]);
		maximum = std::max(maximum, vertex.position[1]);
	}
	CHECK(minimum == -1 && maximum == 1);
}
