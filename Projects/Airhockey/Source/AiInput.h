#ifndef __AI_INPUT_H__
#define __AI_INPUT_H__


#include "InputHandler.h"


/*! Abstract interface for input handling
**/
class AiInput : public InputHandler
{
public:
    AiInput(int player, b2Body* puck, b2Body* paddle);

    virtual ICRESULT Process(icInput* pInput);

private:

};

#endif //__AI_INPUT_H__