
#include "ice/button.h"

#include "ice/input/input.h"
namespace ice {

/*! c'tor
 **/
button::button(ice::assets::content_loader *content) : widget(content) {
	clicked_ = NULL;
	hovered_ = NULL;

	tex_down_ = NULL;

	click_timer_ = -1.0f;
	hover_timer_ = -1.0f;

	down_      = false;
	last_down_ = false;
}

/*! Cleans up the ui button
 *
 *  @returns        ICRESULT        Success/failure of cleanup
 **/
result button::cleanup(void) { return IC_OK; }

/*! Processes input for the Button.
 *
 *  @param      input          Pointer to current input object
 *  @returns    ICRESULT        Success/failure of widget input processing
 **/
result button::process_input(ice::input::input *input) {
	if (!input)
		return IC_FAIL_GEN;

	if (highlight_on_ && !sticky) {
		if (!down_ && last_down_ && clicked_)
			clicked_->play();

		down_      = input->is_down(ice::input::mouse_l);
		last_down_ = down_;
	} else if (sticky && mouse_over_) {
		bool temp = input->is_pressed(ice::input::mouse_l);
		if (temp) {
			last_down_ = down_;
			down_      = !down_;
		}
	}

	// if (Sticky && !m_bDown && !m_bLastDown)
	//     m_bLastDown = true;

	return widget::process_input(input);
}

/*! Updates the UI Button
 *
 *  @param          delta_seconds      Elapsed time since lase update
 *  @returns        ICRESULT        Success/failure of Updates the widget
 **/
result button::update(const math::real delta_seconds) {
	if (click_timer_ > 0.0f)
		click_timer_ -= delta_seconds;

	if (hover_timer_ > 0.0f)
		hover_timer_ -= delta_seconds;

	if (sticky)
		highlight_on_ = down_;

	return widget::update(delta_seconds);
}

/*! Renders the UI Button
 *
 *  @returns        ICRESULT        Success/failure of rendering the widget
 **/
result button::render(void) {
	// TODO, allow color buttons with text?
	// if (strlen(m_szText) > 0)
	//{
	//}
	return widget::render();
}

/*! Toggles the highlight state (overloaded from icWidget)
 *
 *  @param          highlight_on        true - set for highlight
 **/
void button::set_highlight(bool highlight_on) {
	if (hovered_ && hover_timer_ < 0.0f) {
		// no hover sound for 0.5 seconds
		if (!highlight_on_ && highlight_on) {
			// TODO: expose this parameter!
			hover_timer_ = 0.2f;
			hovered_->play();
		} else if (highlight_on_ && !highlight_on) {
			hover_timer_ = 0.5f;
			hovered_->play();
		}
	}

	widget::set_highlight(highlight_on);
}
} // namespace ice
