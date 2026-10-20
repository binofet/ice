#pragma once

#include "ice/core/expected.h"
#include "ice/math/types.h"
#include <cstdint>
#include <string>

namespace ice::motion {
/// A rigid transform; no scale, projection, velocity, or transform authority is implied.
struct rigid_pose {
	/// Translation in the host-declared parent space, using that space's distance unit.
	math::vec3 position{0};
	/// Local-to-parent rotation. Validated results have unit length and a canonical sign.
	math::quat orientation{1, 0, 0, 0};
};

/// Host-assigned identities, never inferred from a pose or an elapsed-time float.
struct sample_tag {
	/// Nonzero identity of the coordinate space, including its lifetime.
	std::uint64_t space = 0;
	/// Nonzero identity of the sampling clock, including its lifetime.
	std::uint64_t clock = 0;
	/// Exact sample on that clock; zero is a valid initial sample.
	std::uint64_t frame                                = 0;
	bool          operator==(const sample_tag &) const = default;
};

/// A staged candidate value. Publishing it or applying a physics target is a host decision.
struct pose_sample {
	/// Candidate rigid transform in the coordinate space identified by tag.
	rigid_pose pose;
	/// Host-assigned coordinate-space, clock and exact-frame identity of this candidate.
	sample_tag tag;
};

/// Rejection category for pose validation, tagged blending or geometric transform operations.
enum class error_code {
	invalid_position,
	invalid_orientation,
	invalid_tag,
	invalid_weight,
	sample_mismatch,
	overflow
};
/// Owned portable failure detail; no invalid pose or fabricated sample accompanies it.
struct diagnostic {
	/// Machine-readable reason the requested pose operation could not produce a valid result.
	error_code code;
	/// Human-readable explanation owned independently of input pose storage.
	std::string message;
};

/// Accept finite float positions and any finite nonzero quaternion; normalize in double.
/// Choose the sign whose first nonzero (w,x,y,z) component is positive; erase signed zeros.
/// Unit length is subject to float rounding. This does not validate a physics world's bounds.
ice::expected<rigid_pose, diagnostic> validate_pose(const rigid_pose &pose);
/// Validate the pose and require nonzero space and clock identities.
ice::expected<pose_sample, diagnostic> validate_sample(const pose_sample &sample);

/// Blend alternative absolute goals with linear position and shortest-arc quaternion slerp.
/// Require identical valid space/clock/frame tags and finite weight in [0,1], even at endpoints.
/// Both inputs are validated; endpoint results contain their canonical input pose exactly.
/// This never resamples clocks, converts spaces, blends constraints, or applies a transform.
ice::expected<pose_sample, diagnostic> blend_samples(const pose_sample &from, const pose_sample &to,
                                                     float weight);

/// parent * local_offset: rotate the local translation, then add the parent translation.
/// Raw geometry helpers require the caller to establish the coordinate relationship.
ice::expected<rigid_pose, diagnostic> compose_pose(const rigid_pose &parent,
                                                   const rigid_pose &local_offset);
/// Compose a timeless local offset and preserve the parent's space/clock/frame tag.
ice::expected<pose_sample, diagnostic> compose_sample(const pose_sample &parent,
                                                      const rigid_pose  &local_offset);
/// Reverse a geometric transform; no sample or coordinate-space identity is fabricated.
ice::expected<rigid_pose, diagnostic> inverse_pose(const rigid_pose &pose);
/// Express world in parent's local frame; both inputs must share a coordinate space.
ice::expected<rigid_pose, diagnostic> relative_pose(const rigid_pose &parent,
                                                    const rigid_pose &world);
} // namespace ice::motion
