#include "ice/motion/pose.h"
#include <cmath>
#include <limits>

namespace ice::motion {
namespace {
struct checked_pose {
	glm::dvec3 position;
	glm::dquat orientation;
};

template <typename quaternion> quaternion canonical_sign(quaternion result) {
	const auto components = {result.w, result.x, result.y, result.z};
	for (auto component : components) {
		if (component == 0)
			continue;
		if (component < 0)
			result = -result;
		break;
	}
	for (int i = 0; i < 4; ++i)
		if (result[i] == 0)
			result[i] = 0;
	return result;
}

ice::expected<checked_pose, diagnostic> checked(const rigid_pose &pose) {
	for (int i = 0; i < 3; ++i)
		if (!std::isfinite(pose.position[i]))
			return ice::unexpected(
			    diagnostic{error_code::invalid_position, "Pose position must be finite"});
	for (int i = 0; i < 4; ++i)
		if (!std::isfinite(pose.orientation[i]))
			return ice::unexpected(
			    diagnostic{error_code::invalid_orientation, "Pose orientation must be finite"});
	// Widen first: every finite float quaternion, including subnormals and FLT_MAX,
	// has a representable squared norm in double precision.
	const glm::dquat orientation(pose.orientation);
	if (glm::dot(orientation, orientation) == 0)
		return ice::unexpected(
		    diagnostic{error_code::invalid_orientation, "Pose orientation must be nonzero"});
	return checked_pose{glm::dvec3(pose.position), canonical_sign(glm::normalize(orientation))};
}

math::quat canonical_orientation(const glm::dquat &orientation) {
	// Decide after float conversion: a leading double component can round to zero.
	return canonical_sign(math::quat(glm::normalize(orientation)));
}

ice::expected<rigid_pose, diagnostic> stored(const glm::dvec3 &position,
                                             const glm::dquat &orientation) {
	for (int i = 0; i < 3; ++i)
		if (!std::isfinite(position[i]) ||
		    std::abs(position[i]) > std::numeric_limits<float>::max())
			return ice::unexpected(
			    diagnostic{error_code::overflow, "Pose result exceeds finite float storage"});
	rigid_pose result{math::vec3(position), canonical_orientation(orientation)};
	for (int i = 0; i < 3; ++i)
		if (result.position[i] == 0)
			result.position[i] = 0.f;
	return result;
}

bool valid_tag(const sample_tag &tag) { return tag.space != 0 && tag.clock != 0; }
} // namespace

ice::expected<rigid_pose, diagnostic> validate_pose(const rigid_pose &pose) {
	const auto value = checked(pose);
	if (!value)
		return ice::unexpected(value.error());
	return stored(value->position, value->orientation);
}

ice::expected<pose_sample, diagnostic> validate_sample(const pose_sample &sample) {
	if (!valid_tag(sample.tag))
		return ice::unexpected(
		    diagnostic{error_code::invalid_tag, "Pose sample requires nonzero space and clock"});
	const auto pose = validate_pose(sample.pose);
	if (!pose)
		return ice::unexpected(pose.error());
	return pose_sample{*pose, sample.tag};
}

ice::expected<pose_sample, diagnostic> blend_samples(const pose_sample &from, const pose_sample &to,
                                                     float weight) {
	if (!std::isfinite(weight) || weight < 0 || weight > 1)
		return ice::unexpected(
		    diagnostic{error_code::invalid_weight, "Pose blend weight must be finite in [0,1]"});
	if (!valid_tag(from.tag) || !valid_tag(to.tag))
		return ice::unexpected(
		    diagnostic{error_code::invalid_tag, "Pose blend requires nonzero space and clock"});
	if (from.tag != to.tag)
		return ice::unexpected(diagnostic{error_code::sample_mismatch,
		                                  "Pose blend space, clock, and frame must match"});
	const auto first = checked(from.pose);
	if (!first)
		return ice::unexpected(first.error());
	const auto second = checked(to.pose);
	if (!second)
		return ice::unexpected(second.error());
	// Both inputs and tags are checked before endpoint selection.
	if (weight == 0)
		return validate_sample(from);
	if (weight == 1)
		return validate_sample(to);
	auto orientation = second->orientation;
	if (glm::dot(first->orientation, orientation) < 0)
		orientation = -orientation;
	const double t = weight;
	// Convex position interpolation avoids a float subtraction overflow for distant goals.
	const auto pose = stored((1 - t) * first->position + t * second->position,
	                         glm::slerp(first->orientation, orientation, t));
	if (!pose)
		return ice::unexpected(pose.error());
	return pose_sample{*pose, from.tag};
}

ice::expected<rigid_pose, diagnostic> compose_pose(const rigid_pose &parent,
                                                   const rigid_pose &local_offset) {
	const auto first = checked(parent);
	if (!first)
		return ice::unexpected(first.error());
	const auto second = checked(local_offset);
	if (!second)
		return ice::unexpected(second.error());
	return stored(first->position + first->orientation * second->position,
	              first->orientation * second->orientation);
}

ice::expected<pose_sample, diagnostic> compose_sample(const pose_sample &parent,
                                                      const rigid_pose  &local_offset) {
	if (!valid_tag(parent.tag))
		return ice::unexpected(
		    diagnostic{error_code::invalid_tag, "Pose sample requires nonzero space and clock"});
	const auto pose = compose_pose(parent.pose, local_offset);
	if (!pose)
		return ice::unexpected(pose.error());
	return pose_sample{*pose, parent.tag};
}

ice::expected<rigid_pose, diagnostic> inverse_pose(const rigid_pose &pose) {
	const auto value = checked(pose);
	if (!value)
		return ice::unexpected(value.error());
	const auto orientation = glm::conjugate(value->orientation);
	return stored(orientation * -value->position, orientation);
}

ice::expected<rigid_pose, diagnostic> relative_pose(const rigid_pose &parent,
                                                    const rigid_pose &world) {
	const auto first = checked(parent);
	if (!first)
		return ice::unexpected(first.error());
	const auto second = checked(world);
	if (!second)
		return ice::unexpected(second.error());
	const auto orientation = glm::conjugate(first->orientation);
	// Subtract before rotating: an unrepresentable intermediate inverse translation
	// must not reject an otherwise representable local offset.
	return stored(orientation * (second->position - first->position),
	              orientation * second->orientation);
}
} // namespace ice::motion
