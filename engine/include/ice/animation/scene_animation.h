#pragma once

#include "ice/assets/scene_asset.h"
namespace ice::animation {

/// Local node translation, unit quaternion rotation, and scale used for animation blending.
struct transform {
	/// Local translation in asset units.
	std::array<float, 3> translation{{0, 0, 0}};
	/// Local per-axis scale.
	std::array<float, 3> scale{{1, 1, 1}};
	/// Local unit quaternion in XYZW storage order.
	std::array<float, 4> rotation{{0, 0, 0, 1}};
};
/// Per-instance mutable state. The asset remains immutable and shareable.
struct pose {
	/// Mutable per-node local TRS state; the source asset remains immutable.
	std::vector<transform> transforms; // local TRS, before hierarchy/skin evaluation
	/// Column-major local-to-world transform(s). Column-major local-to-parent transform(s).
	std::vector<ice::math::mat4> local, world;
	/// World-space joint matrices: jointWorld * inverseBind. No mesh transform twice.
	std::vector<std::vector<ice::math::mat4>> palettes;
};
/// Return a single-precision column-major identity transform.
ice::math::mat4 identity_matrix();
/// Compose column-vector transforms as left * right; the right transform is applied first.
ice::math::mat4 multiply_matrices(const ice::math::mat4 &left, const ice::math::mat4 &right);
/// seconds is relative to the clip start; -1 evaluates the authored rest pose.
/// Loop wraps negative times too; non-looping playback clamps to both endpoints.
/// Failure leaves output intact. No parser or graphics context is required.
bool evaluate_pose(const ice::assets::scene_asset &scene, int clip, double seconds, bool loop,
                   pose &output, std::string &diagnostics);
/// Blend local translation/scale linearly and rotation by shortest-path SLERP.
/// Immutable matrix nodes keep their authored matrix. Both inputs must belong to this asset.
/// Weight must be finite and within [0,1]; aliases of output are safe; failure is transactional.
bool blend_poses(const ice::assets::scene_asset &scene, const pose &from, const pose &to,
                 float weight, pose &output, std::string &diagnostics);
/// CPU reference for inspection/bounds/tests; rendering deforms vertices on the GPU.
bool scene_pose_bounds(const ice::assets::scene_asset &scene, const pose &evaluated_pose,
                       float minimum[3], float maximum[3], std::string &diagnostics);

} // namespace ice::animation
