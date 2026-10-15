#pragma once

#include "ice/render/camera.h"
namespace ice::render {

/*! Provides a basic camera object
 **/
class max_cam : public camera {
public:
	/// Construct inactive state; initialize explicitly before active use.
	max_cam(void);
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	virtual ~max_cam(void);

	/// Advance frame-dependent state on the owning thread; elapsed-time arguments are seconds.
	virtual void update(const float delta_seconds);

	/// Adjust orbit distance multiplicatively; positive input moves closer within the configured
	/// limits.
	ice::math::real &zoom(void) { return r_zoom_; };
	/// Borrow/configure the minimum orbit distance in world units.
	ice::math::real &zoom_min(void) { return r_zoom_min_; };
	/// Borrow/configure the maximum orbit distance in world units.
	ice::math::real &zoom_max(void) { return r_zoom_max_; };

	/// Borrow/configure the orbit-camera X rotation in radians.
	ice::math::real &xrot(void) { return r_xrot_; };
	/// Borrow/configure the orbit-camera Y rotation in radians.
	ice::math::real &yrot(void) { return r_yrot_; };

	/// Borrow the orbit-camera target point in world coordinates.
	ice::math::vec3 &center(void) { return center_; };

	/// Whether this widget currently consumes input.
	bool enable_input;

private:
	ice::math::real r_zoom_;
	ice::math::real r_xrot_;
	ice::math::real r_yrot_;

	ice::math::real r_zoom_min_;
	ice::math::real r_zoom_max_;

	ice::math::vec3 center_;
};

} // namespace ice::render
