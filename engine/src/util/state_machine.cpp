
#include "ice/util/state_machine.h"
namespace ice::util {

/*! c'tor
 **/
state_machine::state_machine() {
	ptr_current_ = NULL;
	// todo, zero this
	// m_Stack[ICMAXSTACK];
	stack_index_ = 0;
}

/*! d'tor
 **/
state_machine::~state_machine(void) {}

/*! Updates the state machine
 *
 *  @param      delta_seconds      Elapsed time since last update
 *  @returns    ICRESULT        Status after updating state machine
 **/
ice::result state_machine::update(const float delta_seconds) {
	ice::result result_code = IC_OK;
	if (ptr_current_) {
		bool   done = false;
		int    cur  = stack_index_ - 1;
		state *prev = (cur >= 1) ? stack_[--cur] : NULL;
		while (prev) {
			if (!prev->update_top_only()) {
				bool finished = false;
				prev->update(delta_seconds, &finished);
				break;
			}
			if (cur > 0)
				prev = stack_[--cur];
			else
				prev = NULL;
		}

		result_code = ptr_current_->update(delta_seconds, &done);
		if (done)
			pop_state();
	}
	return result_code;
}

/*! Renders the state
 *
 *  @returns    ICRESULT        Status after rendering state machine
 **/
ice::result state_machine::render(void) {
	ice::result result_code = IC_OK;
	if (ptr_current_) {
		int    cur  = stack_index_ - 1;
		state *prev = (cur >= 1) ? stack_[--cur] : NULL;
		while (prev) {
			if (!prev->render_top_only()) {
				prev->render();
			} else
				break;

			if (cur > 0)
				prev = stack_[--cur];
			else
				prev = NULL;
		}

		result_code = ptr_current_->render();
	}
	return result_code;
}

/*! Push a new state onto the machine
 *
 *  @param      next_state      State to push onto the stack
 *  @returns    ICRESULT        Status after pushing state
 **/
ice::result state_machine::push_state(state *next_state) {
	if (stack_index_ < ICMAXSTACK - 1) {
		stack_[stack_index_++] = next_state;
		next_state->on_push();
		ptr_current_ = next_state;
		return IC_OK;
	}
	return IC_FAIL_GEN;
}

/*! Pops current state off stack
 *
 *  @returns    icState         The current state (can be NULL)
 **/
state *state_machine::pop_state() {
	state *current = NULL;
	if (stack_index_ > 0) {
		current = stack_[--stack_index_];
		if (stack_index_ > 0)
			ptr_current_ = stack_[stack_index_ - 1];
		else
			ptr_current_ = NULL;
		current->on_pop();
	}
	return current;
}

} // namespace ice::util
