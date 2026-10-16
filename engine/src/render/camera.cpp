#include "ice/render/camera.h"
#include <cmath>
#include <limits>

namespace ice::render {
namespace {
template <typename vector> bool finite_vector(const vector &value) {
	for (typename vector::length_type component = 0; component < value.length(); ++component)
		if (!std::isfinite(value[component]))
			return false;
	return true;
}
template <typename matrix> bool finite_matrix(const matrix &value) {
	for (typename matrix::length_type column = 0; column < value.length(); ++column)
		if (!finite_vector(value[column]))
			return false;
	return true;
}
template <typename vector> bool float_vector(const vector &value) {
	if (!finite_vector(value))
		return false;
	for (typename vector::length_type component = 0; component < value.length(); ++component)
		if (std::abs(value[component]) > std::numeric_limits<float>::max())
			return false;
	return true;
}
template <typename matrix> bool float_matrix(const matrix &value) {
	for (typename matrix::length_type column = 0; column < value.length(); ++column)
		if (!float_vector(value[column]))
			return false;
	return true;
}
bool valid(clip_depth depth) {
	return depth == clip_depth::zero_to_one || depth == clip_depth::negative_one_to_one;
}
ice::expected<glm::dmat4, diagnostic> inverse(const math::mat4 &view,
                                              const math::mat4 &projection) {
	if (!finite_matrix(view) || !finite_matrix(projection))
		return ice::unexpected(diagnostic{"Camera matrices must be finite"});
	// Invert the public float matrices in double precision. In particular, the far-plane
	// homogeneous coordinate can be much smaller than the other components of the inverse.
	const auto combined    = glm::dmat4(projection) * glm::dmat4(view);
	const auto determinant = glm::determinant(combined);
	if (!std::isfinite(determinant) || determinant == 0)
		return ice::unexpected(diagnostic{"Camera view-projection matrix is singular"});
	const auto result = glm::inverse(combined);
	if (!float_matrix(result))
		return ice::unexpected(diagnostic{"Camera inverse exceeds finite float storage"});
	return result;
}
} // namespace

ice::expected<camera_pose, diagnostic>
look_at_pose(const math::vec3 &position, const math::vec3 &target, const math::vec3 &up) {
	if (!finite_vector(position) || !finite_vector(target) || !finite_vector(up))
		return ice::unexpected(diagnostic{"Camera look-at coordinates must be finite"});
	// Widen before subtraction/normalization so valid large and subnormal float inputs do
	// not overflow or become a zero direction during intermediate calculations.
	const auto delta = glm::dvec3(target) - glm::dvec3(position);
	if (glm::dot(delta, delta) == 0 || glm::dot(glm::dvec3(up), glm::dvec3(up)) == 0)
		return ice::unexpected(diagnostic{"Camera look-at direction and up must be nonzero"});
	const auto forward = glm::normalize(delta);
	const auto cross   = glm::cross(forward, glm::normalize(glm::dvec3(up)));
	if (glm::dot(cross, cross) <= 1.0e-24)
		return ice::unexpected(diagnostic{"Camera look-at direction and up are parallel"});
	const auto right = glm::normalize(cross);
	const auto orientation =
	    glm::quat_cast(glm::dmat3(right, glm::cross(right, forward), -forward));
	return camera_pose{position, math::quat(glm::normalize(orientation))};
}

ice::expected<camera_view, diagnostic> make_camera_view(const camera_pose       &pose,
                                                        const camera_projection &projection,
                                                        int width, int height, clip_depth depth) {
	if (width < 1 || height < 1 || !valid(depth))
		return ice::unexpected(
		    diagnostic{"Camera requires positive dimensions and valid clip depth"});
	if (!finite_vector(pose.position) || !std::isfinite(pose.orientation.w) ||
	    !std::isfinite(pose.orientation.x) || !std::isfinite(pose.orientation.y) ||
	    !std::isfinite(pose.orientation.z))
		return ice::unexpected(diagnostic{"Camera pose must be finite"});
	const glm::dquat orientation(pose.orientation);
	if (glm::dot(orientation, orientation) == 0)
		return ice::unexpected(diagnostic{"Camera orientation must be nonzero"});
	if (!std::isfinite(projection.near_plane) || !std::isfinite(projection.far_plane) ||
	    projection.near_plane <= 0 || projection.far_plane <= projection.near_plane)
		return ice::unexpected(diagnostic{"Camera clipping distances require 0 < near < far"});
	const double aspect = double(width) / double(height);
	glm::dmat4   projected;
	if (projection.kind == projection_kind::perspective) {
		if (!std::isfinite(projection.vertical_fov) || projection.vertical_fov <= 0 ||
		    double(projection.vertical_fov) >= glm::pi<double>())
			return ice::unexpected(diagnostic{"Camera vertical field of view must be in (0,pi)"});
		projected =
		    depth == clip_depth::zero_to_one
		        ? glm::perspectiveRH_ZO(double(projection.vertical_fov), aspect,
		                                double(projection.near_plane), double(projection.far_plane))
		        : glm::perspectiveRH_NO(double(projection.vertical_fov), aspect,
		                                double(projection.near_plane),
		                                double(projection.far_plane));
	} else if (projection.kind == projection_kind::orthographic) {
		if (!std::isfinite(projection.vertical_extent) || projection.vertical_extent <= 0)
			return ice::unexpected(
			    diagnostic{"Camera orthographic extent must be positive and finite"});
		const double half_height = double(projection.vertical_extent) * .5;
		const double half_width  = half_height * aspect;
		projected =
		    depth == clip_depth::zero_to_one
		        ? glm::orthoRH_ZO(-half_width, half_width, -half_height, half_height,
		                          double(projection.near_plane), double(projection.far_plane))
		        : glm::orthoRH_NO(-half_width, half_width, -half_height, half_height,
		                          double(projection.near_plane), double(projection.far_plane));
	} else
		return ice::unexpected(diagnostic{"Unknown camera projection model"});
	const auto rotation = glm::mat3_cast(glm::normalize(orientation));
	glm::dmat4 world(rotation);
	world[3]          = glm::dvec4(pose.position, 1);
	const auto viewed = glm::inverse(world);
	if (!float_matrix(viewed) || !float_matrix(projected))
		return ice::unexpected(diagnostic{"Camera transforms exceed finite float storage"});
	camera_view result;
	result.view       = math::mat4(viewed);
	result.projection = math::mat4(projected);
	result.width      = width;
	result.height     = height;
	result.eye        = pose.position;
	result.right      = math::vec3(rotation[0]);
	result.up         = math::vec3(rotation[1]);
	result.forward    = -math::vec3(rotation[2]);
	result.depth      = depth;
	auto inverted     = inverse(result.view, result.projection);
	if (!inverted)
		return ice::unexpected(inverted.error());
	// An extreme far/near ratio can round a finite perspective matrix into an infinite-far
	// projection. Do not publish that different clipping policy under finite authored settings.
	const auto far = *inverted * glm::dvec4(0, 0, 1, 1);
	if (!finite_vector(far) || far.w == 0)
		return ice::unexpected(
		    diagnostic{"Camera clip range loses its finite far plane in float storage"});
	result.inverse_view_projection = math::mat4(*inverted);
	return result;
}

ice::expected<camera_ray3d, diagnostic> camera_ray(const camera_view &camera,
                                                   const math::vec2  &normalized_top_left) {
	if (camera.width < 1 || camera.height < 1 || !valid(camera.depth) ||
	    !finite_vector(normalized_top_left) ||
	    glm::any(glm::lessThan(normalized_top_left, math::vec2(0))) ||
	    glm::any(glm::greaterThan(normalized_top_left, math::vec2(1))))
		return ice::unexpected(
		    diagnostic{"Camera ray requires valid dimensions, depth and coordinates in [0,1]"});
	auto inverted = inverse(camera.view, camera.projection);
	if (!inverted)
		return ice::unexpected(inverted.error());
	const double x = double(normalized_top_left.x) * 2 - 1;
	const double y = 1 - double(normalized_top_left.y) * 2;
	const auto   near =
	    *inverted * glm::dvec4(x, y, camera.depth == clip_depth::zero_to_one ? 0 : -1, 1);
	const auto far = *inverted * glm::dvec4(x, y, 1, 1);
	if (!finite_vector(near) || !finite_vector(far) || near.w == 0 || far.w == 0)
		return ice::unexpected(diagnostic{"Camera ray endpoints cannot be unprojected"});
	const auto   origin   = glm::dvec3(near) / near.w;
	const auto   end      = glm::dvec3(far) / far.w;
	const auto   delta    = end - origin;
	const double distance = glm::length(delta);
	if (!float_vector(origin) || !float_vector(end) || !std::isfinite(distance) ||
	    distance < std::numeric_limits<float>::denorm_min() ||
	    distance > std::numeric_limits<float>::max())
		return ice::unexpected(
		    diagnostic{"Camera ray exceeds finite float storage or is degenerate"});
	return camera_ray3d{math::vec3(origin), math::vec3(delta / distance), float(distance)};
}
} // namespace ice::render
