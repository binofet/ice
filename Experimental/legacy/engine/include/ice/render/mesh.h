#pragma once

#include "ice/globals.h"
#include "ice/math/mat4.h"
#include "ice/render/index_buf.h"
#include "ice/render/texture.h"
#include "ice/render/vert_buffer.h"
namespace ice::render {

/*! Renderable mesh with a vertex buffer, index buffer (optional),
 *  a texture (optional), and a rendering technique
 **/
class mesh {
public:
	mesh(void);
	~mesh(void);

	ice::result update(const float delta_time);
	ice::result render(const ice::math::mat4 &transform);

private:
	vert_buffer *vb_;
	index_buf   *ib_;
	texture     *dif_tex_;
	texture     *norm_tex_;
	texture     *spec_tex_;
};

} // namespace ice::render
