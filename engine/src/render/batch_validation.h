#pragma once
#include "ice/core/expected.h"
#include "ice/render/types.h"
#include <cmath>
#include <cstddef>

namespace ice::render::detail {
template <typename mesh_description>
inline ice::expected<void, diagnostic> validate_mesh_data(const mesh_description &description) {
	static_assert(sizeof(mesh_vertex) == 24 && offsetof(mesh_vertex, normal) == 12);
	static_assert(sizeof(mesh_vertex_uv) == 32 && offsetof(mesh_vertex_uv, normal) == 12 &&
	              offsetof(mesh_vertex_uv, uv) == 24);
	static_assert(sizeof(mesh_vertex_skin) == 64 && offsetof(mesh_vertex_skin, normal) == 12 &&
	              offsetof(mesh_vertex_skin, uv) == 24 &&
	              offsetof(mesh_vertex_skin, joints) == 32 &&
	              offsetof(mesh_vertex_skin, weights) == 48);
	if (description.vertices.empty() || description.vertices.size() > 1048576 ||
	    description.indices.size() < 3 || description.indices.size() > 3145728 ||
	    description.indices.size() % 3)
		return ice::unexpected(
		    diagnostic{"Invalid mesh vertex/index count or triangle-list budget"});
	for (const auto &vertex : description.vertices) {
		for (unsigned axis = 0; axis < 3; ++axis)
			if (!std::isfinite(vertex.position[axis]) || !std::isfinite(vertex.normal[axis]))
				return ice::unexpected(diagnostic{"Mesh position/normal must be finite"});
		if constexpr (requires { vertex.uv; })
			for (float component : vertex.uv)
				if (!std::isfinite(component))
					return ice::unexpected(diagnostic{"Mesh UV coordinates must be finite"});
		if constexpr (requires {
			              vertex.joints;
			              vertex.weights;
		              }) {
			float sum = 0;
			for (unsigned lane = 0; lane < 4; ++lane) {
				const auto joint = vertex.joints[lane], weight = vertex.weights[lane];
				if (!std::isfinite(joint) || joint < 0 || joint > 16777215 ||
				    std::floor(joint) != joint)
					return ice::unexpected(diagnostic{
					    "Skin joints require exact nonnegative integer floats in [0,16777215]"});
				if (!std::isfinite(weight) || weight < 0 || weight > 1)
					return ice::unexpected(diagnostic{"Skin weights must be finite and in [0,1]"});
				sum += weight;
			}
			if (std::abs(sum - 1) > .0001f)
				return ice::unexpected(diagnostic{"Skin weights must sum to one within 0.0001"});
		}
	}
	for (auto index : description.indices)
		if (index >= description.vertices.size())
			return ice::unexpected(diagnostic{"Mesh index is outside the vertex array"});
	return {};
}
inline bool valid_options(const pipeline_options &options) {
	return (options.vertices == vertex_layout::procedural ||
	        options.vertices == vertex_layout::position_normal ||
	        options.vertices == vertex_layout::position_normal_uv ||
	        options.vertices == vertex_layout::position_normal_uv_skin) &&
	       (!options.depth_write || options.depth_test) &&
	       (options.target_format == color_format::rgba8_unorm ||
	        options.target_format == color_format::rgba16_float) &&
	       (options.sampled_bindings <= 15 || options.sampled_bindings == 0xff) &&
	       options.depth_sampled_bindings <= 15 &&
	       (options.sampled_bindings == 0xff
	            ? options.depth_sampled_bindings == 0
	            : (options.depth_sampled_bindings & ~options.sampled_bindings) == 0);
}
inline bool valid_target(const render_target_description &description) {
	return description.width >= 1 && description.height >= 1 && description.width <= 16384 &&
	       description.height <= 16384 &&
	       (description.format == color_format::rgba8_unorm ||
	        description.format == color_format::rgba16_float);
}
template <typename item> auto sampled_at(const item &draw, unsigned slot) {
	return slot == 0 && !draw.sampled_images[0] ? draw.sampled_image : draw.sampled_images[slot];
}
template <typename batch> ice::expected<void, diagnostic> validate_batch_shape(const batch &input) {
	if (input.draws.size() > 131072)
		return ice::unexpected(diagnostic{"Frame exceeds 131072 draws"});
	for (float component : input.clear_color)
		if (!std::isfinite(component) || component < 0 || component > 1)
			return ice::unexpected(diagnostic{"Frame clear color must be finite normalized RGBA"});
	std::size_t bytes = 0;
	for (const auto &item : input.draws) {
		if (!item.pipeline || item.uniforms.size() > max_uniform_bytes ||
		    item.uniforms.size() > 64 * 1024 * 1024 - bytes)
			return ice::unexpected(
			    diagnostic{"Null pipeline or frame uniform byte budget exceeded"});
		if (item.sampled_image && item.sampled_images[0] &&
		    item.sampled_image != item.sampled_images[0])
			return ice::unexpected(diagnostic{"Conflicting sampled slot-zero bindings"});
		bytes += item.uniforms.size();
	}
	return {};
}
} // namespace ice::render::detail
