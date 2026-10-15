#pragma once

#include "ice/globals.h"
namespace ice::util {

#define ICMAXSTACK 64

/*! Provides an abstract interface for application state
 **/
class state {
public:
	/// Construct inactive state; initialize explicitly before active use.
	state(void) {
		render_top_only_ = true;
		update_top_only_ = true;
	};
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	virtual ~state(void) {};

	/// Advance frame-dependent state on the owning thread; elapsed-time arguments are seconds.
	virtual ice::result update(const float delta_seconds, bool *finished) = 0;
	/// Render using the current owning graphics context; inspect the result before continuing a
	/// failed frame.
	virtual ice::result render(void) = 0;

	/// Observe activation after the state is pushed onto the borrowed state stack.
	virtual void on_push(void) = 0;
	/// Observe deactivation before the state leaves the borrowed state stack.
	virtual void on_pop(void) = 0;

	/// Render this frame and its overlays in the current device context.
	bool render_top_only(void) { return render_top_only_; };
	/// Update this frame without recursively updating all descendants.
	bool update_top_only(void) { return update_top_only_; };

protected:
	bool render_top_only_;
	bool update_top_only_;
};

/*! Provides a basic stack-based state machine
 **/
class state_machine {
public:
	state_machine(void);
	virtual ~state_machine(void);

	virtual ice::result update(const float delta_seconds);
	virtual ice::result render(void);

	virtual ice::result push_state(state *next_state);

	virtual state *pop_state(void);

	const state *peek_state(void) const;

private:
	state        *stack_[ICMAXSTACK];
	state        *ptr_current_;
	unsigned char stack_index_;
};

/*! Returns the current state
 **/
FORCE_INLINE const state *state_machine::peek_state(void) const { return ptr_current_; }

} // namespace ice::util
