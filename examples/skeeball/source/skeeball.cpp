
#include "skeeball.h"
#include <chrono>
#include <random>

#include "ice/render/gl/graphics_device.h"
#if 1
#include "ice/input/input.h"
#endif

#ifdef __APPLE__
#include <OpenGL/glu.h>
#else
#include <GL/glu.h>
#endif

#ifdef WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define USE_BEEPS_AND_DELAYS // undefine to test library works without them

#ifdef WIIMOTE
void on_state_change(wiimote &remote, state_change_flags changed) {
	// a MotionPlus was detected
	if (changed & MOTIONPLUS_DETECTED) {
		// enable it if there isn't a normal extension plugged into it
		// (MotionPlus devices don't report like normal extensions until
		//  enabled - and then, other extensions attached to it will no longer be
		//  reported (so disable it when you want to access to those again).
		if (remote.ExtensionType == wiimote_state::NONE) {
			bool res = remote.EnableMotionPlus();
			_ASSERT(res);
		}
	} else if (changed & MOTIONPLUS_EXTENSION_CONNECTED) {
		// an extension is connected to the MotionPlus.  We can't read it if the
		//  MotionPlus is currently enabled, so disable it:
		if (remote.MotionPlusEnabled())
			remote.DisableMotionPlus();
	} else if (changed & MOTIONPLUS_EXTENSION_DISCONNECTED) {
		// the extension plugged into the MotionPlus was removed, so enable
		//  the MotionPlus data again:
		if (remote.MotionPlusConnected())
			remote.EnableMotionPlus();
	}
	// extension was just connected:
	else if (changed & EXTENSION_CONNECTED) {
#ifdef USE_BEEPS_AND_DELAYS
		Beep(1000, 200);
#endif
		// switch to a report mode that includes the extension data (we will
		//  loose the IR dot sizes)
		// note: there is no need to set report types for a Balance Board.
		if (!remote.IsBalanceBoard())
			remote.SetReportType(wiimote::IN_BUTTONS_ACCEL_IR_EXT);
	}
	// extension was just disconnected:
	else if (changed & EXTENSION_DISCONNECTED) {
#ifdef USE_BEEPS_AND_DELAYS
		Beep(200, 300);
#endif
		// use a non-extension report mode (this gives us back the IR dot sizes)
		remote.SetReportType(wiimote::IN_BUTTONS_ACCEL_IR);
	}
}
#endif

/*! c'tor
 **/
SkeeBall::SkeeBall(void) {
	main_win_          = NULL;
	content_           = nullptr;
	back_ground_music_ = nullptr;

	last_x_ = 0;
	last_y_ = 0;

	net_timer_ = 0.016f;
	playing    = false;
	wiimote    = false;
	memset(&initial_settings_, 0, sizeof(initial_settings_));
} // END FUNCTION SkeeBall(void)

/*!d'tor
 **/
SkeeBall::~SkeeBall(void) {
	if (ICEFAIL(cleanup()))
		ice::warning("SkeeBall did not cleanup properly");

#ifdef WIIMOTE
	remote.Disconnect();
#endif

#ifdef CONSOLE_OUTPUT
	BRIGHT_WHITE; // for automatic 'press any key to continue' msg
	CloseHandle(console);
#endif
} // END FUNCTION ~SkeeBall(void)

/*! Frees all allocated memory
 *
 *  @returns        ICRESULT        Success/failure code of cleanup
 **/
ice::result SkeeBall::cleanup(void) {
	if (!main_win_ && !gx_device_)
		return IC_OK;
	playing = false;
	if (pthread_.joinable())
		pthread_.join();
	grade_.release();
	sound_device_.cleanup();
	if (content_) {
		delete content_;
		content_ = nullptr;
	}
	if (gx_device_)
		SAFEDEL(gx_device_);
	if (main_win_)
		SAFEDEL(main_win_);

	playing = false;

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

		// GET FREE CAM SETTINGS
		temps = (settings_.bFreeCam) ? "true" : "false";
		settings_file_.add_pair("Game.bFreeCam", temps.c_str());

		// GET WII MOTE SETTINGS
		temps = (settings_.bWiiMote) ? "true" : "false";
		settings_file_.add_pair("Game.bWiiMote", temps.c_str());

		// GET HIGH SCORE
		sprintf(tempb, "%i", settings_.iHighScore);
		settings_file_.add_pair("Game.iHighScore", tempb);

		settings_file_.write();
	}

	ice::printf("SkeeBall::Cleanup succeeded");

	return IC_OK;
} // END FUNCTION Cleanup(void)

/*! Initialised the airhockey game
 *
 *  @param          argc            Number of commandl-ine arguments
 *  @param          args            Command-line arguments
 *  @returns        ICRESULT        Success/failure of initialization
 **/
ice::result SkeeBall::init(int argc, char *args[]) {
	ice::result res = IC_OK;

	// Initialize SkeeBall Settings
	process_settings();
#ifndef WIIMOTE
	settings_.bWiiMote = false;
#endif

#ifdef WIIMOTE
	if (m_Settings.bWiiMote) {
		// spawn console window and attache input/output streams
		AllocConsole();

		console      = GetStdHandle(STD_OUTPUT_HANDLE);
		int   hCrt   = _open_osfhandle((long)console, _O_TEXT);
		FILE *hf_out = _fdopen(hCrt, "w");
		setvbuf(hf_out, NULL, _IONBF, 1);
		*stdout = *hf_out;

		HANDLE handle_in = GetStdHandle(STD_INPUT_HANDLE);
		hCrt             = _open_osfhandle((long)handle_in, _O_TEXT);
		FILE *hf_in      = _fdopen(hCrt, "r");
		setvbuf(hf_in, NULL, _IONBF, 128);
		*stdin = *hf_in;
	}
#endif

	// create the main window
#if defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW32__)
	ice::uint winFlags = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
	// uint winFlags = WS_CAPTION ;
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
	                                    "Skee Ball",
	                                    settings_.bFullscreen};
	main_win_                        = ice::platform::window::get_window(window);

	// create the graphics device
	gx_device_ = new ice::render::gl::graphics_device();

	// check that we got the goods
	if (!main_win_ || !gx_device_) {
#ifdef DEBUG_INSTALL
		MessageBoxA(0, "Failed to window or engine", "Error", 0);
#endif
		return IC_FAIL_GEN;
	}

	// initialize the graphics device
	if (ICEFAIL(res = gx_device_->init(main_win_)))
		return res;

	// m_pGxDevice->EnableFog(m_Settings.bFog, m_Settings.fogParams);

	// Set the clear color to black
	((ice::render::gl::graphics_device *)gx_device_)->set_clear_color(ice::math::color::black);

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
	params.paused = false;
	params.pitch  = 1.0f;
	params.loop   = true;
	params.vol    = settings_.fMusic_vol;
	if (ICEFAIL(res = content_->load("resource/audio/01castlevania.wav", &back_ground_music_)))
		return res;
	back_ground_music_->set_params(params);

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
	if (ICEFAIL(res = ui.init(content_)))
		return res;
	if (ICEFAIL(res = game_state_.init(content_)))
		return res;
	if (ICEFAIL(res = main_menu_.init(content_)))
		return res;
	if (ICEFAIL(res = settings_menu_.init(content_)))
		return res;
	if (ICEFAIL(res = pause_menu_.init(content_)))
		return res;
	if (ICEFAIL(res = last_game_.init(content_)))
		return res;
	state_machine_.push_state(&last_game_);
	state_machine_.push_state(&main_menu_);

	// start keeping time
	game_time_.elapsed();

	font_.initialize("Arial", 24, content_->get_device());
	pair_font_.initialize("Arial", 48, content_->get_device());

	playing = true;

#ifdef WIIMOTE
	if (m_Settings.bWiiMote) {
		m_pthread = std::jthread([this] { WiiMoteLoop(nullptr); });
	}
#endif

	if (!ICEFAIL(res))
		return ice::platform::application::init(argc, args);

	return res;
} // END FUNCTION Init(int argc, char* args[])

#ifdef WIIMOTE
ice::result SkeeBall::WiiMoteLoop(void *) {
	// let's load a couple of samples:
	wiimote_sample sine_sample, daisy_sample;

	// simple callback example (we use polling for almost everything here):
	remote.ChangedCallback = on_state_change;
	//  notify us only when something related to the extension changes
	remote.CallbackTriggerFlags = (state_change_flags)(EXTENSION_CHANGED | MOTIONPLUS_CHANGED);

reconnect:
	COORD pos = {0, 6};
	SetConsoleCursorPosition(console, pos);

	// try to connect the first available wiimote in the system
	//  (available means 'installed, and currently Bluetooth-connected'):
	COLWHITE;
	_tprintf(_T("  Looking for a Wiimote     "));

	static const TCHAR *wait_str[] = {_T(".  "), _T(".. "), _T("...")};
	unsigned            count      = 0;
	while (playing && !remote.Connect(wiimote::FIRST_AVAILABLE)) {
		_tprintf(_T("\b\b\b\b%s "), wait_str[count % 3]);
		count++;
#ifdef USE_BEEPS_AND_DELAYS
		Beep(500, 30);
		Sleep(1000);
#endif
	}

	if (!playing)
		return IC_OK;
	wiimote = true;

	// connected - light all LEDs
	remote.SetLEDs(0x0f);
	BRIGHT_CYAN;
	_tprintf(_T("\b\b\b\b... connected!"));
	if (remote.IsBalanceBoard()) {
		BRIGHT_WHITE;
		_tprintf(_T("  (Balance Board)"));
	}

#ifdef USE_BEEPS_AND_DELAYS
	Beep(1000, 300);
	Sleep(2000);
#endif

	COORD cursor_pos = {0, 6};

	// ask the wiimote to report everything (using the 'non-continous updates'
	//  default mode - updates will be frequent anyway due to the acceleration/IR
	//  values changing):

	// note1: you don't need to set a report type for Balance Board - the library
	//         takes care of it (there is only one).
	// note2: for wiimotes, the report mode that includes the extension data
	//		   unfortunately only reports the 'BASIC' IR info (ie. no dot sizes) -
	//		   so let's choose the best mode based on the extension status (we also
	//		   toggle modes as needed in the callback above):
	if (!remote.IsBalanceBoard()) {
		if (remote.bExtension)
			remote.SetReportType(wiimote::IN_BUTTONS_ACCEL_IR_EXT); // no IR dots
		else
			remote.SetReportType(wiimote::IN_BUTTONS_ACCEL_IR); //    IR dots
	}

	// (stuff for animations)
	DWORD    last_rumble_time = timeGetTime(); // for rumble text animation
	DWORD    last_led_time    = timeGetTime(); // for led         animation
	bool     rumble_text      = true;
	unsigned lit_led          = 0;

	// display the wiimote state data until 'Home' is pressed:
	bool caliubrate_atrest_once = true;
	while (playing) // && !GetAsyncKeyState(VK_ESCAPE))
	{
		// the wiimote state needs to be refreshed for each pass
		while (remote.RefreshState() == NO_CHANGE)
			Sleep(1); // // don't hog the CPU if nothing changed

		cursor_pos.Y = 8;
		SetConsoleCursorPosition(console, cursor_pos);

		// did we loose the connection?
		if (remote.ConnectionLost()) {
			BRIGHT_RED;
			_tprintf(_T("   *** connection lost! ***                                          ")
			         _T("\n") BLANK_LINE BLANK_LINE BLANK_LINE BLANK_LINE BLANK_LINE BLANK_LINE
			             BLANK_LINE BLANK_LINE BLANK_LINE BLANK_LINE BLANK_LINE BLANK_LINE
			                 BLANK_LINE BLANK_LINE BLANK_LINE);
			Beep(100, 1000);
			Sleep(2000);
			COORD pos = {0, 6};
			SetConsoleCursorPosition(console, pos);
			_tprintf(BLANK_LINE BLANK_LINE BLANK_LINE);
			goto reconnect;
		}

		// rumble if 'B' (trigger) is pressed
		remote.SetRumble(remote.Button.B());

		// disable MotionPlus (if connected) to allow its own extension port
		//  to work
		if (remote.Button.Minus())
			remote.DisableMotionPlus();

		// actions for buttons just pressed/released:
		static bool last_A = false, last_One = false, last_Two = false;

#define ON_PRESS_RELEASE(button, pressed_action, released_action)                                  \
	{                                                                                              \
		bool pressed = remote.Button.button();                                                     \
		if (pressed) { /* just pressed? */                                                         \
			if (!last_##button)                                                                    \
				pressed_action;                                                                    \
		} else if (last_##button) /* just released */                                              \
			released_action;                                                                       \
		/* remember the current button state for next time */                                      \
		last_##button = pressed;                                                                   \
	}

		// Battery level:
		CYAN;
		_tprintf(_T("  Battery: "));
		// (the green/yellow colour ranges are rough guesses - my wiimote
		//  with rechargeable battery pack fails at around 15%)
		(remote.bBatteryDrained)        ? BRIGHT_RED
		: (remote.BatteryPercent >= 30) ? BRIGHT_GREEN
		                                : BRIGHT_YELLOW;
		_tprintf(_T("%3u%%   "), remote.BatteryPercent);

		DWORD current_time = timeGetTime();

		// LEDs:
		//  animate them every second
		if ((current_time - last_led_time) >= 1000) {
			remote.SetLEDs((BYTE)(1 << lit_led));
			lit_led       = (++lit_led) % 4;
			last_led_time = timeGetTime();
		}

		CYAN;
		_tprintf(_T("LEDs: "));
		COLWHITE;
		_tprintf(_T("["));
		for (unsigned led = 0; led < 4; led++) {
			if (remote.LED.Lit(led)) {
				BRIGHT_CYAN;
				_tprintf(_T("*"));
			} else {
				COLWHITE;
				_tprintf(_T("-")); //_T("%c"), '0'+led);
			}
		}

		// Rumble
		COLWHITE;
		_tprintf(_T("]    "));
		if (remote.bRumble) {
			BRIGHT_WHITE;
			_tprintf(rumble_text ? _T(" RUMBLE") : _T("RUMBLE "));
			// animate the text
			if ((current_time - last_rumble_time) >= 110) {
				rumble_text      = !rumble_text;
				last_rumble_time = current_time;
			}
		} else
			_tprintf(_T("       "));

		// Output method:
		CYAN;
		_tprintf(_T("    using %s\n"),
		         (remote.IsUsingHIDwrites() ? _T("HID writes") : _T("WriteFile()")));

		// Buttons:
		CYAN;
		_tprintf(_T("\n  Buttons: "));
		COLWHITE;
		_tprintf(_T("["));
		for (unsigned bit = 0; bit < 16; bit++) {
			WORD mask = (WORD)(1 << bit);
			// skip unused bits
			if ((wiimote_state::buttons::ALL & mask) == 0)
				continue;

			const TCHAR *button_name = wiimote::ButtonNameFromBit[bit];
			bool         pressed     = ((remote.Button.Bits & mask) != 0);
			if (bit > 0) {
				CYAN;
				_tprintf(_T("|")); // seperator
			}
			if (pressed) {
				BRIGHT_WHITE;
				_tprintf(_T("%s"), button_name);
			} else {
				COLWHITE;
				_tprintf(_T("%*s"), _tcslen(button_name), _T(""));
			}
		}
		COLWHITE;
		_tprintf(_T("]\n"));

		// Acceleration:
		CYAN;
		_tprintf(_T("    Accel:"));
		remote.IsBalanceBoard() ? RED : COLWHITE;
		_tprintf(_T("  X %+2.3f  Y %+2.3f  Z %+2.3f  \n"), remote.Acceleration.X,
		         remote.Acceleration.Y, remote.Acceleration.Z);

		// Orientation estimate (shown red if last valid update is aging):
		CYAN;
		_tprintf(_T("   Orient:"));
		remote.IsBalanceBoard() ? RED : COLWHITE;
		_tprintf(_T("  UpdateAge %3u  "), remote.Acceleration.Orientation.UpdateAge);

		//  show if the last orientation update is considered out-of-date
		//   (using an arbitrary threshold)
		if (remote.Acceleration.Orientation.UpdateAge > 10)
			RED;

		_tprintf(_T("Pitch:%4ddeg  Roll:%4ddeg  \n")
		         _T("                           (X %+.3f  Y %+.3f  Z %+.3f)      \n"),
		         (int)remote.Acceleration.Orientation.Pitch,
		         (int)remote.Acceleration.Orientation.Roll, remote.Acceleration.Orientation.X,
		         remote.Acceleration.Orientation.Y, remote.Acceleration.Orientation.Z);

		// IR:
		CYAN;
		_tprintf(_T("       IR:"));
		remote.IsBalanceBoard() ? RED : COLWHITE;
		_tprintf(_T("  Mode %s  "),
		         ((remote.IR.Mode == wiimote_state::ir::OFF)        ? _T("OFF  ")
		          : (remote.IR.Mode == wiimote_state::ir::BASIC)    ? _T("BASIC")
		          : (remote.IR.Mode == wiimote_state::ir::EXTENDED) ? _T("EXT. ")
		                                                            : _T("FULL ")));
		// IR dot sizes are only reported in EXTENDED IR mode (FULL isn't supported yet)
		bool dot_sizes = (remote.IR.Mode == wiimote_state::ir::EXTENDED);

		for (unsigned index = 0; index < 4; index++) {
			wiimote_state::ir::dot &dot = remote.IR.Dot[index];

			if (!remote.IsBalanceBoard())
				COLWHITE;
			_tprintf(_T("%u: "), index);

			if (dot.bVisible) {
				COLWHITE;
				_tprintf(_T("Seen       "));
			} else {
				RED;
				_tprintf(_T("Not seen   "));
			}

			_tprintf(_T("Size"));
			if (dot_sizes)
				_tprintf(_T("%3d "), dot.Size);
			else {
				RED;
				_tprintf(_T(" n/a"));
				if (dot.bVisible)
					COLWHITE;
			}

			_tprintf(_T("  X %.3f  Y %.3f\n"), dot.X, dot.Y);

			if (index < 3)
				_tprintf(_T("                        "));
		}

		// Speaker:
		CYAN;
		_tprintf(_T("  Speaker:"));
		remote.IsBalanceBoard() ? RED : COLWHITE;
		_tprintf(_T("  %s | %s    "), (remote.Speaker.bEnabled ? _T("On ") : _T("Off")),
		         (remote.Speaker.bMuted ? _T("Muted") : _T("     ")));
		if (!remote.Speaker.bEnabled || remote.Speaker.bMuted)
			RED;
		else // if(remote.IsPlayingAudio()) BRIGHT_WHITE; else COLWHITE;
			COLWHITE;
		_tprintf(_T("Frequency %4u Hz   Volume 0x%02x\n"), wiimote::FreqLookup[remote.Speaker.Freq],
		         remote.Speaker.Volume);

		// -- Extensions --:
		CYAN;
		_tprintf(_T("__________\n  Extnsn.:  "));
		switch (remote.ExtensionType) {
		case wiimote_state::NONE: {
			RED;
			_tprintf(_T("None                                                             \n"));
			_tprintf(BLANK_LINE BLANK_LINE BLANK_LINE);
		} break;

		case wiimote_state::PARTIALLY_INSERTED: {
			BRIGHT_RED;
			_tprintf(_T("Partially Inserted                                               \n"));
			_tprintf(BLANK_LINE BLANK_LINE BLANK_LINE);
		} break;

			// -- Nunchuk --
		case wiimote_state::NUNCHUK: {
			BRIGHT_WHITE;
			_tprintf(_T("Nunchuk   "));

			// Buttons:
			CYAN;
			_tprintf(_T("Buttons: "));
			COLWHITE;
			_tprintf(_T("["));
			BRIGHT_WHITE;
			_tprintf(remote.Nunchuk.C ? _T("C") : _T(" "));
			CYAN;
			_tprintf(_T("|"));
			BRIGHT_WHITE;
			_tprintf(remote.Nunchuk.Z ? _T("Z") : _T(" "));
			COLWHITE;
			_tprintf(_T("]   "));
			// Joystick:
			CYAN;
			_tprintf(_T("Joystick:  "));
			COLWHITE;
			_tprintf(_T("X %+2.3f  Y %+2.3f\n"), remote.Nunchuk.Joystick.X,
			         remote.Nunchuk.Joystick.Y);
			// Acceleration:
			CYAN;
			_tprintf(_T("    Accel:"));
			COLWHITE;
			_tprintf(_T("  X %+2.3f  Y %+2.3f  Z %+2.3f  \n"), remote.Nunchuk.Acceleration.X,
			         remote.Nunchuk.Acceleration.Y, remote.Nunchuk.Acceleration.Z);

			// Orientation estimate (shown red if last valid update is aging):
			CYAN;
			_tprintf(_T("   Orient:"));
			COLWHITE;
			_tprintf(_T("  UpdateAge %3u  "), remote.Nunchuk.Acceleration.Orientation.UpdateAge);
			//  show if the last orientation update is aging
			if (remote.Nunchuk.Acceleration.Orientation.UpdateAge > 10)
				RED;
			_tprintf(_T("Pitch %4ddeg  Roll %4ddeg  \n")
			         _T("                           (X %+.2f  Y %+.2f  Z %+.2f)      \n"),
			         (int)remote.Nunchuk.Acceleration.Orientation.Pitch,
			         (int)remote.Nunchuk.Acceleration.Orientation.Roll,
			         remote.Nunchuk.Acceleration.Orientation.X,
			         remote.Nunchuk.Acceleration.Orientation.Y,
			         remote.Nunchuk.Acceleration.Orientation.Z);
		} break;

			// -- Classic Controller --
		case wiimote_state::CLASSIC:
		case wiimote_state::GH3_GHWT_GUITAR:
		case wiimote_state::GHWT_DRUMS: {
			BRIGHT_WHITE;
			// the Guitar Hero controller is just a classic controller with
			//  another ID
			if (remote.ExtensionType == wiimote_state::CLASSIC)
				_tprintf(_T("Classic Controller   "));
			else if (remote.ExtensionType == wiimote_state::GH3_GHWT_GUITAR)
				_tprintf(_T("GH3/GHWT Guitar      "));
			else
				_tprintf(_T("GHWT Drums           "));

			// L: Joystick/Trigger
			COLWHITE;
			_tprintf(_T("L:  "));
			CYAN;
			_tprintf(_T("Joy "));
			COLWHITE;
			_tprintf(_T("X %+2.3f  Y %+2.3f  "), remote.ClassicController.JoystickL.X,
			         remote.ClassicController.JoystickL.Y);
			CYAN;
			_tprintf(_T("Trig "));
			COLWHITE;
			_tprintf(_T("%+2.3f\n"), remote.ClassicController.TriggerL);
			// R: Joystick/Trigger
			COLWHITE;
			_tprintf(_T("                                 R:  "));
			CYAN;
			_tprintf(_T("Joy "));
			COLWHITE;
			_tprintf(_T("X %+2.3f  Y %+2.3f  "), remote.ClassicController.JoystickR.X,
			         remote.ClassicController.JoystickR.Y);
			CYAN;
			_tprintf(_T("Trig "));
			COLWHITE;
			_tprintf(_T("%+2.3f\n"), remote.ClassicController.TriggerR);

			// Buttons:
			CYAN;
			_tprintf(_T("  Buttons: "));
			COLWHITE;
			_tprintf(_T("["));
			for (unsigned bit = 0; bit < 16; bit++) {
				WORD mask = (WORD)(1 << bit);
				// skip unused bits
				if ((wiimote_state::classic_controller::buttons::ALL & mask) == 0)
					continue;

				const TCHAR *button_name = wiimote::ClassicButtonNameFromBit[bit];
				const TCHAR *seperator   = (bit == 0) ? _T("") : _T("|");
				bool         pressed     = ((remote.ClassicController.Button.Bits & mask) != 0);
				CYAN;
				_tprintf(seperator);
				if (pressed) {
					BRIGHT_WHITE;
					_tprintf(_T("%s"), button_name);
				} else {
					COLWHITE;
					_tprintf(_T("%*s"), _tcslen(button_name), _T(""));
				}
			}
			COLWHITE;
			_tprintf(_T("]"));
		} break;

		case wiimote_state::BALANCE_BOARD: {
			BRIGHT_WHITE;
			_tprintf(_T("Balance Board"));

			// Weights:
			CYAN;
			_tprintf(_T("   Weight: "));
			COLWHITE;
			_tprintf(_T("TL    "));
			_tprintf(_T("%6.2f"), remote.BalanceBoard.Kg.TopL);
			CYAN;
			_tprintf(_T(" kg"));
			COLWHITE;
			;
			_tprintf(_T("  TR "));
			_tprintf(_T("%6.2f"), remote.BalanceBoard.Kg.TopR);
			CYAN;
			_tprintf(_T(" kg\n"));
			COLWHITE;
			_tprintf(_T("                                    BL    "));
			_tprintf(_T("%6.2f"), remote.BalanceBoard.Kg.BottomL);
			CYAN;
			_tprintf(_T(" kg"));
			COLWHITE;
			_tprintf(_T("  BR "));
			_tprintf(_T("%6.2f"), remote.BalanceBoard.Kg.BottomR);
			CYAN;
			_tprintf(_T(" kg \n"));
			COLWHITE;
			_tprintf(_T("                                    Total "));
			_tprintf(_T("%6.2f"), remote.BalanceBoard.Kg.Total);
			CYAN;
			_tprintf(_T(" kg"));
		} break;

		case wiimote_state::MOTION_PLUS: {
			BRIGHT_WHITE;
			_tprintf(_T("Motion Plus"));

			CYAN;
			_tprintf(_T("    Raw: "));
			COLWHITE;
			_tprintf(_T("Yaw: %04hx  "), remote.MotionPlus.Raw.Yaw);
			COLWHITE;
			_tprintf(_T("Pitch: %04hx  "), remote.MotionPlus.Raw.Pitch);
			COLWHITE;
			_tprintf(_T("Roll: %04hx  \n"), remote.MotionPlus.Raw.Roll);
			CYAN;
			_tprintf(_T("                         Float: "));
			COLWHITE;
			_tprintf(_T("  %8.3fdeg"), remote.MotionPlus.Speed.Yaw);
			COLWHITE;
			_tprintf(_T("  %8.3fdeg"), remote.MotionPlus.Speed.Pitch);
			COLWHITE;
			_tprintf(_T(" %8.3fdeg\n"), remote.MotionPlus.Speed.Roll);
			_tprintf(BLANK_LINE BLANK_LINE);
			// Sleep(500);
		} break;
		}
	}

	// disconnect (auto-happens on wiimote destruction anyway, but let's play nice)
	remote.Disconnect();
	Beep(1000, 200);

	return IC_OK;
}
#endif

/*! Updates the airhockey game
 *
 *  @param          ICRESULT        Success/failure of update
 **/
ice::result SkeeBall::update(void) {
	ice::result res = IC_OK;

	// current game time
	ice::math::real elapsed = game_time_.elapsed();

	// current input
#if 1
	ice::input::input *pInput = ice::input::input::instance();
#endif

	// update the graphics device
	res = gx_device_->update();

	if (pInput->is_pressed(ice::input::key_f2))
		toggle_companion();
	if (pInput->is_pressed(ice::input::key_f3))
		toggle_browser();
	const double now =
	    std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
	const bool gameReady = CompanionFocused && !CompanionPanel &&
	                       state_machine_.peek_state() == &game_state_ &&
	                       game_state_.ready_for_shot() &&
	                       (!BrowserPanel || CompanionWeb.room().state() ==
	                                             ice::input::companion_room::phase::playing);
	Companion.accept_shots(gameReady);
	CompanionWeb.set_game_ready(gameReady);
	CompanionWeb.poll(now);
	Companion.poll(now);
	if (BrowserPanel) {
		const auto &room = CompanionWeb.room();
		if (room.round() != CompanionRoundSeen) {
			CompanionRoundSeen = room.round();
			game_state_.on_push();
			if (state_machine_.peek_state() != &game_state_)
				state_machine_.push_state(&game_state_);
			CompanionPanel = false;
		}
		if (room.state() != ice::input::companion_room::phase::playing)
			CompanionPanel = true;
	} else if (CompanionPanel && Companion.status() == ice::net::companion_link::state::connected)
		CompanionPanel = false;
	// Pause the game while the dedicated pairing screen is visible.
	if (!CompanionPanel)
		res |= state_machine_.update(elapsed);

#ifdef WIIMOTE
	if (m_Settings.bWiiMote) {
		if (remote.IsConnected()) {
			float x   = 0.0f;
			float y   = 0.0f;
			int   num = 0;

			for (unsigned index = 0; index < 4; index++) {
				wiimote_state::ir::dot &dot = remote.IR.Dot[index];

				if (dot.bVisible) {
					x += dot.X;
					y += dot.Y;
					num++;
				}
			}

			int x_pos = 0;
			int y_pos = 0;

			if (!num) {
				x_pos = m_lastX;
				y_pos = m_lastY;
			} else {
				x_pos = (x / (float)num) * m_Settings.iScreen_x;
				y_pos = (y / (float)num) * m_Settings.iScreen_y;
			}

			wiimote_state::ir::dot &dot = remote.IR.Dot[0];

			x_pos = ice::math::clamp((int)(dot.X * (float)m_Settings.iScreen_x), 0,
			                         m_Settings.iScreen_x);
			y_pos = ice::math::clamp((int)(dot.Y * (float)m_Settings.iScreen_y), 0,
			                         m_Settings.iScreen_y);

			ice::input::input::Instance()->UpdateMousePos(x_pos, y_pos);

			m_lastX = x_pos;
			m_lastY = y_pos;
		}
	}
#endif

#if 1
	pInput->update();
#endif

	// icSoundParam param;

	// param.fPitch = 1.0f;
	// param.bLoop = true;
	// param.fVol = m_Settings.fMusic_vol;

	// m_BackGroundMusic->SetParams(param);
	return res;
} // END FUNCTION Update(void)

/*! Renders the airhockey game
 *
 *  @param          ICRESULT        Success/failure of render
 **/
ice::result SkeeBall::render(void) { return render_frame(true); }
void        SkeeBall::present_frame() { gx_device_->frame_end(true); }
ice::result SkeeBall::render_frame(bool present) {
	ice::result res = IC_OK;
	gx_device_->frame_begin();

	// render the game state
	if (!CompanionPanel)
		res = state_machine_.render();
	else {
		glPushAttrib(GL_COLOR_BUFFER_BIT | GL_ENABLE_BIT);
		glDisable(GL_SCISSOR_TEST);
		glClearColor(.025f, .045f, .07f, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glPopAttrib();
	}

	if (!CompanionPanel && state_machine_.peek_state() == &game_state_) {
		int w, h;
		main_win_->get_framebuffer_size(&w, &h);
		grade_.apply(w, h);
	}
	gx_device_->set_projection(gx_device_->get_ortho());
	ice::math::mat4 temp;
	(temp) = ice::math::mat4(1);
	gx_device_->set_world_transform(temp);
	gx_device_->set_view_matrix(temp);

#ifdef WIIMOTE
	// if user has bWiiMote=true in the settings file
	if (m_Settings.bWiiMote) {
		if (wiimote) {
			wiimote_state::ir::dot &dot = remote.IR.Dot[0];

			int x_pos = ice::math::clamp((int)(dot.X * (float)m_Settings.iScreen_x), 0,
			                             m_Settings.iScreen_x);
			int y_pos =
			    ice::math::clamp(m_Settings.iScreen_y - (int)(dot.Y * (float)m_Settings.iScreen_y),
			                     0, m_Settings.iScreen_y);

			// m_font.Print(x_pos,y_pos,icColor::WHITE,"Wii Pos");

			// m_font.Print(70,60,icColor::WHITE,"Wiimote connected");
		} else
			m_font.Print(ice::math::ivec2(10, 50), ice::math::color::WHITE, "Looking for wiimote");
	}
#endif

	if (CompanionPanel) {
		int width, height;
		main_win_->get_framebuffer_size(&width, &height);
		int x = width / 10;
		if (BrowserPanel) {
			std::string address = CompanionAddresses.substr(0, CompanionAddresses.find(" / "));
			pair_font_.print(ice::math::ivec2(x, height * 85 / 100), ice::math::color::white,
			                 "SCAN TO JOIN");
			int module = std::max(3, std::min(width / 2, height * 6 / 10) / 37), side = 37 * module,
			    qx = x, qy = height / 5;
			glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT |
			             GL_DEPTH_BUFFER_BIT);
			glDisable(GL_TEXTURE_2D);
			glDisable(GL_LIGHTING);
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_CULL_FACE);
			glDisable(GL_BLEND);
			glColor3f(1, 1, 1);
			glBegin(GL_QUADS);
			glVertex2i(qx, qy);
			glVertex2i(qx + side, qy);
			glVertex2i(qx + side, qy + side);
			glVertex2i(qx, qy + side);
			glEnd();
			glColor3f(0, 0, 0);
			glBegin(GL_QUADS);
			for (int row = 0; row < 29; ++row)
				for (int col = 0; col < 29; ++col)
					if (CompanionQR.dark[row][col]) {
						int left = qx + (col + 4) * module, bottom = qy + (32 - row) * module;
						glVertex2i(left, bottom);
						glVertex2i(left + module, bottom);
						glVertex2i(left + module, bottom + module);
						glVertex2i(left, bottom + module);
					}
			glEnd();
			glPopAttrib();
			int         rx = width * 55 / 100, y = height * 74 / 100;
			const auto &room = CompanionWeb.room();
			font_.print(ice::math::ivec2(rx, y), ice::math::color::white,
			            room.state() == ice::input::companion_room::phase::results
			                ? "ROUND COMPLETE"
			                : "PLAYERS AT THE TABLE");
			y -= 42;
			for (const auto &player : room.players()) {
				char line[128];
				std::snprintf(line, sizeof(line), "%s%s   %u", player.name.c_str(),
				              player.online ? "" : " (offline)", player.score);
				font_.print(ice::math::ivec2(rx, y), ice::math::color::white, line);
				y -= 36;
			}
			if (room.players().empty())
				font_.print(ice::math::ivec2(rx, y), ice::math::color::white,
				            "Scan with your phone camera.");
			font_.print(ice::math::ivec2(rx, height / 5), ice::math::color::white,
			            "Start the round on the leader's phone.");
			font_.print(ice::math::ivec2(x, height / 9), ice::math::color::white,
			            CompanionWeb.join_url(address).c_str());
			font_.print(ice::math::ivec2(x, height / 18), ice::math::color::white,
			            "Same Wi-Fi. F3: show/hide lobby. F2: close table.");
		} else {
			font_.print(ice::math::ivec2(x, height * 8 / 10), ice::math::color::white,
			            "CONNECT YOUR PHONE");
			font_.print(ice::math::ivec2(x, height * 7 / 10), ice::math::color::white,
			            "Open ICE Companion and tap Connect");
			font_.print(ice::math::ivec2(x, height * 6 / 10), ice::math::color::white,
			            "Desktop address");
			std::string address = CompanionAddresses.substr(0, CompanionAddresses.find(" / "));
			pair_font_.print(ice::math::ivec2(x, height * 5 / 10), ice::math::color::white,
			                 address.c_str());
			char port[64], code[32];
			std::snprintf(port, sizeof(port), "Port     %u", Companion.port());
			std::snprintf(code, sizeof(code), "%06u", CompanionCode);
			font_.print(ice::math::ivec2(x, height * 4 / 10), ice::math::color::white, port);
			font_.print(ice::math::ivec2(x, height * 3 / 10), ice::math::color::white,
			            "Pairing code");
			pair_font_.print(ice::math::ivec2(x, height * 2 / 10), ice::math::color::white, code);
			font_.print(ice::math::ivec2(x, height / 10), ice::math::color::white,
			            "Same Wi-Fi network. F2 to cancel.");
		}
	} else if (state_machine_.peek_state() == &game_state_) {
		font_.print(ice::math::ivec2(12, 18), ice::math::color::white,
		            CompanionCode ? "F3: show lobby  |  F2: disconnect"
		                          : "F2: phone app   F3: browser party");
		if (BrowserPanel) {
			int width, height;
			main_win_->get_framebuffer_size(&width, &height);
			int y = height - 40;
			for (const auto &player : CompanionWeb.room().players()) {
				char line[128];
				std::snprintf(line, sizeof(line), "%s%s  %u  (%u/5)",
				              CompanionWeb.room().current() == player.id ? "> " : "",
				              player.name.c_str(), player.score, player.balls);
				font_.print(ice::math::ivec2(24, y), ice::math::color::white, line);
				y -= 30;
			}
		}
	}
	if (present)
		present_frame();
	return IC_OK;
} // END FUNCTION Render(void)

/*! Quits the airhockey game
 *
 *  @param          ICRESULT        Success/failure of quit
 **/
ice::result SkeeBall::quit(void) {
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
void SkeeBall::process_settings(void) {
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

		initial_settings_ = settings_;

		// GET GAME SETTINGS
		val                = settings_file_.get_value("Game.bFreeCam", "false");
		settings_.bFreeCam = (strcmp(val.c_str(), "true") == 0) ? true : false;

		// GET WIIMOTE SETTINGS
		val                = settings_file_.get_value("Game.bWiiMote", "true");
		settings_.bWiiMote = (strcmp(val.c_str(), "true") == 0) ? true : false;

		// GET HIGH SCORE
		val                  = settings_file_.get_value("Game.iHighScore", "0");
		settings_.iHighScore = atoi(val.c_str());
	}
} // END FUNCTION ProcessSettings(void)
void SkeeBall::toggle_companion() {
	if (CompanionCode) {
		CompanionWeb.close();
		BrowserPanel = false;
		Companion.close();
		CompanionCode      = 0;
		CompanionPanel     = false;
		CompanionRoundSeen = 0;
		return;
	}
	std::random_device random;
	unsigned           code = 100000 + random() % 900000;
	if (Companion.listen(0, code)) {
		CompanionCode      = code;
		CompanionPanel     = true;
		CompanionAddresses = ice::net::companion_link::local_addresses();
		ice::printf("Companion %s port=%u code=%06u", CompanionAddresses.c_str(), Companion.port(),
		            code);
	}
}

void SkeeBall::toggle_browser() {
	if (CompanionWeb.port()) {
		CompanionPanel = !CompanionPanel;
		return;
	}
	if (CompanionCode)
		toggle_companion();
	if (!CompanionCode)
		toggle_companion();
	if (CompanionCode && CompanionWeb.start(Companion.port(), CompanionCode)) {
		BrowserPanel = CompanionPanel = true;
		auto address = CompanionAddresses.substr(0, CompanionAddresses.find(" / "));
		CompanionQR.encode(CompanionWeb.join_url(address));
		ice::printf("Browser controller: %s", CompanionWeb.join_url(address).c_str());
	}
}
