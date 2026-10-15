#ifndef __AIRHOCKEY_H__
#define __AIRHOCKEY_H__

//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "ice/audio/sound_al.h"
#include "ice/platform/application.h"

#include "ice/assets/content_loader.h"

#include "ice/render/font.h"

#include "ice/util/settings_file.h"
#include "ice/util/state_machine.h"
#include "ice/util/time.h"

#include "ice/net/socket.h"

//////////////////////////////////////////////////////////////////////////////
// LOCAL INCLUDES
#include "airhockey_settings.h"
#include "gamestate.h"
#include "main_menu.h"
#include "net_menu.h"
#include "pause_menu.h"
#include "settings_menu.h"
#include "spinstate.h"

#define AIRPORT 24220
#define AIRPACKET 0x42

// #define DEBUG_INSTALL

struct _airpacket {
	_airpacket() { type = AIRPACKET; }
	ice::uchar      type;
	ice::math::vec2 puck_pos;
	ice::math::vec2 paddle_pos;
	ice::math::vec2 paddle_vel;
	ice::math::real puck_ang;
	ice::uchar      score[2];
};

/*! This is the airhockey game
 **/
class Airhockey : public ice::platform::application {
public:
	ice::platform::window *get_main_window() const { return main_win_; }
	//////////////////////////////////////////////////////////////////////////
	// LIFECYCLE
	Airhockey(void);
	~Airhockey(void);

	ice::result cleanup(void);

	ice::result init(int argc, char *args[]);

	//////////////////////////////////////////////////////////////////////////
	// OPERATIONS
	ice::result update(void);
	ice::result render(void);
	ice::result render_frame(bool present);
	void        present_frame();
	ice::result quit(void);

	const AirSettings &get_settings(void) const;

	void update_settings(const AirSettings &settings);

	ice::util::state_machine &get_state_machine(void) { return state_machine_; };

	GameState    game_state_;    //! Main game state
	SpinState    spin_state_;    //! Spins the table under the menu
	MainMenu     main_menu_;     //! Main menu state
	NetMenu      net_menu_;      //! Network menu state
	SettingsMenu settings_menu_; //! Settings menu state
	PauseMenu    pause_menu_;    //! Pause menu state

	int              i_game_mode_;
	ice::net::socket Socket;
	_airpacket       SendPacket;
	_airpacket       ReceivePacket;

private:
	void process_settings(void);

	float                        net_timer_;
	ice::assets::content_loader *content_;

	ice::util::state_machine state_machine_; //! The game state manager

	ice::util::timer game_time_; //! The time keeper

	ice::render::font *font_;

	ice::platform::window *main_win_; //! Main window

	ice::audio::sound_device_al sound_device_;      //! Audio device
	ice::audio::sound_i        *back_ground_music_; //! Background music object
	ice::audio::sound_i        *intro_clip_;        //! Audio Clip
	ice::audio::sound_listener  listener_;          //! Devices listener for 3d sound

	AirSettings settings_;         //! Current settings
	AirSettings initial_settings_; //! Settings when loaded

	ice::util::settings_file settings_file_;
};

/*! Get the current settings
 *
 *  @returns        const AirSettings&      Reference to current settings
 **/
FORCE_INLINE const AirSettings &Airhockey::get_settings(void) const {
	return settings_;
} // END FUNCTION GetSettings(void) const

/*! Updates the game/engine settings
 *
 *  @param          settings        Current settings structure
 **/
FORCE_INLINE void Airhockey::update_settings(const AirSettings &settings) {
	settings_ = settings;

	if (!settings_.bMusic)
		back_ground_music_->stop();

	ice::audio::sound_param params = back_ground_music_->get_params();
	params.vol                     = settings_.fMusic_vol;
	back_ground_music_->set_params(params);

	if (!settings_.bSfx)
		intro_clip_->stop();

	params     = intro_clip_->get_params();
	params.vol = settings_.fSfx_vol;
	intro_clip_->set_params(params);

} // END FUNCTION UpdateSettings(const AirSettings& settings)

#endif //__AIRHOCKEY_H__