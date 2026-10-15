
#include "ice/frame.h"
namespace ice {

/*! c'tor
 **/
frame::frame(ice::assets::content_loader *content) : widget(content) {
	head_     = NULL;
	selected_ = NULL;
}

/*! Cleans up the ui frame
 *
 *  @returns        ICRESULT        Success/failure of cleanup
 **/
result frame::cleanup(void) { return IC_OK; }

/*! Frame processes input, and then all children
 *
 *  @returns        ICRESULT        Success/failure of update operation
 **/
result frame::process_input(ice::input::input *input) {
	result result_code = widget::process_input(input);

	if (!head_)
		return result_code;

	widget *cur = head_;
	while (cur) {
		result_code |= cur->process_input(input);
		cur = cur->next_;
	}
	return result_code;
}

/*! Updates the frame, and then all children
 *
 *  @returns        ICRESULT        Success/failure of update operation
 **/
result frame::update(const math::real delta_seconds) {
	result result_code = widget::update(delta_seconds);

	if (!head_)
		return result_code;

	widget *cur = head_;
	while (cur) {
		result_code |= cur->update(delta_seconds);
		cur = cur->next_;
	}
	return result_code;
}

/*! Renders the frame, and then all children
 *
 *  @returns        ICRESULT        Success/failure of render operation
 **/
result frame::render(void) {
	// TODO: figure out this business
	result result_code = IC_OK; // icWidget::Render();

	if (!head_)
		return result_code;

	widget *cur = head_;
	while (cur) {
		result_code |= cur->render();
		cur = cur->next_;
	}
	return result_code;
}

/*! Add a child widget to the frame
 *
 *  @param          out_object        Pointer to widget
 *  @param          relative    Use relative positioning?
 *  @returns        ICRESULT    Success/failure
 **/
result frame::add_child(widget *out_object, bool relative) {
	(void)relative; // TODO: allow relative positioning
	if (!head_) {
		head_     = out_object;
		selected_ = head_;
		// m_pSelected->icWidget::SetHighlight(true);
		return IC_OK;
	}

	widget *cur  = head_->next_;
	widget *prev = head_;

	while (cur) {
		prev = cur;
		cur  = cur->next_;
	}
	prev->next_ = out_object;

	return IC_OK;
}

/*! Find a child widget
 *
 *  @param          out_object        Pointer to widget
 *  @returns        ICRESULT    Found(IC_OK)/not found(IC_FAIL_GEN)
 **/
result frame::find_child(widget *out_object) {
	if (!out_object || !head_)
		return IC_FAIL_GEN;

	widget *cur = head_;
	while (cur) {
		if (cur == out_object)
			return IC_OK;

		cur = cur->next_;
	}

	return IC_FAIL_GEN;
}

/*! Removes a child widget
 *
 *  @param          out_object        Pointer to widget
 *  @returns        ICRESULT    Removed(IC_OK)/not found(IC_FAIL_GEN)
 **/
result frame::remove_child(widget *out_object) {
	if (!out_object || !head_)
		return IC_FAIL_GEN;

	if (out_object == head_) {
		widget *temp = head_->next_;
		head_->next_ = NULL;
		head_        = temp;
		return IC_OK;
	}

	widget *cur  = head_->next_;
	widget *prev = head_;

	while (cur) {
		if (cur == out_object) {
			prev->next_ = cur->next_;
			cur->next_  = NULL;
			return IC_OK;
		}
		prev = cur;
		cur  = cur->next_;
	}

	return IC_FAIL_GEN;
}
} // namespace ice
