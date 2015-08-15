#ifndef __AIRHOCKEY_H__
#define __AIRHOCKEY_H__


//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "Core/Application/icApplication.h"
#include "Audio/icSoundAL.h"

#include "Content/icContentLoader.h"

#include "Render/Font/icFont.h"

#include "Util/icSettingsFile.h"
#include "Util/icStateMachine.h"
#include "Util/icTime.h"

#include "Network/icSocket.h"

//////////////////////////////////////////////////////////////////////////////
// LOCAL INCLUDES
#include "gamestate.h"
#include "spinstate.h"
#include "main_menu.h"
#include "net_menu.h"
#include "pause_menu.h"
#include "settings_menu.h"
#include "airhockey_settings.h"

#define AIRPORT     24220
#define AIRPACKET   0x42

//#define DEBUG_INSTALL

struct _airpacket
{
    _airpacket()
    {
        type = AIRPACKET;
    }
    uchar  type;
    icVector2 puck_pos;
    icVector2 paddle_pos;
    icVector2 paddle_vel;
    icReal puck_ang;
    uchar score[2];
};

/*! This is the airhockey game
**/
class Airhockey : public icApplication
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    Airhockey(void);
    ~Airhockey(void);

    ICRESULT Cleanup(void);

    ICRESULT Init(int argc, char* args[]);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    ICRESULT Update(void);
    ICRESULT Render(void);
    ICRESULT Quit(void);

    const AirSettings& GetSettings(void) const;

    void UpdateSettings(const AirSettings& settings);


    icStateMachine& GetStateMachine(void)
    {
        return m_StateMachine;
    };

    GameState           m_GameState;        //! Main game state
    SpinState           m_SpinState;        //! Spins the table under the menu
    MainMenu            m_MainMenu;         //! Main menu state
    NetMenu             m_NetMenu;          //! Network menu state
    SettingsMenu        m_SettingsMenu;     //! Settings menu state
    PauseMenu           m_PauseMenu;        //! Pause menu state

    int                 m_iGameMode;
    icSocket            Socket;
    _airpacket          SendPacket;
    _airpacket          ReceivePacket;

private:
    void ProcessSettings(void);

    float m_fNetTimer;
    icContentLoader*    m_Content;

    icStateMachine      m_StateMachine;     //! The game state manager

    icTimer             m_GameTime;         //! The time keeper

    icFontI*            m_pFont;

    icWindow*           m_pMainWin;         //! Main window

    icSoundDeviceAL     m_SoundDevice;      //! Audio device
    icSoundI*           m_BackGroundMusic;  //! Background music object
    icSoundI*           m_IntroClip;        //! Audio Clip
    icSoundListener     m_Listener;         //! Devices listener for 3d sound

    AirSettings         m_Settings;         //! Current settings
    AirSettings         m_InitialSettings;  //! Settings when loaded

    icSettingsFile      m_SettingsFile;
};


/*! Get the current settings
 *
 *  @returns        const AirSettings&      Reference to current settings
**/
FORCE_INLINE const AirSettings& Airhockey::GetSettings(void) const
{
    return m_Settings;
}// END FUNCTION GetSettings(void) const


/*! Updates the game/engine settings
 *
 *  @param          settings        Current settings structure
**/
FORCE_INLINE void Airhockey::UpdateSettings(const AirSettings& settings)
{
    m_Settings = settings;

    if (!m_Settings.bMusic)
        m_BackGroundMusic->Stop();

    icSoundParam params = m_BackGroundMusic->GetParams();
    params.fVol = m_Settings.fMusic_vol;
    m_BackGroundMusic->SetParams(params);

    if (!m_Settings.bSfx)
        m_IntroClip->Stop();

    params = m_IntroClip->GetParams();
    params.fVol = m_Settings.fSfx_vol;
    m_IntroClip->SetParams(params);

}// END FUNCTION UpdateSettings(const AirSettings& settings)

#endif //__AIRHOCKEY_H__