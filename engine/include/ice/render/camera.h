#pragma once

#include "ice/math/ivec2.h"
#include "ice/math/mat4.h"
#include "ice/math/vec2.h"
namespace ice::render {

/*! Provides a basic camera object
 **/
class camera {
public:
	/// Construct inactive state; initialize explicitly before active use.
	camera(void) {};
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	virtual ~camera(void) {};

	/// Advance frame-dependent state on the owning thread; elapsed-time arguments are seconds.
	virtual void update(const float delta_seconds) { (void)delta_seconds; };

	ice::math::mat4 &get_projection(void);
	ice::math::mat4 &get_view_mat(void);

	const ice::math::mat4 &get_projection(void) const;
	const ice::math::mat4 &get_view_mat(void) const;

	virtual void set_camera_pos(const ice::math::vec3 &pos);

	virtual ice::math::vec3 get_camera_pos(void) const;

	ice::math::vec2 world_to_screen(const ice::math::vec3 &world, int width, int height);

	ice::math::vec3 screen_to_world_dir(const ice::math::vec2  &pos,
	                                    const ice::math::ivec2 &size) const;

protected:
	ice::math::mat4 projection_;
	ice::math::mat4 view_mat_;
};

/*! Returns the projection matrix
 *
 *    @returns    icMatrix44&        Reference to projection
 **/
FORCE_INLINE ice::math::mat4 &camera::get_projection(void) { return projection_; }

/*! Returns the view matrix
 *
 *    @returns    icTransform&    Reference to view matrix
 **/
FORCE_INLINE ice::math::mat4 &camera::get_view_mat(void) { return view_mat_; }

/*! Returns the projection matrix
 *
 *    @returns    icMatrix44&        const reference to projection
 **/
FORCE_INLINE const ice::math::mat4 &camera::get_projection(void) const { return projection_; }

/*! Returns the view matrix
 *
 *    @returns    icTransform&        const reference to projection
 *
 **/
FORCE_INLINE const ice::math::mat4 &camera::get_view_mat(void) const { return view_mat_; }

/*! Sets the camera position
 *
 *    @param        pos        The camera position in world space
 **/
FORCE_INLINE void camera::set_camera_pos(const ice::math::vec3 &pos) {
	ice::math::set_position(view_mat_, ice::math::vec3(pos));
}

/*! Gets the current camera position
 *
 *    @returns      const icVector3&    Current camera position
 **/
FORCE_INLINE ice::math::vec3 camera::get_camera_pos(void) const {
	return ice::math::vec3((view_mat_)[3]);
}

} // namespace ice::render
