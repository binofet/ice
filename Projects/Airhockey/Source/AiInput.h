#ifndef __AI_INPUT_H__
#define __AI_INPUT_H__


#include "InputHandler.h"


/*! Abstract interface for input handling
**/
class AiInput : public InputHandler
{
public:
    AiInput(int player, b2Body* puck, b2Body* paddle);

    void ResetTarget() override;
    virtual ICRESULT Process(icInput* pInput, float dt);

private:
    float m_reaction = 0;
    icVector2 m_target;

};

#endif //__AI_INPUT_H__