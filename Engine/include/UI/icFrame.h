#ifndef __IC_FRAME_H__
#define __IC_FRAME_H__

#include "icWidget.h"


/*! Basic interface for a widget frame
**/
class icFrame : public icWidget
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icFrame(icContentLoader* pContent);
    virtual ~icFrame(void) {Cleanup();};

    virtual ICRESULT Cleanup(void);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT ProcessInput(icInput* pInput);
    virtual ICRESULT Update(const icReal fDeltaTime);
    virtual ICRESULT Render(void);

    virtual ICRESULT AddChild(icWidget* pObj, bool relative=false);
    virtual ICRESULT FindChild(icWidget* pObj);
    virtual ICRESULT RemoveChild(icWidget* pObj);


private:
    icWidget*   m_pHead;

    icWidget*   m_pSelected;
};

#endif //__IC_FRAME_H__