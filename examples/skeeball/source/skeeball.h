#ifndef __SKEE_BALL_H__
#define __SKEE_BALL_H__

//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "ice/audio/sound_al.h"
#include "ice/platform/application.h"
#include <atomic>
#include <thread>

#include "ice/assets/content_loader.h"

#include "arcade_grade.h"
#include "arcade_ui.h"
#include "ice/render/gl/font_gl.h"

#include "ice/util/settings_file.h"
#include "ice/util/state_machine.h"
#include "ice/util/time.h"

//////////////////////////////////////////////////////////////////////////////
// LOCAL INCLUDES
#include "gamestate.h"
#include "ice/net/companion_link.h"
#include "ice/net/companion_web.h"
#include "ice/util/join_qr.h"
#include "last_game.h"
#include "main_menu.h"
#include "pause_menu.h"
#include "settings.h"
#include "settings_menu.h"

#ifdef WIIMOTE
#include "wiimote.h"
#endif

#ifdef WIIMOTE
#define CONSOLE_OUTPUT

#define _SETCOL(flags) SetConsoleTextAttribute(console, flags)

#define COLWHITE _SETCOL(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define BRIGHT_WHITE                                                                               \
	_SETCOL(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define red _SETCOL(FOREGROUND_RED)
#define BRIGHT_RED _SETCOL(FOREGROUND_RED | FOREGROUND_INTENSITY)
#define blue _SETCOL(FOREGROUND_BLUE)
#define BRIGHT_BLUE _SETCOL(FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define green _SETCOL(FOREGROUND_GREEN)
#define BRIGHT_GREEN _SETCOL(FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define yellow _SETCOL(FOREGROUND_RED | FOREGROUND_GREEN)
#define BRIGHT_YELLOW _SETCOL(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define CYAN _SETCOL(FOREGROUND_BLUE | FOREGROUND_GREEN)
#define BRIGHT_CYAN _SETCOL(FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define PURPLE _SETCOL(FOREGROUND_RED | FOREGROUND_BLUE)
#define BRIGHT_PURPLE _SETCOL(FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY)

#define BLANK_LINE                                                                                 \
	_T("                                                                             \n")

#endif // WIIMOTE console helpers

/*! This is the SkeeBall game
 **/
class SkeeBall : public ice::platform::application {
public:
	//////////////////////////////////////////////////////////////////////////
	// LIFECYCLE
	SkeeBall(void);
	~SkeeBall(void);

	ice::result cleanup(void);

	ice::result init(int argc, char *args[]);

	//////////////////////////////////////////////////////////////////////////
	// OPERATIONS
	ice::result            update(void);
	ice::result            render(void);
	ice::result            render_frame(bool present);
	void                   present_frame();
	ice::platform::window *get_main_window() const { return main_win_; }
	ice::result            quit(void);

	const SkeeBallSettings &get_settings(void) const;

	void update_settings(const SkeeBallSettings &settings);

	ice::util::state_machine &get_state_machine(void) { return state_machine_; };

	ice::net::companion_link Companion;
	ice::net::companion_web  CompanionWeb;
	bool                     BrowserPanel = false;
	ice::util::join_qr       CompanionQR;
	unsigned                 CompanionRoundSeen = 0;
	void                     toggle_browser();
	unsigned                 CompanionCode    = 0;
	bool                     CompanionFocused = true;
	bool                     CompanionPanel   = false;
	std::string              CompanionAddresses;
	void                     toggle_companion();
	GameState                game_state_; //! Main game state
	MainMenu                 main_menu_;  //! Main menu state
	ArcadeUI                 ui;
	SettingsMenu             settings_menu_; //! Settings menu state
	PauseMenu                pause_menu_;    //! Pause menu state
	LastGame                 last_game_;

#ifdef WIIMOTE
	// create a wiimote object
	wiimote remote;
#endif

private:
	ice::result wii_mote_loop(void *);

	void process_settings(void);

	volatile bool    wiimote;
	std::atomic_bool playing{false};

	float                        net_timer_;
	ice::assets::content_loader *content_;

	int last_x_;
	int last_y_;

	ice::util::state_machine state_machine_; //! The game state manager

	ice::util::timer game_time_; //! The time keeper

	ArcadeGrade              grade_;
	ice::render::gl::font_gl font_;
	ice::render::gl::font_gl pair_font_;

	ice::platform::window *main_win_; //! Main window

	std::jthread pthread_;

#ifdef WIIMOTE
	HANDLE console;
#endif

	ice::audio::sound_device_al sound_device_;      //! Audio device
	ice::audio::sound_i        *back_ground_music_; //! Background music object
	ice::audio::sound_listener  listener_;          //! Devices listener for 3d sound

	SkeeBallSettings settings_;         //! Current settings
	SkeeBallSettings initial_settings_; //! Settings when loaded

	ice::util::settings_file settings_file_;
};

/*! Get the current settings
 *
 *  @returns        const SkeeBallSettings&      Reference to current settings
 **/
FORCE_INLINE const SkeeBallSettings &SkeeBall::get_settings(void) const {
	return settings_;
} // END FUNCTION GetSettings(void) const

/*! Updates the game/engine settings
 *
 *  @param          settings        Current settings structure
 **/
FORCE_INLINE void SkeeBall::update_settings(const SkeeBallSettings &settings) {
	settings_ = settings;

	if (!settings_.bMusic)
		back_ground_music_->stop();

	ice::audio::sound_param params = back_ground_music_->get_params();
	params.vol                     = settings_.fMusic_vol;
	back_ground_music_->set_params(params);

} // END FUNCTION UpdateSettings(const SkeeBallSettings& settings)

#endif //__SKEE_BALL_H__