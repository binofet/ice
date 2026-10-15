
#include "ice/render/max_cam.h"

#include "ice/input/input.h"

#include "ice/math/mat4.h"
namespace ice::render {

max_cam::max_cam(void) {
	r_zoom_ = 100.0f;
	r_xrot_ = 0.0f;
	r_yrot_ = 0.0f;

	r_zoom_min_ = 50.0f;
	r_zoom_max_ = 500.0f;

	(center_) = ice::math::vec3(0.0f, 0.0f, 0.0f);

	enable_input = true;
}

max_cam::~max_cam(void) {}

void max_cam::update(const float delta_seconds) {
	if (enable_input) {
		ice::input::input *input = ice::input::input::instance();

		if (input->is_down(ice::input::mouse_m) || input->is_down(ice::input::mouse_r)) {
			if (input->is_down(ice::input::key_lalt)) {
				// arc rotate
				ice::math::real x = input->get_axis(ice::input::mouse_x) * delta_seconds * 5000.0f *
				                    r_zoom_ / r_zoom_max_;
				ice::math::real y = input->get_axis(ice::input::mouse_y) * delta_seconds * 5000.0f *
				                    r_zoom_ / r_zoom_max_;

				if (r_yrot_ > 0)
					r_yrot_ += y;
				else
					r_yrot_ -= y;

				r_xrot_ -= x;
			} else {
				// pan
				ice::math::real z_amt =
				    (r_zoom_ - r_zoom_min_) + 0.05f * (r_zoom_max_ - r_zoom_min_);
				ice::math::real x =
				    input->get_axis(ice::input::mouse_x) * delta_seconds * 200.0f * z_amt;
				ice::math::real y =
				    input->get_axis(ice::input::mouse_y) * delta_seconds * 200.0f * z_amt;

				center_ += ice::math::transform_direction(view_mat_, ice::math::vec3(-x, -y, 0.0f));
			}
		}

		// handle zoom
		ice::math::real zoom = input->get_axis(ice::input::mouse_z);
		r_zoom_ -= zoom * delta_seconds * 1000.0f;
		r_zoom_ = ice::math::clamp(r_zoom_, r_zoom_min_, r_zoom_max_);
	}

	// trimming
	while (r_yrot_ > IC_PI * 2.0f)
		r_yrot_ -= 2 * IC_PI;
	while (r_yrot_ < 0.0f)
		r_yrot_ += 2 * IC_PI;

	while (r_xrot_ > IC_PI * 2.0f)
		r_xrot_ -= 2 * IC_PI;
	while (r_xrot_ < 0.0f)
		r_xrot_ += 2 * IC_PI;

	ice::math::real theta = r_xrot_;
	ice::math::real phi   = r_yrot_ - IC_PI;
	ice::math::real cx, cy, cz;
	cx = r_zoom_ * ice::math::cos(theta) * ice::math::sin(phi);
	cy = r_zoom_ * ice::math::cos(phi);
	cz = r_zoom_ * ice::math::sin(theta) * ice::math::sin(phi);
	ice::math::vec3 point(cx, cy, cz);
	ice::math::vec3 position = point + center_;

	if (phi < 0.0)
		view_mat_ = ice::math::camera_pose_lh(position, center_, ice::math::vec3(0.0f, -1.0, 0.0f));
	else
		view_mat_ = ice::math::camera_pose_lh(position, center_, ice::math::vec3(0, 1, 0));
}
} // namespace ice::render
