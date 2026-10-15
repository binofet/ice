#pragma once
namespace ice {} // namespace ice
#include "ice/widget.h"
namespace ice {

/*! Provides a slider ui widget
 **/
class slider : public widget {
public:
	slider(ice::assets::content_loader *content);

	virtual result process_input(ice::input::input *input);
	virtual result update(const math::real delta_seconds);
	virtual result render(void);

	/// Return the currently selected slider value.
	int get_val(void) { return value_; };
	/// Set the slider value, subject to its configured range/snapping.
	void set_val(int new_val) { value_ = new_val; };
	/// Set the allowed numeric slider range.
	void set_min_max(int min, int max) {
		minval_ = min;
		maxval_ = max;
	};

	/// Select the widgets active/highlighted state.
	void set_selected(bool selected) { selected_ = selected; };

	/// Set the numeric snapping interval used by slider interaction.
	void set_snap_interval(int interval) { snap_interval_ = interval; };

	void set_tick_u_vs(const ice::math::vec2 &tl, const ice::math::vec2 &tr,
	                   const ice::math::vec2 &bl, const ice::math::vec2 &br);

	void set_tick_high_u_vs(const ice::math::vec2 &tl, const ice::math::vec2 &tr,
	                        const ice::math::vec2 &bl, const ice::math::vec2 &br);

	/// Set the visual size of slider ticks in logical UI coordinates.
	void set_tick_size(const ice::math::vec2 &size) { tick_size_ = size; };

protected:
	virtual void update_buffer(void);

	enum mstate {
		mstate_waiting = 0,
		mstate_waiting_over_bar,
		mstate_first_down,
		mstate_first_drag,
		mstate_first_up,
		mstate_num_states
	};

	int             snap_interval_;
	int             value_;
	int             minval_;
	int             maxval_;
	ice::math::vec2 click_pos_;
	ice::math::vec2 tick_size_;

	ice::math::vec2 tick_uvs_[2][4];

	math::real fpos_;

	ice::render::vert_buffer *ticker_vb_;

	bool selected_;

	mstate mstate_;
};

/*! Sets the regular uvs for the slider ticker
 **/
FORCE_INLINE
void slider::set_tick_u_vs(const ice::math::vec2 &tl, const ice::math::vec2 &tr,
                           const ice::math::vec2 &bl, const ice::math::vec2 &br) {
	update_buffer_ = true;

	tick_uvs_[0][0] = bl;
	tick_uvs_[0][1] = tl;
	tick_uvs_[0][2] = tr;
	tick_uvs_[0][3] = br;
}
//                         const icVector2& bl, const icVector2& br)

/*! Sets the highlight uvs for the slider ticker
 **/
FORCE_INLINE
void slider::set_tick_high_u_vs(const ice::math::vec2 &tl, const ice::math::vec2 &tr,
                                const ice::math::vec2 &bl, const ice::math::vec2 &br) {
	update_buffer_ = true;

	tick_uvs_[1][0] = bl;
	tick_uvs_[1][1] = tl;
	tick_uvs_[1][2] = tr;
	tick_uvs_[1][3] = br;
}
//                             const icVector2& bl, const icVector2& br)
} // namespace ice
