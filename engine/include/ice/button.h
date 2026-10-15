#pragma once

#include "ice/audio/sound.h"
#include "ice/input/input.h"

#include "ice/widget.h"
namespace ice {

/*! Abstract interface for all interface objects
 **/
class button : public widget {
public:
	button(ice::assets::content_loader *content);
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	virtual ~button(void) { cleanup(); };

	virtual result cleanup(void);

	virtual result process_input(ice::input::input *input);
	virtual result update(const math::real delta_seconds);
	virtual result render(void);

	virtual void set_highlight(bool highlight_on);

	void set_sounds(ice::audio::sound_i *clicked, ice::audio::sound_i *hovered);

	/// Borrow the texture displayed while this button is pressed; the caller retains ownership.
	void set_texture_down(ice::render::texture *down);

	/// Set the native held-key/button state in the input accumulator.
	void set_down(bool down);
	/// Return whether the key/button is currently held.
	bool is_down(void) const;

	/// Return whether the widget activation transition occurred this frame.
	bool was_pressed(void) const;

protected:
	ice::audio::sound_i *clicked_;
	ice::audio::sound_i *hovered_;

	float click_timer_;
	float hover_timer_;

	ice::render::texture *tex_down_;

	char text_[64];

	bool down_;
	bool last_down_;
};

FORCE_INLINE void button::set_down(bool down) {
	last_down_ = down_;
	down_      = down;
}

FORCE_INLINE bool button::is_down(void) const {
	if (!sticky)
		return (highlight_on_ && ice::input::input::instance()->is_down(ice::input::mouse_l));

	return down_;
}

FORCE_INLINE bool button::was_pressed(void) const {
	return (highlight_on_ && ice::input::input::instance()->is_pressed(ice::input::mouse_l));
	// return (!m_bDown && m_bLastDown);
}

/*! Set the clicked and hovered sound pointers
 *
 *  @param          clicked        Pointer to the clicked sound
 *  @param          hovered        Pointer to the hovered sound
 **/
FORCE_INLINE void button::set_sounds(ice::audio::sound_i *clicked, ice::audio::sound_i *hovered) {
	clicked_ = clicked;
	hovered_ = hovered;
}

} // namespace ice
