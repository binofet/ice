#ifndef WIN32
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif

#include "airhockey.h"

#include "ice/render/gl/graphics_device.h"
// Cross-platform input
#if 1
#include "ice/input/input.h"
#endif

#ifdef __APPLE__
#include <OpenGL/glu.h>
#include <netdb.h>

#elif defined(WIN32)
#include <GL/glu.h>
#else
#include <GL/glu.h>
#endif

/*! c'tor
 **/
Airhockey::Airhockey(void) {
	main_win_          = NULL;
	content_           = NULL;
	font_              = NULL;
	intro_clip_        = NULL;
	back_ground_music_ = NULL;

	i_game_mode_ = GAME_MODE_SINGLE;

	net_timer_ = 0.016f;
	memset(&initial_settings_, 0, sizeof(initial_settings_));
} // END FUNCTION Airhockey(void)

/*!d'tor
 **/
Airhockey::~Airhockey(void) {
	if (ICEFAIL(cleanup()))
		ice::warning("Airhockey did not cleanup properly");
} // END FUNCTION ~Airhockey(void)

/*! Frees all allocated memory
 *
 *  @returns        ICRESULT        Success/failure code of cleanup
 **/
ice::result Airhockey::cleanup(void) {
	if (!main_win_ && !gx_device_ && !content_)
		return IC_OK;
	sound_device_.cleanup();
	if (font_) {
		gx_device_->release_font(font_);
		font_ = nullptr;
	}
	if (content_) {
		delete content_;
		content_ = nullptr;
	}
	if (gx_device_)
		SAFEDEL(gx_device_);
	if (main_win_)
		SAFEDEL(main_win_);

	// write to settings.ini
	// if (m_InitialSettings != m_Settings)
	{
		std::string temps;
		char        tempb[256];

		// clear current settings
		settings_file_.clear_values();

		// GET SCREEN AND BACK BUFFER RESOLUTION
		temps = (settings_.bFullscreen) ? "true" : "false";
		settings_file_.add_pair("Engine.Graphics.bFullScreen", temps.c_str());
		sprintf(tempb, "%i", settings_.iScreen_x);
		settings_file_.add_pair("Engine.Graphics.ScreenX", tempb);
		sprintf(tempb, "%i", settings_.iScreen_y);
		settings_file_.add_pair("Engine.Graphics.ScreenY", tempb);
		// sprintf(tempb,"%i",m_Settings.iBackbuf_x);
		// m_SettingsFile.AddPair("Engine.Graphics.BackBufferX", tempb);
		// sprintf(tempb,"%i",m_Settings.iBackbuf_y);
		// m_SettingsFile.AddPair("Engine.Graphics.BackBufferY", tempb);

		// GET FOG SETTINGS
		temps = (settings_.bFog) ? "true" : "false";
		settings_file_.add_pair("Engine.Fog.bEnable", temps.c_str());
		sprintf(tempb, "%f", settings_.fogParams.start);
		settings_file_.add_pair("Engine.Fog.fStart", tempb);
		sprintf(tempb, "%f", settings_.fogParams.end);
		settings_file_.add_pair("Engine.Fog.fEnd", tempb);
		sprintf(tempb, "%f", settings_.fogParams.density);
		settings_file_.add_pair("Engine.Fog.fDensity", tempb);
		sprintf(tempb, "(%u,%u,%u,%u)", settings_.fogParams.color.r, settings_.fogParams.color.g,
		        settings_.fogParams.color.b, settings_.fogParams.color.a);
		settings_file_.add_pair("Engine.Fog.Color", tempb);
		sprintf(tempb, "%d", settings_.fogParams.type);
		settings_file_.add_pair("Engine.Fog.ucType", tempb);

		// GET AUDIO SETTINGS
		temps = (settings_.bMusic) ? "true" : "false";
		settings_file_.add_pair("Game.Audio.bEnableMusic", temps.c_str());
		temps = (settings_.bSfx) ? "true" : "false";
		settings_file_.add_pair("Game.Audio.bEnableSfx", temps.c_str());
		sprintf(tempb, "%f", settings_.fMusic_vol);
		settings_file_.add_pair("Game.Audio.fMusicVol", tempb);
		sprintf(tempb, "%f", settings_.fSfx_vol);
		settings_file_.add_pair("Game.Audio.fSfxVol", tempb);

		// GET PADDLE SETTINGS
		sprintf(tempb, "%d", settings_.ucPaddle_type);
		settings_file_.add_pair("Game.Paddle.ucType", tempb);

		// GET NETWORK SETTINGS
		settings_file_.add_pair("Game.Network.sConnectIP", settings_.sLast_join_ip.c_str());

		settings_file_.write();
	}

	ice::printf("Airhockey::Cleanup succeeded");

	return IC_OK;
} // END FUNCTION Cleanup(void)

/*! Initialised the airhockey game
 *
 *  @param          argc            Number of commandl-ine arguments
 *  @param          args            Command-line arguments
 *  @returns        ICRESULT        Success/failure of initialization
 **/
ice::result Airhockey::init(int argc, char *args[]) {
	ice::result res = IC_OK;

	// Initialize Airhockey Settings
	process_settings();

	// Local play must not depend on DNS resolving the machine's hostname.
	char temp[256] = "127.0.0.1";
	char hostname[256];
	if (gethostname(hostname, sizeof(hostname)) == 0) {
		hostent *host = gethostbyname(hostname);
		if (host && host->h_addr_list[0]) {
			in_addr addr;
			memcpy(&addr, host->h_addr_list[0], sizeof(addr));
			snprintf(temp, sizeof(temp), "%s", inet_ntoa(addr));
		}
	}

	Socket.set_home_address(AIRPORT, temp);
	Socket.set_dest_address(AIRPORT, settings_.sLast_join_ip.c_str());

	// create the main window
#if defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW32__)
	ice::uint winFlags = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
	// uint winFlags = WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
#elif defined __APPLE__
	ice::uint winFlags = 0;
#else
	ice::uint winFlags = 0;
#endif

	ice::platform::window_def window = {winFlags,
	                                    static_cast<short>(settings_.iScreen_x),
	                                    static_cast<short>(settings_.iScreen_y),
	                                    0,
	                                    0,
	                                    "Airhockey",
	                                    settings_.bFullscreen};
	main_win_                        = ice::platform::window::get_window(window);

	// create the graphics device
	gx_device_ = new ice::render::gl::graphics_device();
	// m_pGxDevice = new icGXDeviceDX9();

	// check that we got the goods
	if (!main_win_ || !gx_device_) {
#ifdef DEBUG_INSTALL
		MessageBoxA(0, "Failed to window or engine", "Error", 0);
#endif
		return IC_FAIL_GEN;
	}

	// initialize the graphics device
	res = gx_device_->init(main_win_);
	if (ICEFAIL(res))
		return res;

	gx_device_->enable_fog(settings_.bFog, settings_.fogParams);

	// Set the clear color to black
	gx_device_->set_clear_color(ice::math::color::black);

	// show the main window
	main_win_->show();

	// initialize the sound engine
	res = sound_device_.initialize();
	if (ICEFAIL(res)) {
#ifdef DEBUG_INSTALL
		MessageBoxA(0, "Failed to init audio device", "Error", 0);
#endif
		return res;
	}

	// Initialize the content manager
	content_ = new ice::assets::content_loader(gx_device_, &sound_device_);

	ice::audio::sound_param params;
	params.loop   = false;
	params.paused = false;
	params.pitch  = 1.0f;
	params.vol    = settings_.fSfx_vol;

	// m_Content->Load("resource/audio/welcome_to_my_domain.wav",&m_IntroClip);
	content_->load("resource/audio/insane_laughter.wav", &intro_clip_);
	intro_clip_->set_params(params);

	params.loop = true;
	params.vol  = settings_.fMusic_vol;
	content_->load("resource/audio/09 Lateralus.wav", &back_ground_music_);
	back_ground_music_->set_params(params);

	// play intro audio
	if (settings_.bSfx)
		intro_clip_->play();

	// start the background music
	if (settings_.bMusic)
		back_ground_music_->play();

	if (ICEFAIL(res)) {
#ifdef DEBUG_INSTALL
		MessageBoxA(0, "some other shit failed", "Error", 0);
#endif
		return res;
	}

	// push the main menu (TODO, goes to gamestate right now)
	if (ICEFAIL(res = game_state_.init(content_)))
		return res;
	if (ICEFAIL(res = spin_state_.init(content_)))
		return res;
	if (ICEFAIL(res = main_menu_.init(content_)))
		return res;
	if (ICEFAIL(res = settings_menu_.init(content_)))
		return res;
	if (ICEFAIL(res = net_menu_.init(content_)))
		return res;
	if (ICEFAIL(res = pause_menu_.init(content_)))
		return res;
	state_machine_.push_state(&spin_state_);
	state_machine_.push_state(&main_menu_);

	// start keeping time
	game_time_.elapsed();

	ice::render::font_params fontParams = {"Arial", 24};
	gx_device_->create_font(&font_, &fontParams);
	// m_font.Initialize("Arial",24,m_Content);

	if (!ICEFAIL(res))
		return ice::platform::application::init(argc, args);

	return res;
} // END FUNCTION Init(int argc, char* args[])

/*! Updates the airhockey game
 *
 *  @param          ICRESULT        Success/failure of update
 **/
ice::result Airhockey::update(void) {
	ice::result res = IC_OK;

	// current game time
	ice::math::real elapsed = game_time_.elapsed();
	elapsed                 = ice::math::clamp(elapsed, 0.0f, 0.3f);

	// this is a network game, read shit
	if (game_state_.GameMode == GAME_MODE_HOST || game_state_.GameMode == GAME_MODE_CLIENT) {
		int  read = 0;
		char temp[256];
		Socket.read(temp, &read);
		if (read) {
			ReceivePacket = *((_airpacket *)temp);
		}
	}

	// current input
#if 1
	ice::input::input *pInput = ice::input::input::instance();
#endif

	// update the graphics device
	res = gx_device_->update();

	// update the game
	res |= state_machine_.update(elapsed);

#if 1
	pInput->update();
#endif

	net_timer_ -= elapsed;

	if (net_timer_ <= 0.0f) {
		// this is a network game, send shit
		if (game_state_.GameMode == GAME_MODE_HOST || game_state_.GameMode == GAME_MODE_CLIENT) {
			if (ICEOK(Socket.send_packet((char *)&SendPacket, sizeof(_airpacket)))) {
				int a = 0;
			}
		}
		net_timer_ = 0.016f;
	}

	return res;
} // END FUNCTION Update(void)

/*! Renders the airhockey game
 *
 *  @param          ICRESULT        Success/failure of render
 **/
ice::result Airhockey::render(void) { return render_frame(true); }
void        Airhockey::present_frame() { gx_device_->frame_end(true); }
ice::result Airhockey::render_frame(bool present) {
	ice::result res = IC_OK;
	gx_device_->frame_begin();

	// render the game state
	res = state_machine_.render();

	gx_device_->set_projection(gx_device_->get_ortho());
	ice::math::mat4 temp;
	(temp) = ice::math::mat4(1);
	gx_device_->set_world_transform(temp);
	gx_device_->set_view_matrix(temp);
	ice::input::input *input = ice::input::input::instance();

	// static int state = 0;
	// if (input->IsDown(ice::input::ICMOUSE_L))
	//{
	//     state = 1;
	// }
	// else if (input->IsPressed(ice::input::ICMOUSE_L))
	//{
	//     if (state == 2)
	//         state = 3;
	//     if (state == 1)
	//         state = 2;
	//     if (state == 0)
	//         state = 4;
	// }
	// else
	//{
	//     //if (state == 2)
	//         state = 0;
	// }

	// m_pGxDevice->EnableDepthTest(false);
	// switch (state)
	//{
	// case 0: m_font.Print(10,10,icColor::WHITE,"MOUSE UP"); break;
	// case 1: m_font.Print(10,10,icColor::WHITE,"MOUSE DOWN"); break;
	// case 2: m_font.Print(10,10,icColor::WHITE,"MOUSE PRESSED"); break;
	// case 3: m_font.Print(10,10,icColor::WHITE,"MOUSE FUCKING SHIT DOUBLE CLICK"); break;
	// case 4: m_font.Print(10,10,icColor::WHITE,"MOUSE FUCKING SHIT NO DOWN FIRST"); break;
	// }

	// icReal dx = input->GetAxis(ICAXIS_MOUSE_X);
	// icReal dy = input->GetAxis(ICAXIS_MOUSE_Y);

	// m_font.Printf(10,30,icColor::WHITE,"Delta X: %lf", dx);
	// m_font.Printf(10,50,icColor::WHITE,"Delta Y: %lf", dy);
	// m_pGxDevice->EnableDepthTest(true);

	if (present)
		present_frame();
	return res;
} // END FUNCTION Render(void)

/*! Quits the airhockey game
 *
 *  @param          ICRESULT        Success/failure of quit
 **/
ice::result Airhockey::quit(void) {
	if (main_win_)
		main_win_->request_close();
	// should probably do some clean-up and what have you here.
	// still not sure about the existance of this function. . .
	// we'll see. (was thinking it could be called anywere, like exit(0)
	// without being as bad of a hack)
	//
	//  - joel
	return IC_OK;
} // END FUNCTION Quit(void)

/*! Process settings
 **/
void Airhockey::process_settings(void) {
	// get settings file opened
	if (ICEFAIL(settings_file_.set_file("settings.ini")))
		return;

	std::string val;
	std::string temp;
	char        tbuff[256];

	// make sure the settins file read ok
	if (ICEOK_NOWARN(settings_file_.read())) {
		// GET SCREEN AND BACK BUFFER RESOLUTION
		val                   = settings_file_.get_value("Engine.Graphics.bFullScreen", "false");
		settings_.bFullscreen = (strcmp(val.c_str(), "true") == 0) ? true : false;

		val                 = settings_file_.get_value("Engine.Graphics.ScreenX", "1280");
		settings_.iScreen_x = atoi(val.c_str());

		val                 = settings_file_.get_value("Engine.Graphics.ScreenY", "720");
		settings_.iScreen_y = atoi(val.c_str());

		// val = m_SettingsFile.GetValue("Engine.Graphics.BackBufferX", "1280");
		// m_Settings.iBackbuf_x = atoi(val.c_str());

		// val = m_SettingsFile.GetValue("Engine.Graphics.BackBufferY", "720");
		// m_Settings.iBackbuf_y = atoi(val.c_str());

		// GET FOG SETTINGS
		val            = settings_file_.get_value("Engine.Fog.bEnable", "true");
		settings_.bFog = (strcmp(val.c_str(), "true") == 0) ? true : false;

		val                       = settings_file_.get_value("Engine.Fog.fStart", "20.000000");
		settings_.fogParams.start = (float)atof(val.c_str());

		val                     = settings_file_.get_value("Engine.Fog.fEnd", "125.000000");
		settings_.fogParams.end = (float)atof(val.c_str());

		val                         = settings_file_.get_value("Engine.Fog.fDensity", "0.000001");
		settings_.fogParams.density = (float)atof(val.c_str());

		val = settings_file_.get_value("Engine.Fog.Color", "(240,156,46,255)");
		sscanf(val.c_str(), "(%hhu,%hhu,%hhu,%hhu)", &settings_.fogParams.color.r,
		       &settings_.fogParams.color.g, &settings_.fogParams.color.b,
		       &settings_.fogParams.color.a);

		val                      = settings_file_.get_value("Engine.Fog.ucType", "1");
		settings_.fogParams.type = (ice::uchar)atoi(val.c_str());

		// GET AUDIO SETTINGS
		val              = settings_file_.get_value("Game.Audio.bEnableMusic", "true");
		settings_.bMusic = (strcmp(val.c_str(), "true") == 0) ? true : false;

		val            = settings_file_.get_value("Game.Audio.bEnableSfx", "true");
		settings_.bSfx = (strcmp(val.c_str(), "true") == 0) ? true : false;

		val                  = settings_file_.get_value("Game.Audio.fMusicVol", "0.75");
		settings_.fMusic_vol = (float)atof(val.c_str());

		val                = settings_file_.get_value("Game.Audio.fSfxVol", "0.5");
		settings_.fSfx_vol = (float)atof(val.c_str());

		// GET PADDLE SETTINGS
		val                     = settings_file_.get_value("Game.Paddle.ucType", "1");
		settings_.ucPaddle_type = (ice::uchar)atoi(val.c_str());

		// GET NETWORK SETTINGS
		settings_.sLast_join_ip = settings_file_.get_value("Game.Network.sConnectIP", "127.0.0.1");

		initial_settings_ = settings_;
	}
} // END FUNCTION ProcessSettings(void)