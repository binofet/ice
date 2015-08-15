#ifndef __NET_MENU_STATE_H__
#define __NET_MENU_STATE_H__


//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "Util/icStateMachine.h"
#include "Content/icContentLoader.h"
#include "UI/icFrame.h"
#include "UI/icButton.h"


/*! Implements the games main menu
**/
class NetMenu : public icState
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    NetMenu(void);
    ~NetMenu(void);

    ICRESULT Init(icContentLoader* pContentLoader);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    ICRESULT Update(const float fDeltaTime, bool* bFinished);
    ICRESULT Render(void);

    void OnPush(void);
    void OnPop(void);

private:
    icContentLoader*    m_pContent;

    // renderable objects
    icFrame*            m_uiFrame;

    icWidget*           m_wgtNetPlay;

    icButton*           m_btnHost;
    icButton*           m_btnJoinIP;
    icButton*           m_btnFind;
    icButton*           m_btnMainMenu;

    icSoundI*           m_pClick;
    icSoundI*           m_pHover;
};



#endif //__NET_MENU_STATE_H__