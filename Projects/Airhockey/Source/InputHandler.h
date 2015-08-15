#ifndef __INPUT_HANDLER_H__
#define __INPUT_HANDLER_H__


#include "Math/Vector/icVector2.h"


class icInput;
class b2Body;

#define PLAYER_ONE  0
#define PLAYER_TWO  1

/*! Abstract interface for input handling
**/
class InputHandler
{
public:
    InputHandler(int player, b2Body* puck, b2Body* paddle)
    {
        m_i32Player = player;
        m_Puck = puck;
        m_Paddle = paddle;
    };

    virtual ICRESULT Process(icInput* pInput)=0;

protected:
    int     m_i32Player;
    b2Body* m_Puck;
    b2Body* m_Paddle;
};

#endif //__INPUT_HANDLER_H__