#ifndef __AI_INPUT_H__
#define __AI_INPUT_H__

#include "InputHandler.h"

/*! Abstract interface for input handling
 **/
class AiInput : public InputHandler {
public:
	AiInput(int player, b2Body *puck, b2Body *paddle);

	void                reset_target() override;
	virtual ice::result process(ice::input::input *pInput, float dt);

private:
	float           reaction_ = 0;
	ice::math::vec2 target_;
};

#endif //__AI_INPUT_H__