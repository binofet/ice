
#include "ice/render/chase_cam.h"
namespace ice::render {

/*! default c'tor
 **/
chase_cam::chase_cam(void) : r_mass_(5), r_damping_(600), r_stiffness_(100) {
	(velocity_) = ice::math::vec3(0.0f, 0.0f, 0.0f);

	(desired_pos_)      = ice::math::vec3(0.0f, 0.0f, 0.0f);
	(look_offset_)      = ice::math::vec3(0.0f, 0.2f, 0.0f);
	(chase_offset_)     = ice::math::vec3(0.0f, 0.0f, -2.0f);
	chase_object_trans_ = NULL;
}

/*! Updates the chase camera using a dampened mass-spring model
 *
 *  @param          delta_seconds      Elapsed time since last update (seconds)
 **/
void chase_cam::update(const float delta_seconds) {
	if (!chase_object_trans_)
		return;

	ice::math::vec3 chase_pos = ice::math::vec3(((*chase_object_trans_))[3]);

	ice::math::vec3 chase_up = ((*chase_object_trans_))[1];

	ice::math::vec3 xformed_offset =
	    ice::math::transform_direction((*chase_object_trans_), ice::math::vec3(chase_offset_));
	ice::math::vec3 xformed_look =
	    ice::math::transform_direction((*chase_object_trans_), ice::math::vec3(look_offset_));

	desired_pos_ = chase_pos + xformed_offset;

	// calc spring force
	ice::math::vec3 cur_pos = ice::math::vec3((view_mat_)[3]);
	ice::math::vec3 stretch = cur_pos - desired_pos_;
	ice::math::vec3 force   = -(r_stiffness_ * stretch) - (r_damping_ * velocity_);

	// acceleration
	ice::math::vec3 accel = force / r_mass_;
	velocity_ += accel * delta_seconds;

	// update position
	ice::math::vec3 calc_pos = cur_pos + velocity_;
	calc_pos += xformed_look;

	ice::math::set_position(view_mat_, ice::math::vec3(calc_pos));
	view_mat_ = ice::math::camera_pose_lh(calc_pos, chase_pos + xformed_look, chase_up);
}

} // namespace ice::render
