#ifndef __NETWORK_INPUT_H__
#define __NETWORK_INPUT_H__

#include "InputHandler.h"

#include "ice/util/time.h"

/*! Abstract interface for input handling
 **/
class NetworkInput : public InputHandler {
public:
	NetworkInput(int player, b2Body *puck, b2Body *paddle);

	virtual ice::result process(ice::input::input *pInput, float dt);

private:
	ice::util::timer timer_;
};

#endif //__NETWORK_INPUT_H__