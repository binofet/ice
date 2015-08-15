#ifndef __SKEE_BALL_H__
#define __SKEE_BALL_H__


//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "Core/Application/icApplication.h"
#include "Core/Threading/icThread.h"
#include "Audio/icSoundAL.h"

#include "Content/icContentLoader.h"

#include "Render/Font/icFontGL.h"

#include "Util/icSettingsFile.h"
#include "Util/icStateMachine.h"
#include "Util/icTime.h"


//////////////////////////////////////////////////////////////////////////////
// LOCAL INCLUDES
#include "gamestate.h"
#include "main_menu.h"
#include "pause_menu.h"
#include "settings_menu.h"
#include "settings.h"
#include "last_game.h"

#include "wiimote.h"

#define WIIMOTE
#define CONSOLE_OUTPUT



#define _SETCOL(flags) SetConsoleTextAttribute(console, flags)

#define COLWHITE			_SETCOL(FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE)
#define BRIGHT_WHITE	_SETCOL(FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY)
#define RED				_SETCOL(FOREGROUND_RED)
#define BRIGHT_RED		_SETCOL(FOREGROUND_RED|FOREGROUND_INTENSITY)
#define BLUE			_SETCOL(FOREGROUND_BLUE)
#define BRIGHT_BLUE		_SETCOL(FOREGROUND_BLUE|FOREGROUND_INTENSITY)
#define GREEN			_SETCOL(FOREGROUND_GREEN)
#define BRIGHT_GREEN	_SETCOL(FOREGROUND_GREEN|FOREGROUND_INTENSITY)
#define YELLOW			_SETCOL(FOREGROUND_RED|FOREGROUND_GREEN)
#define BRIGHT_YELLOW	_SETCOL(FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_INTENSITY)
#define CYAN			_SETCOL(FOREGROUND_BLUE|FOREGROUND_GREEN)
#define BRIGHT_CYAN		_SETCOL(FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY)
#define PURPLE			_SETCOL(FOREGROUND_RED|FOREGROUND_BLUE)
#define BRIGHT_PURPLE	_SETCOL(FOREGROUND_RED|FOREGROUND_BLUE|FOREGROUND_INTENSITY)

#define BLANK_LINE \
 _T("                                                                             \n")


/*! This is the SkeeBall game
**/
class SkeeBall : public icApplication
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    SkeeBall(void);
    ~SkeeBall(void);

    ICRESULT Cleanup(void);

    ICRESULT Init(int argc, char* args[]);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    ICRESULT Update(void);
    ICRESULT Render(void);
    ICRESULT Quit(void);

    const SkeeBallSettings& GetSettings(void) const;

    void UpdateSettings(const SkeeBallSettings& settings);


    icStateMachine& GetStateMachine(void)
    {
        return m_StateMachine;
    };

    GameState           m_GameState;        //! Main game state
    MainMenu            m_MainMenu;         //! Main menu state
    SettingsMenu        m_SettingsMenu;     //! Settings menu state
    PauseMenu           m_PauseMenu;        //! Pause menu state
    LastGame            m_LastGame;

#ifdef WIIMOTE
    // create a wiimote object
	wiimote remote;
#endif

private:
    ICRESULT WiiMoteLoop(void*);

    void ProcessSettings(void);


    volatile bool wiimote;
    volatile bool playing;


    float m_fNetTimer;
    icContentLoader*    m_Content;

    int                 m_lastX;
    int                 m_lastY;

    icStateMachine      m_StateMachine;     //! The game state manager

    icTimer             m_GameTime;         //! The time keeper

    icFontGL            m_font;

    icWindow*           m_pMainWin;         //! Main window

    icThread*           m_pthread;

    HANDLE console;

    icSoundDeviceAL     m_SoundDevice;      //! Audio device
    icSoundI*           m_BackGroundMusic;  //! Background music object
    icSoundListener     m_Listener;         //! Devices listener for 3d sound

    SkeeBallSettings         m_Settings;         //! Current settings
    SkeeBallSettings         m_InitialSettings;  //! Settings when loaded

    icSettingsFile      m_SettingsFile;
};


/*! Get the current settings
 *
 *  @returns        const SkeeBallSettings&      Reference to current settings
**/
FORCE_INLINE const SkeeBallSettings& SkeeBall::GetSettings(void) const
{
    return m_Settings;
}// END FUNCTION GetSettings(void) const


/*! Updates the game/engine settings
 *
 *  @param          settings        Current settings structure
**/
FORCE_INLINE void SkeeBall::UpdateSettings(const SkeeBallSettings& settings)
{
    m_Settings = settings;

    if (!m_Settings.bMusic)
        m_BackGroundMusic->Stop();

    icSoundParam params = m_BackGroundMusic->GetParams();
    params.fVol = m_Settings.fMusic_vol;
    m_BackGroundMusic->SetParams(params);

}// END FUNCTION UpdateSettings(const SkeeBallSettings& settings)

#endif //__SKEE_BALL_H__