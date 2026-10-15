
#include "ice/slider.h"

#include "ice/assets/content_loader.h"
#include "ice/input/input.h"
#include "ice/render/graphics_device.h"
#include "ice/render/vert_buffer.h"
namespace ice {

/*! c'tor
 **/
slider::slider(ice::assets::content_loader *content) : widget(content) {
	mstate_ = mstate_waiting;

	(size_) = ice::math::vec2(238.0f, 43.0f);
	(pos_)  = ice::math::vec2(0.0f, 0.0);

	value_  = 0;
	minval_ = 0;
	maxval_ = 100;

	fpos_ = 0.0f;

	selected_ = false;

	ticker_vb_ = NULL;
}

/*! Input process function
 **/
result slider::process_input(ice::input::input *input) {
	if (!input)
		return IC_FAIL_GEN;

	bool ldown = input->is_down(ice::input::mouse_l);

	short x, y;
	input->get_pos(&x, &y);
	ice::math::vec2 mouse_pos((math::real)x, (math::real)y);

	// update the input state
	math::aabb2 box;
	box.extents.x  = size_.x / 2.0f;
	snap_interval_ = (snap_interval_ > 1) ? snap_interval_ : 1;
	box.extents.x += ((math::real)snap_interval_ + 1.0f) / ((math::real)maxval_ + 1.0f);
	box.extents.y = tick_size_.y / 2.0f;
	box.pos       = pos_;

	bool over_bar = point_in_aabb2(mouse_pos, box);

	if (ldown) {
		if (mstate_ == mstate_first_down && over_bar)
			mstate_ = mstate_first_drag;
		if (mstate_ == mstate_waiting_over_bar && over_bar)
			mstate_ = mstate_first_down;
	} else {
		if (mstate_ == mstate_first_up || (mstate_ == mstate_waiting_over_bar && !over_bar))
			mstate_ = mstate_waiting;
		else if (mstate_ == mstate_waiting && !ldown && over_bar)
			mstate_ = mstate_waiting_over_bar;
		else if (mstate_ == mstate_first_drag)
			mstate_ = mstate_first_up;
	}

	// Update the values
	if (mstate_ == mstate_first_down) {
		fpos_      = (mouse_pos.x + size_.x / 2.0f - pos_.x) / size_.x * (math::real)(maxval_);
		value_     = (int)floor(fpos_ + 0.5f);
		click_pos_ = mouse_pos;
	} else if (mstate_ == mstate_first_drag) {
		math::real dif = mouse_pos.x - click_pos_.x;
		fpos_ += ((math::real)dif / size_.x) * (math::real)(maxval_ - minval_);

		value_     = (int)floor(fpos_ + 0.5f) + minval_;
		click_pos_ = mouse_pos;
	} else if (selected_ && input->is_pressed(ice::input::key_right))
		value_ += snap_interval_;
	else if (selected_ && input->is_pressed(ice::input::key_left))
		value_ -= snap_interval_;

	value_ = (value_ > maxval_) ? maxval_ : value_;
	value_ = (value_ < minval_) ? minval_ : value_;

	return widget::process_input(input);
}

/*! Updates the icSlider
 *
 *      if the icSlider has focus, the user can interat with the keyboard
 *      otherwise the mouse drives the icSlider
 *
 *  @param      delta_seconds      Elapsed time since last update
 **/
result slider::update(const math::real delta_seconds) { return widget::update(delta_seconds); }

/*! Renders the icSlider
 **/
result slider::render(void) {
	result result_code = IC_OK;

	set_pos(pos_);

	// make sure we have a device
	ice::render::graphics_device *dev = content_->get_device();
	if (dev) {
		if (vb_ && ticker_vb_) {
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

			// draw the widgets vertex buffers
			dev->draw_vert_buf(vb_);

			math::real tempx = (size_.x) / (math::real)(maxval_ - minval_);
			(temp)           = glm::translate(
                ice::math::mat4(1),
                ice::math::vec3(pos_.x - size_.x / 2.0f + (math::real)(value_ - minval_) * tempx,
			                              pos_.y, 0.0f));
			dev->set_world_transform(temp);
			dev->draw_vert_buf(ticker_vb_);

			dev->enable_depth_test(true);
			dev->enable_alpha_blend(false);
		}
		return result_code;
	}
	return IC_WARN_GEN;
}

/*! Called when the vertex buffers need to be updated
 **/
void slider::update_buffer(void) {
	if (!content_)
		return;

	ice::render::graphics_device *dev = content_->get_device();
	if (!dev)
		return;

	// make sure we have the ticker vertex buffer
	if (!ticker_vb_) {
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

		dev->create_vb(&ticker_vb_, &description);
	}

	// error!
	if (!ticker_vb_)
		return;

	ice::render::vert_lock vert_lock;
	if (ICEOK(ticker_vb_->lock(&vert_lock))) {
		math::real halfx = tick_size_.x / 2.0f;
		math::real halfy = tick_size_.y / 2.0f;

		const ice::math::color &color = (highlight_on_) ? high_color_ : color_;
		ice::math::vec2        *puv   = (highlight_on_) ? tick_uvs_[1] : tick_uvs_[0];

		if (ticker_vb_->get_vert_def()->vert_type == ice::render::ic_vert_2_dcol) {
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
		} else if (ticker_vb_->get_vert_def()->vert_type == ice::render::ic_vert_2_dcol_dif) {
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

		ticker_vb_->unlock();
	}

	widget::update_buffer();
}
} // namespace ice
