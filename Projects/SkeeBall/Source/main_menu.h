#ifndef __MAIN_MENU_STATE_H__
#define __MAIN_MENU_STATE_H__


//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "Util/icStateMachine.h"
#include "Content/icContentLoader.h"
#include "UI/icFrame.h"
#include "UI/icButton.h"


class icSoundI;

/*! Implements the games main menu
**/
class MainMenu : public icState
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    MainMenu(void);
    ~MainMenu(void);

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
    icButton*           m_btnOnePlayer;
    icButton*           m_btnTwoPlayer;
    icButton*           m_btnNetPlay;
    icButton*           m_btnSettings;
    icButton*           m_btnExit;

    icSoundI*           m_pHover;
    icSoundI*           m_pClick;
};



#endif //__MAIN_MENU_STATE_H__