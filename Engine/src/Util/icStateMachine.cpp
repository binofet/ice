
#include "Util/icStateMachine.h"



/*! c'tor
**/
icStateMachine::icStateMachine()
{
    m_ptrCurrent = NULL;
    // todo, zero this
    //m_Stack[ICMAXSTACK];
    m_u8StackIndex = 0;
}// END FUNCTION icStateMachine(void)


/*! d'tor
**/
icStateMachine::~icStateMachine(void)
{
}// END FUNCTION ~icStateMachine(void)


/*! Updates the state machine
 *
 *  @param      fDeltaTime      Elapsed time since last update
 *  @returns    ICRESULT        Status after updating state machine
**/
ICRESULT icStateMachine::Update(const float fDeltaTime)
{
    ICRESULT res = IC_OK;
    if (m_ptrCurrent)
    {
        bool done = false;
        int cur = m_u8StackIndex-1;
        icState* prev = (cur >= 1) ? m_Stack[--cur] : NULL;
        while (prev)
        {
            if (!prev->UpdateTopOnly())
            {
                bool finished = false;
                prev->Update(fDeltaTime,&finished);
                break;
            }
            if (cur > 0)
                prev = m_Stack[--cur];
            else
                prev = NULL;
        }

        res = m_ptrCurrent->Update(fDeltaTime,&done);
        if (done)
            PopState();
    }
    return res;
}// END FUNCTION Update(const float fDeltaTime)


/*! Renders the state
 *
 *  @returns    ICRESULT        Status after rendering state machine
**/
ICRESULT icStateMachine::Render(void)
{
    ICRESULT res = IC_OK;
    if (m_ptrCurrent)
    {
        int cur = m_u8StackIndex-1;
        icState* prev = (cur >= 1) ? m_Stack[--cur] : NULL;
        while (prev)
        {
            if (!prev->RenderTopOnly())
            {
                prev->Render();
            }
            else
                break;

            if (cur > 0)
                prev = m_Stack[--cur];
            else
                prev = NULL;
        }

        res = m_ptrCurrent->Render();
    }
    return res;
}// END FUNCTION Render(void)


/*! Push a new state onto the machine
 *
 *  @param      pNextState      State to push onto the stack
 *  @returns    ICRESULT        Status after pushing state
**/
ICRESULT icStateMachine::PushState(icState* pNextState)
{
    if (m_u8StackIndex < ICMAXSTACK-1)
    {
        m_Stack[m_u8StackIndex++] = pNextState;
        pNextState->OnPush();
        m_ptrCurrent = pNextState;
        return IC_OK;
    }
    return IC_FAIL_GEN;
}// END FUNCTION PushState(icState* pNextState)


/*! Pops current state off stack
 *
 *  @returns    icState         The current state (can be NULL)
**/
icState* icStateMachine::PopState()
{
    icState* current = NULL;
    if (m_u8StackIndex > 0)
    {
        current = m_Stack[--m_u8StackIndex];
        if (m_u8StackIndex > 0)
            m_ptrCurrent = m_Stack[m_u8StackIndex-1];
        else
            m_ptrCurrent = NULL;
    }
    return current;
}// END FUNCTION PopState(bool bWaitForFinish)
