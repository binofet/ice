#ifndef __NETWORK_INPUT_H__
#define __NETWORK_INPUT_H__


#include "InputHandler.h"

#include "Util/icTime.h"


/*! Abstract interface for input handling
**/
class NetworkInput : public InputHandler
{
public:
    NetworkInput(int player, b2Body* puck, b2Body* paddle);

    virtual ICRESULT Process(icInput* pInput);

private:

    icTimer m_timer;
};

#endif //__NETWORK_INPUT_H__