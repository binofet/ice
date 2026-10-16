#pragma once
#include "ice/render/camera.h"
#include <array>

namespace ice::examples::xr {
/// Native eye angles in radians, ordered left/right/down/up; no symmetric-FOV approximation.
struct eye_fov {
	float left = 0, right = 0, down = 0, up = 0;
};
/// Build a complete zero-to-one, right-handed camera from the native reference-space eye pose.
/// reference_from_world includes the retained room anchor and optional synthetic floor offset.
/// Reject nonfinite, singular, nonrigid poses and invalid extents/FOV/clip planes.
ice::expected<ice::render::camera_view, ice::render::diagnostic>
make_eye_camera(const ice::math::mat4 &reference_from_eye,
                const ice::math::mat4 &reference_from_world, eye_fov fov, int width, int height,
                float near_plane = .05f, float far_plane = 1000.f);
} // namespace ice::examples::xr
