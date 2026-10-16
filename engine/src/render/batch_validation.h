#pragma once
#include "ice/core/expected.h"
#include "ice/render/types.h"
#include <cmath>
#include <cstddef>

namespace ice::render::detail {
inline ice::expected<void, diagnostic> validate_mesh_data(const mesh_data &description) {
	static_assert(sizeof(mesh_vertex) == 24 && offsetof(mesh_vertex, normal) == 12);
	if (description.vertices.empty() || description.vertices.size() > 1048576 ||
	    description.indices.size() < 3 || description.indices.size() > 3145728 ||
	    description.indices.size() % 3)
		return ice::unexpected(
		    diagnostic{"Invalid mesh vertex/index count or triangle-list budget"});
	for (const auto &vertex : description.vertices)
		for (unsigned axis = 0; axis < 3; ++axis)
			if (!std::isfinite(vertex.position[axis]) || !std::isfinite(vertex.normal[axis]))
				return ice::unexpected(diagnostic{"Mesh position/normal must be finite"});
	for (auto index : description.indices)
		if (index >= description.vertices.size())
			return ice::unexpected(diagnostic{"Mesh index is outside the vertex array"});
	return {};
}
inline bool valid_options(const pipeline_options &options) {
	return (options.vertices == vertex_layout::procedural ||
	        options.vertices == vertex_layout::position_normal) &&
	       (!options.depth_write || options.depth_test);
}
template <typename batch> ice::expected<void, diagnostic> validate_batch_shape(const batch &input) {
	if (input.draws.size() > 131072)
		return ice::unexpected(diagnostic{"Frame exceeds 131072 draws"});
	for (float component : input.clear_color)
		if (!std::isfinite(component) || component < 0 || component > 1)
			return ice::unexpected(diagnostic{"Frame clear color must be finite normalized RGBA"});
	std::size_t bytes = 0;
	for (const auto &item : input.draws) {
		if (!item.pipeline || item.uniforms.size() > 4096 ||
		    item.uniforms.size() > 64 * 1024 * 1024 - bytes)
			return ice::unexpected(
			    diagnostic{"Null pipeline or frame uniform byte budget exceeded"});
		bytes += item.uniforms.size();
	}
	return {};
}
} // namespace ice::render::detail
