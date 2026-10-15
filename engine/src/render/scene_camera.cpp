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
scene_view scene_camera::view(int width, int height) const {
	scene_view view;
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
	view.projection = glm::perspectiveRH_NO(3.14159265f / 4, aspect, near_plane, far_plane);
	return view;
}

} // namespace ice::render
