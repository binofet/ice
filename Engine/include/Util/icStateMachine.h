#ifndef __IC_STATE_MACHINE_H__
#define __IC_STATE_MACHINE_H__


#include "Core/icGlobals.h"

#define ICMAXSTACK 64

/*! Provides an abstract interface for application state
**/
class icState
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icState(void) {m_bRenderTopOnly = true;m_bUpdateTopOnly=true;};
    virtual ~icState(void){};

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT Update(const float fDeltaTime, bool* bFinished)=0;
    virtual ICRESULT Render(void)=0;

    virtual void OnPush(void)=0;
    virtual void OnPop(void)=0;

    bool RenderTopOnly(void) {return m_bRenderTopOnly;};
    bool UpdateTopOnly(void) {return m_bUpdateTopOnly;};

protected:

    bool        m_bRenderTopOnly;
    bool        m_bUpdateTopOnly;
};


/*! Provides a basic stack-based state machine
**/
class icStateMachine
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icStateMachine(void);
    virtual ~icStateMachine(void);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT Update(const float fDeltaTime);
    virtual ICRESULT Render(void);

    virtual ICRESULT PushState(icState* pNextState);

    virtual icState* PopState(void);

    const icState* const PeekState(void) const;

private:

    icState*        m_Stack[ICMAXSTACK];
    icState*        m_ptrCurrent;
    unsigned char   m_u8StackIndex;
};


/*! Returns the current state
**/
FORCE_INLINE const icState* const icStateMachine::PeekState(void) const
{
    return m_ptrCurrent;
}// END FUNCTION PeekState(void) const

#endif // __IC_STATE_MACHINE_H__