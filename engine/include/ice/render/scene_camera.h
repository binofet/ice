#pragma once

#include "ice/render/scene_renderer.h"
namespace ice::render {
/// Right-handed, +Y up, OpenGL clip depth [-1,+1]. Unlike legacy GX SetViewMatrix,
/// the returned view is already world-to-camera and must not be inverted again.
class scene_camera {
public:
	/// Frame the assets world bounds using the right-handed +Y-up viewer camera.
	void fit(const ice::assets::scene_asset &scene);
	/// Adjust orbit yaw/pitch in radians; pitch is clamped away from the pole singularity.
	void orbit(float x, float y);
	/// Adjust orbit distance multiplicatively; positive input moves closer within the configured
	/// limits.
	void zoom(float amount);
	/// Move the orbit center in camera-relative horizontal/vertical directions.
	void pan(float x, float y);
	/// Return right-handed world-to-camera and OpenGL [-1,+1] clip-depth transforms for drawable
	/// pixel dimensions.
	scene_view view(int width, int height) const;

private:
	float center_[3] = {}, radius_ = 1, distance_ = 3, yaw_ = .3f, pitch_ = .12f;
};

} // namespace ice::render
