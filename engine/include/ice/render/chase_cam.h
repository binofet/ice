#pragma once

#include "ice/render/camera.h"
namespace ice::render {

/*! Provides a mass-spring-dampened chase camera
 **/
class chase_cam : public camera {
public:
	chase_cam(void);

	virtual void update(const float delta_seconds);

	void set_camera_pos(const ice::math::vec3 &pos);

	void set_cam_physics(ice::math::real mass, ice::math::real damping, ice::math::real stiffness);

	void set_chase_xform(ice::math::mat4 *trans);
	void set_chase_offset(const ice::math::vec3 &pos_offset);
	void set_look_offset(const ice::math::vec3 &look_offset);

protected:
	ice::math::real r_mass_;      //! Mass of the camera
	ice::math::real r_damping_;   //! Internal spring friction(aprx)
	ice::math::real r_stiffness_; //! Spring stiffness coefficient

	ice::math::vec3 velocity_; //! Current camera velocity

	ice::math::vec3  desired_pos_;        //! Calculated desired position
	ice::math::vec3  look_offset_;        //! Camera look offset
	ice::math::vec3  chase_offset_;       //! Offset from chase object
	ice::math::mat4 *chase_object_trans_; //! Pointer to chase objects xform
};

/*! Sets the cameras position, resets current velocity
 *
 *  @param          pos         Position to warp camera to
 **/
FORCE_INLINE void chase_cam::set_camera_pos(const ice::math::vec3 &pos) {
	ice::math::set_position(view_mat_, ice::math::vec3(pos));
	(velocity_) = ice::math::vec3(0.0f, 0.0f, 0.0f);
}

/*! Sets the physics parameters of the chase camera
 *
 *  @param          mass        Mass of the camera
 *  @param          damping     Internal friction of the spring
 *  @param          stiffness   Spring stiffness
 **/
FORCE_INLINE
void chase_cam::set_cam_physics(ice::math::real mass, ice::math::real damping,
                                ice::math::real stiffness) {
	(velocity_) = ice::math::vec3(0.0f, 0.0f, 0.0f);

	r_mass_      = mass;
	r_damping_   = damping;
	r_stiffness_ = stiffness;
}

/*! Sets the transform this camera is to be attached to
 *
 *  @param          trans      Pointer to chase target transform
 **/
FORCE_INLINE void chase_cam::set_chase_xform(ice::math::mat4 *trans) {
	(velocity_) = ice::math::vec3(0.0f, 0.0f, 0.0f);

	chase_object_trans_ = trans;
	view_mat_           = *chase_object_trans_;
}

/*! Sets the offset the camera should have from the chase object
 *
 *  @param          pos_offset       Positional offset from chase object
 **/
FORCE_INLINE void chase_cam::set_chase_offset(const ice::math::vec3 &pos_offset) {
	(velocity_) = ice::math::vec3(0.0f, 0.0f, 0.0f);

	chase_offset_ = pos_offset;
}

/*! Sets the relative look vector the camera should have from the chase object
 *
 *  @param          look_offset      Look offset from chase object
 **/
FORCE_INLINE void chase_cam::set_look_offset(const ice::math::vec3 &look_offset) {
	(velocity_) = ice::math::vec3(0.0f, 0.0f, 0.0f);

	look_offset_ = look_offset;
}

} // namespace ice::render
