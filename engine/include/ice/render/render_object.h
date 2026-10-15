#pragma once
namespace ice::render {} // namespace ice::render
#include "ice/math/mat4.h"
#include "ice/render/graphics_device.h"
#include "ice/render/index_buf.h"
#include "ice/render/vert_buffer.h"
namespace ice::render {

/*! Provides a simple interface to render objects
 **/
class render_object {
public:
	render_object(graphics_device *device);
	virtual ~render_object(void);

	virtual ice::result cleanup(void);

	ice::result create_sphere(const ice::math::real &radius, const ice::math::color &color,
	                          int subdiv = 10);

	ice::result create_box(const ice::math::vec3 &dimensions, const ice::math::color &color,
	                       int subdiv = 1, class texture *tex = NULL);

	ice::result create_plane(const ice::math::real &length, const ice::math::real &width,
	                         const ice::math::color &color, int numrow = 10, int numcol = 10);

	ice::result create_lines(ice::math::vec3 *verts, const ice::math::color &color, int num_points,
	                         bool list = false);

	virtual ice::result update(const float delta_seconds);
	virtual ice::result render(void);

	/// Borrow the local-to-world transform; mutable access changes subsequent draws.
	ice::math::mat4 &get_transform(void) { return trans_; };

private:
	graphics_device *dev_;

	vert_buffer *vb_;
	index_buf   *ib_;

	ice::math::mat4 trans_;

	class texture *tex_;
};

} // namespace ice::render
