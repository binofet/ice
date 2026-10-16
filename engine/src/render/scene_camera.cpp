#include "ice/render/scene_camera.h"
#include <algorithm>
#include <cmath>
namespace ice::render {
void scene_camera::fit(const ice::assets::scene_asset &scene) {
	float sum = 0;
	for (int k = 0; k < 3; k++) {
		center_[k] = (scene.bounds_min[k] + scene.bounds_max[k]) * .5f;
		float d    = (scene.bounds_max[k] - scene.bounds_min[k]) * .5f;
		sum += d * d;
	}
	radius_   = std::max(.001f, std::sqrt(sum));
	distance_ = radius_ * 2.9f;
	yaw_      = .3f;
	pitch_    = .12f;
}
void scene_camera::orbit(float x, float y) {
	yaw_ += x;
	pitch_ = std::max(-1.5f, std::min(1.5f, pitch_ + y));
}
void scene_camera::zoom(float amount) {
	distance_ = std::max(radius_ * .15f, std::min(radius_ * 100.f, distance_ * std::exp(-amount)));
}
void scene_camera::pan(float x, float y) {
	float right[3] = {std::cos(yaw_), 0, -std::sin(yaw_)};
	for (int k = 0; k < 3; k++)
		center_[k] += right[k] * x * distance_;
	center_[1] += y * distance_;
}
camera_view scene_camera::view(int width, int height, clip_depth depth) const {
	camera_view view;
	view.width   = width;
	view.height  = height;
	float aspect = float(std::max(1, width)) / std::max(1, height),
	      d      = distance_ * std::max(1.f, 1.f / aspect);
	float eye[3] = {center_[0] + d * std::cos(pitch_) * std::sin(yaw_),
	                center_[1] + d * std::sin(pitch_),
	                center_[2] + d * std::cos(pitch_) * std::cos(yaw_)};
	view.view =
	    glm::lookAtRH(glm::make_vec3(eye), glm::make_vec3(center_), ice::math::vec3(0, 1, 0));
	float near_plane = std::max(radius_ * .001f, d - radius_ * 1.5f), far_plane = d + radius_ * 4.f;
	view.projection = depth == clip_depth::zero_to_one
	                      ? glm::perspectiveRH_ZO(3.14159265f / 4, aspect, near_plane, far_plane)
	                      : glm::perspectiveRH_NO(3.14159265f / 4, aspect, near_plane, far_plane);
	// Keep the orbit helper's established float transforms while exposing the same basis
	// and picking metadata as an explicitly positioned camera.
	view.eye                     = glm::make_vec3(eye);
	const auto world_rotation    = glm::transpose(ice::math::mat3(view.view));
	view.right                   = world_rotation[0];
	view.up                      = world_rotation[1];
	view.forward                 = -world_rotation[2];
	view.inverse_view_projection = ice::math::mat4(
	    glm::inverse(ice::math::dmat4(view.projection) * ice::math::dmat4(view.view)));
	view.depth = depth;
	return view;
}

} // namespace ice::render
