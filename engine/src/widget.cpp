
#include "ice/widget.h"

#include "ice/assets/content_loader.h"
#include "ice/input/input.h"
#include "ice/render/graphics_device.h"
#include "ice/render/vert_buffer.h"
namespace ice {

widget::widget(ice::assets::content_loader *content) {
	content_ = content;

	next_ = NULL;
	vb_   = NULL;
	tex1_ = NULL;
	tex2_ = NULL;

	(pos_)  = ice::math::vec2(0.0f, 0.0f);
	(size_) = ice::math::vec2(0.0f, 0.0f);
	color_.set(0, 0, 0, 0);
	high_color_.set(0, 0, 0, 0);
	rotation_ = 0.0f;

	tab_stop_ = 0;

	highlight_on_      = false;
	update_buffer_     = true;
	highlight_enabled_ = true;
	mouse_over_        = false;
	sticky             = false;
}

/*! Processes input for the widget.
 *      Default processing includes mouse-over highlighting
 *
 *  @param      input          Pointer to current input object
 *  @returns    ICRESULT        Success/failure of widget input processing
 **/
result widget::process_input(ice::input::input *input) {
	if (!input)
		return IC_FAIL_GEN;

	short x, y;
	input->get_pos(&x, &y);
	ice::math::vec2 mouse_pos((math::real)x, (math::real)y);

	// update the input state
	math::aabb2 box;
	box.extents.x = size_.x / 2.0f;
	box.extents.y = size_.y / 2.0f;
	box.pos       = pos_;

	mouse_over_ = point_in_aabb2(mouse_pos, box);

	if (highlight_enabled_ && !sticky)
		set_highlight(mouse_over_);

	return IC_OK;
}

/*! Updates the UI Widget
 *
 *  @param          delta_seconds      Elapsed time since lase update
 *  @returns        ICRESULT        Success/failure of Updating the widget
 **/
result widget::update(const math::real delta_seconds [[maybe_unused]]) {
	if (update_buffer_)
		update_buffer();

	return IC_OK;
}

/*! Renders the UI Widget
 *
 *  @returns        ICRESULT        Success/failure of rendering the widget
 **/
result widget::render(void) {
	// make sure we have a device
	ice::render::graphics_device *dev = content_->get_device();
	if (dev) {
		if (vb_) {
			// handle textures and highlighting
			if (highlight_on_ && tex2_)
				dev->set_texture(0, tex2_);
			else if (tex1_)
				dev->set_texture(0, tex1_);

			ice::math::mat4 temp;
			(temp) = glm::translate(ice::math::mat4(1), ice::math::vec3(pos_.x, pos_.y, 0.0f));
			dev->set_world_transform(temp);

			dev->enable_depth_test(false);
			dev->enable_alpha_blend(true);

			// draw the widgets vertex buffer
			dev->draw_vert_buf(vb_);

			dev->enable_depth_test(true);
			dev->enable_alpha_blend(false);
		}
		return IC_OK;
	}
	return IC_WARN_GEN;
}

/*! Updates the widgets vertex buffer
 **/
void widget::update_buffer(void) {
	if (!content_)
		return;

	ice::render::graphics_device *dev = content_->get_device();
	if (!dev)
		return;

	if (!vb_) {
		ice::render::vert_def description;
		description.num_prims = 2;
		description.prim_type = ice::render::prim_tlist;
		description.num_verts = 6;
		if (tex1_ || tex2_) {
			description.vert_size = sizeof(ice::render::vrt2_dcol_dif);
			description.vert_type = ice::render::ic_vert_2_dcol_dif;
		} else {
			description.vert_size = sizeof(ice::render::vrt2_dcol);
			description.vert_type = ice::render::ic_vert_2_dcol;
		}
		description.usage = IC_VERT_USAGE_STATIC;

		dev->create_vb(&vb_, &description);
	}

	// error!
	if (!vb_)
		return;

	// update the verts
	ice::render::vert_lock vert_lock = {};
	if (ICEOK(vb_->lock(&vert_lock))) {
		math::real halfx = size_.x / 2.0f;
		math::real halfy = size_.y / 2.0f;

		const ice::math::color &color = (highlight_on_) ? high_color_ : color_;
		ice::math::vec2        *puv   = (highlight_on_) ? uvs_[1] : uvs_[0];

		if (vb_->get_vert_def()->vert_type == ice::render::ic_vert_2_dcol) {
			ice::render::vrt2_dcol *vb = (ice::render::vrt2_dcol *)vert_lock.data;

			(vb[0].pos) = ice::math::vec2(-halfx, -halfy);
			vb[0].color = color;

			(vb[1].pos) = ice::math::vec2(-halfx, halfy);
			vb[1].color = color;

			(vb[2].pos) = ice::math::vec2(halfx, halfy);
			vb[2].color = color;

			(vb[3].pos) = ice::math::vec2(halfx, halfy);
			vb[3].color = color;

			(vb[4].pos) = ice::math::vec2(halfx, -halfy);
			vb[4].color = color;

			(vb[5].pos) = ice::math::vec2(-halfx, -halfy);
			vb[5].color = color;
		} else if (vb_->get_vert_def()->vert_type == ice::render::ic_vert_2_dcol_dif) {
			ice::render::vrt2_dcol_dif *vb = (ice::render::vrt2_dcol_dif *)vert_lock.data;

			(vb[0].pos) = ice::math::vec2(-halfx, -halfy);
			vb[0].color = color;
			vb[0].uv    = puv[0];

			(vb[1].pos) = ice::math::vec2(-halfx, halfy);
			vb[1].color = color;
			vb[1].uv    = puv[1];

			(vb[2].pos) = ice::math::vec2(halfx, halfy);
			vb[2].color = color;
			vb[2].uv    = puv[2];

			(vb[3].pos) = ice::math::vec2(halfx, halfy);
			vb[3].color = color;
			vb[3].uv    = puv[2];

			(vb[4].pos) = ice::math::vec2(halfx, -halfy);
			vb[4].color = color;
			vb[4].uv    = puv[3];

			(vb[5].pos) = ice::math::vec2(-halfx, -halfy);
			vb[5].color = color;
			vb[5].uv    = puv[0];
		}

		vb_->unlock();
	}
	update_buffer_ = false;
}

} // namespace ice
