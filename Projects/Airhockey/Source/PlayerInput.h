#ifndef __PLAYER_INPUT_H__
#define __PLAYER_INPUT_H__

#include "Util/icTime.h"


#include "InputHandler.h"


/*! Abstract interface for input handling
**/
class PlayerInput : public InputHandler
{
public:
    PlayerInput(int player, b2Body* puck, b2Body* paddle);

    void PrepareFrame(icInput* input) override;
    void ResetTarget() override;
    virtual ICRESULT Process(icInput* pInput, float dt);

private:
    icVector2       m_target;

    icTimer         m_time;
};

#endif //__PLAYER_INPUT_H__