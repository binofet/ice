#include "ice/assets/primitive_mesh.h"
#include <cmath>
#include <numbers>

namespace ice::assets {
namespace {
using mesh = render::mesh_data;
void vertex(mesh &result, const math::vec3 &position, const math::vec3 &normal) {
	result.vertices.push_back(
	    {{position.x, position.y, position.z}, {normal.x, normal.y, normal.z}});
}
void triangle(mesh &result, math::vec3 a, math::vec3 b, math::vec3 c) {
	auto normal = glm::normalize(glm::cross(b - a, c - a));
	auto index  = std::uint32_t(result.vertices.size());
	vertex(result, a, normal);
	vertex(result, b, normal);
	vertex(result, c, normal);
	result.indices.insert(result.indices.end(), {index, index + 1, index + 2});
}
void face(mesh &result, math::vec3 center, math::vec3 u, math::vec3 v) {
	auto normal = glm::normalize(glm::cross(u, v));
	auto index  = std::uint32_t(result.vertices.size());
	vertex(result, center - u - v, normal);
	vertex(result, center + u - v, normal);
	vertex(result, center + u + v, normal);
	vertex(result, center - u + v, normal);
	result.indices.insert(result.indices.end(),
	                      {index, index + 1, index + 2, index, index + 2, index + 3});
}
struct ring {
	float radius, y, normal_radius, normal_y;
};
mesh revolved(const primitive_description &recipe) {
	std::vector<ring> profile;
	constexpr float   pi = std::numbers::pi_v<float>;
	if (recipe.kind == primitive_kind::sphere) {
		for (std::uint32_t i = 0; i <= recipe.rings; ++i) {
			float angle  = pi * float(i) / float(recipe.rings);
			float radial = i == 0 || i == recipe.rings ? 0 : std::sin(angle);
			profile.push_back(
			    {recipe.radius * radial, recipe.radius * std::cos(angle), radial, std::cos(angle)});
		}
	} else {
		for (std::uint32_t i = 0; i <= recipe.rings; ++i) {
			float angle  = pi * .5f * float(i) / float(recipe.rings);
			float radial = i == 0 ? 0 : std::sin(angle);
			profile.push_back({recipe.radius * radial,
			                   recipe.half_height + recipe.radius * std::cos(angle), radial,
			                   std::cos(angle)});
		}
		// Separate equator rings give the cylinder exactly radial normals.
		for (std::uint32_t i = 0; i <= recipe.rings; ++i) {
			float angle  = pi * .5f + pi * .5f * float(i) / float(recipe.rings);
			float radial = i == recipe.rings ? 0 : std::sin(angle);
			profile.push_back({recipe.radius * radial,
			                   -recipe.half_height + recipe.radius * std::cos(angle), radial,
			                   std::cos(angle)});
		}
	}
	mesh result;
	for (const auto &row : profile)
		for (std::uint32_t j = 0; j <= recipe.segments; ++j) {
			float angle = 2 * pi * float(j) / float(recipe.segments);
			float x = std::cos(angle), z = std::sin(angle);
			vertex(result, {row.radius * x, row.y, row.radius * z},
			       {row.normal_radius * x, row.normal_y, row.normal_radius * z});
		}
	auto stride = recipe.segments + 1;
	for (std::uint32_t i = 0; i + 1 < profile.size(); ++i)
		for (std::uint32_t j = 0; j < recipe.segments; ++j) {
			auto a = i * stride + j, b = a + stride;
			if (profile[i].radius > 0)
				result.indices.insert(result.indices.end(), {a, a + 1, b});
			if (profile[i + 1].radius > 0)
				result.indices.insert(result.indices.end(), {a + 1, b + 1, b});
		}
	return result;
}
} // namespace
ice::expected<render::mesh_data, render::diagnostic>
make_primitive(const primitive_description &description) {
	auto finite = [](float number) {
		return std::isfinite(number) && number >= .001f && number <= 10000;
	};
	if (!finite(description.half_extent.x) || !finite(description.half_extent.y) ||
	    !finite(description.half_extent.z) || !finite(description.radius) ||
	    !std::isfinite(description.half_height) || description.half_height < 0 ||
	    description.half_height > 10000 || description.segments < 3 || description.segments > 128 ||
	    description.rings < 2 || description.rings > 128)
		return ice::unexpected(
		    render::diagnostic{"Invalid primitive dimensions or subdivision budget"});
	if (description.kind == primitive_kind::sphere || description.kind == primitive_kind::capsule) {
		auto recipe = description;
		if (recipe.kind == primitive_kind::capsule && recipe.half_height == 0)
			recipe.kind = primitive_kind::sphere;
		return revolved(recipe);
	}
	mesh result;
	auto size = description.half_extent;
	if (description.kind == primitive_kind::box) {
		face(result, {size.x, 0, 0}, {0, 0, -size.z}, {0, size.y, 0});
		face(result, {-size.x, 0, 0}, {0, 0, size.z}, {0, size.y, 0});
		face(result, {0, size.y, 0}, {size.x, 0, 0}, {0, 0, -size.z});
		face(result, {0, -size.y, 0}, {size.x, 0, 0}, {0, 0, size.z});
		face(result, {0, 0, size.z}, {size.x, 0, 0}, {0, size.y, 0});
		face(result, {0, 0, -size.z}, {-size.x, 0, 0}, {0, size.y, 0});
	} else if (description.kind == primitive_kind::pyramid) {
		math::vec3 top(0, size.y, 0), a(-size.x, -size.y, size.z), b(size.x, -size.y, size.z),
		    c(size.x, -size.y, -size.z), d(-size.x, -size.y, -size.z);
		triangle(result, a, b, top);
		triangle(result, b, c, top);
		triangle(result, c, d, top);
		triangle(result, d, a, top);
		face(result, {0, -size.y, 0}, {size.x, 0, 0}, {0, 0, size.z});
	} else if (description.kind == primitive_kind::plane)
		face(result, {0, 0, 0}, {size.x, 0, 0}, {0, 0, -size.z});
	else if (description.kind == primitive_kind::triangle) {
		math::vec3 a(-size.x, -size.y, 0), b(size.x, -size.y, 0), c(0, size.y, 0);
		triangle(result, a, b, c);
		triangle(result, c, b, a);
	} else
		return ice::unexpected(render::diagnostic{"Unknown primitive kind"});
	return result;
}
} // namespace ice::assets
