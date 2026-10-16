#pragma once

#include "ice/assets/scene_asset.h"
#include "ice/render/camera.h"
namespace ice::render {
/// Right-handed orbit camera. Consumers select clip depth explicitly; viewport orientation
/// and presentation transforms belong to the backend/surface rather than global GLM flags.
class scene_camera {
public:
	/// Frame the asset's world bounds using a +Y-up orbit camera.
	void fit(const ice::assets::scene_asset &scene);
	/// Adjust orbit yaw/pitch in radians; clamp pitch away from pole singularities.
	void orbit(float x, float y);
	/// Adjust distance multiplicatively; positive input moves closer within configured limits.
	void zoom(float amount);
	/// Move the center in camera-relative horizontal/vertical directions.
	void pan(float x, float y);
	/// Return world-to-camera and camera-to-clip matrices for drawable dimensions and depth range.
	camera_view view(int width, int height, clip_depth depth = clip_depth::zero_to_one) const;

private:
	float center_[3] = {}, radius_ = 1, distance_ = 3, yaw_ = .3f, pitch_ = .12f;
};
} // namespace ice::render
