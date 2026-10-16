#pragma once

#include "ice/core/expected.h"
#include "ice/math/types.h"
#include "ice/render/types.h"

namespace ice::render {
/// Explicit normalized clip-depth convention; both choices use a right-handed camera.
enum class clip_depth { zero_to_one, negative_one_to_one };
/// Camera-to-world rigid pose, with local +X right, +Y up and -Z forward.
struct camera_pose {
	/// World-space eye position in the same units as scene geometry.
	math::vec3 position{0};
	/// Camera-to-world orientation; construction accepts and normalizes any finite nonzero value.
	math::quat orientation{1, 0, 0, 0};
};
/// Symmetric perspective or orthographic projection; viewport orientation belongs to the surface.
enum class projection_kind { perspective, orthographic };
/// Authored projection settings independent of a drawable size or graphics backend.
struct camera_projection {
	/// Projection model selected explicitly by the consumer.
	projection_kind kind = projection_kind::perspective;
	/// Vertical perspective field of view in radians, strictly between zero and pi.
	float vertical_fov = glm::radians(52.f);
	/// Full vertical orthographic span in world units; strictly positive.
	float vertical_extent = 10;
	/// Positive near clipping distance along camera-local -Z.
	float near_plane = .05f;
	/// Finite far clipping distance, strictly greater than near_plane.
	float far_plane = 1000;
};
/// CPU camera transforms and world basis, independent of devices and presentation surfaces.
struct camera_view {
	/// Right-handed world-to-camera transform, with local +Y up and -Z forward.
	math::mat4 view{1};
	/// Camera-to-clip transform using the explicitly selected depth convention.
	math::mat4 projection{1};
	/// Drawable pixel dimensions used to determine aspect ratio.
	int width = 1, height = 1;
	/// World-space eye position.
	math::vec3 eye{0};
	/// Unit world-space camera-right direction.
	math::vec3 right{1, 0, 0};
	/// Unit world-space camera-up direction.
	math::vec3 up{0, 1, 0};
	/// Unit world-space viewing direction.
	math::vec3 forward{0, 0, -1};
	/// Inverse of projection * view, prepared alongside the other factory outputs.
	math::mat4 inverse_view_projection{1};
	/// Depth interval expected by projection and used when unprojecting a picking ray.
	clip_depth depth = clip_depth::zero_to_one;
};
/// Finite picking segment clipped to the camera's near/far planes.
struct camera_ray3d {
	/// World-space point on the near clipping plane, for either projection model.
	math::vec3 origin{0};
	/// Unit world-space direction from near to far; multiply by length for a cast displacement.
	math::vec3 direction{0, 0, -1};
	/// Positive distance from origin to the far clipping plane along direction.
	float length = 0;
};
/// Construct a camera-to-world pose looking from position toward target with the requested up.
/// Reject nonfinite coordinates, coincident position/target, zero up and parallel view/up axes.
ice::expected<camera_pose, diagnostic> look_at_pose(const math::vec3 &position,
                                                    const math::vec3 &target,
                                                    const math::vec3 &up = {0, 1, 0});
/// Validate pose, selected projection parameters and positive drawable dimensions before building
/// transforms. Reject matrices or inverses that cannot be represented by finite float scalars.
/// Reject finite clip ranges that round to an infinite-far perspective projection.
/// The unused projection parameter is ignored; global GLM handedness/depth flags are not used.
ice::expected<camera_view, diagnostic> make_camera_view(const camera_pose       &pose,
                                                        const camera_projection &projection,
                                                        int width, int height,
                                                        clip_depth depth = clip_depth::zero_to_one);
/// Unproject a top-left normalized drawable coordinate, with each component in [0,1].
/// Read view/projection/depth directly so explicitly supplied matrices remain usable; reject
/// nonfinite/singular matrices, invalid dimensions and unrepresentable or degenerate segments.
ice::expected<camera_ray3d, diagnostic> camera_ray(const camera_view &camera,
                                                   const math::vec2  &normalized_top_left);
} // namespace ice::render
