#ifndef __INPUT_HANDLER_H__
#define __INPUT_HANDLER_H__

#include "ice/math/vec2.h"
namespace ice::input {
class input;
}
class b2Body;

#define PLAYER_ONE 0
#define PLAYER_TWO 1

/*! Abstract interface for input handling
 **/
class InputHandler {
public:
	InputHandler(int player, b2Body *puck, b2Body *paddle) {
		player_ = player;
		puck_   = puck;
		paddle_ = paddle;
	};

	virtual ~InputHandler() {}
	virtual void        reset_target() {}
	virtual void        prepare_frame(ice::input::input *) {}
	virtual ice::result process(ice::input::input *pInput, float dt) = 0;

protected:
	int     player_;
	b2Body *puck_;
	b2Body *paddle_;
};

#endif //__INPUT_HANDLER_H__