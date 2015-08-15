#ifndef __SETTINGS_MENU_STATE_H__
#define __SETTINGS_MENU_STATE_H__


//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "Util/icStateMachine.h"
#include "Content/icContentLoader.h"
#include "Render/Camera/icCamera.h"
#include "UI/icFrame.h"
#include "UI/icButton.h"
#include "UI/icSlider.h"

class icSlider;

/*! Implements the games main menu
**/
class SettingsMenu : public icState
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    SettingsMenu(void);
    ~SettingsMenu(void);

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
    icWidget*           m_ptxtSettings;
    icWidget*           m_ptxtSfx;
    icWidget*           m_ptxtMusic;

    icButton*           m_pbtnBack;

    icSlider*           m_slidSfxVol;
    icSlider*           m_slidMusicVol;

    icSoundI*           m_pClick;
    icSoundI*           m_pHover;
};



#endif //__SETTINGS_MENU_STATE_H__