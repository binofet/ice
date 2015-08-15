#ifndef __PAUSE_MENU_STATE_H__
#define __PAUSE_MENU_STATE_H__


//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "Util/icStateMachine.h"
#include "Content/icContentLoader.h"
#include "Render/Camera/icCamera.h"
#include "UI/icFrame.h"
#include "UI/icButton.h"

class icSoundI;

/*! Implements the games main menu
**/
class PauseMenu : public icState
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    PauseMenu(void);
    ~PauseMenu(void);

    ICRESULT Init(icContentLoader* pContentLoader);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    ICRESULT Update(const float fDeltaTime, bool* bFinished);
    ICRESULT Render(void);

    void OnPush(void);
    void OnPop(void);

    bool SkipUpdate;

private:
    icContentLoader*    m_pContent;

    // renderable objects
    icFrame*            m_uiFrame;

    icWidget*           m_wgtPaused;

    icButton*           m_btnMainMenu;
    icButton*           m_btnSettingsMenu;
    icButton*           m_btnBack;

    icSoundI*           m_pClick;
    icSoundI*           m_pHover;

    icSoundI*           m_pPause;
};



#endif //__PAUSE_MENU_STATE_H__