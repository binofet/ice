#ifndef __PLAYER_INPUT_H__
#define __PLAYER_INPUT_H__

#include "ice/util/time.h"

#include "InputHandler.h"

/*! Abstract interface for input handling
 **/
class PlayerInput : public InputHandler {
public:
	PlayerInput(int player, b2Body *puck, b2Body *paddle);

	void                prepare_frame(ice::input::input *input) override;
	void                reset_target() override;
	virtual ice::result process(ice::input::input *pInput, float dt);

private:
	ice::math::vec2 target_;

	ice::util::timer time_;
};

#endif //__PLAYER_INPUT_H__