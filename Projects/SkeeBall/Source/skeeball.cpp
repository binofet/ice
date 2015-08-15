
#include "skeeball.h"

#include "Core/GXDevice/icGXDeviceGL.h"
#ifndef __APPLE__ // input is not good on OSX yet
#include "Core/IO/icInput.h"
#endif


#ifdef __APPLE__
#include <OpenGL/glu.h>
#else
#include <gl/GLU.h>
#endif

#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#endif

#define USE_BEEPS_AND_DELAYS			// undefine to test library works without them

bool HACK_FULLSCREEN=false;



#ifdef WIIMOTE
void on_state_change (wiimote &remote, state_change_flags changed)
{
    // a MotionPlus was detected
    if(changed & MOTIONPLUS_DETECTED)
    {
        // enable it if there isn't a normal extension plugged into it
        // (MotionPlus devices don't report like normal extensions until
        //  enabled - and then, other extensions attached to it will no longer be
        //  reported (so disable it when you want to access to those again).
        if(remote.ExtensionType == wiimote_state::NONE) {
            bool res = remote.EnableMotionPlus();
            _ASSERT(res);
        }
    }
    else if(changed & MOTIONPLUS_EXTENSION_CONNECTED)
    {
        // an extension is connected to the MotionPlus.  We can't read it if the
        //  MotionPlus is currently enabled, so disable it:
        if(remote.MotionPlusEnabled())
            remote.DisableMotionPlus();
    }
    else if(changed & MOTIONPLUS_EXTENSION_DISCONNECTED)
    {
        // the extension plugged into the MotionPlus was removed, so enable
        //  the MotionPlus data again:
        if(remote.MotionPlusConnected())
            remote.EnableMotionPlus();
    }
    // extension was just connected:
    else if(changed & EXTENSION_CONNECTED)
    {
#ifdef USE_BEEPS_AND_DELAYS
        Beep(1000, 200);
#endif
        // switch to a report mode that includes the extension data (we will
        //  loose the IR dot sizes)
        // note: there is no need to set report types for a Balance Board.
        if(!remote.IsBalanceBoard())
            remote.SetReportType(wiimote::IN_BUTTONS_ACCEL_IR_EXT);
    }
    // extension was just disconnected:
    else if(changed & EXTENSION_DISCONNECTED)
    {
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
SkeeBall::SkeeBall(void)
{
    m_pMainWin = NULL;

    m_lastX = 0;
    m_lastY = 0;

    m_fNetTimer = 0.016f;
    playing = false;
    wiimote = false;
    memset(&m_InitialSettings,0,sizeof(m_InitialSettings));
}// END FUNCTION SkeeBall(void)


/*!d'tor
**/
SkeeBall::~SkeeBall(void)
{
    if (ICEFAIL(Cleanup()))
        icWarning("SkeeBall did not cleanup properly");

#ifdef WIIMOTE
    remote.Disconnect();
#endif

#ifdef CONSOLE_OUTPUT
    BRIGHT_WHITE; // for automatic 'press any key to continue' msg
    CloseHandle(console);
#endif
}// END FUNCTION ~SkeeBall(void)


/*! Frees all allocated memory
 *
 *  @returns        ICRESULT        Success/failure code of cleanup
**/
ICRESULT SkeeBall::Cleanup(void)
{
    if (m_pMainWin)
        SAFEDEL(m_pMainWin);

    if (m_pGxDevice)
        SAFEDEL(m_pGxDevice);

    playing = false;

    // write to settings.ini
    //if (m_InitialSettings != m_Settings)
    {
        icString temps;
        char tempb[256];

        // clear current settings
        m_SettingsFile.ClearValues();
        
        // GET SCREEN AND BACK BUFFER RESOLUTION
        temps = (m_Settings.bFullscreen) ? "true" : "false";
        m_SettingsFile.AddPair("Engine.Graphics.bFullScreen", temps.ToChar());
        sprintf(tempb,"%i",m_Settings.iScreen_x);
        m_SettingsFile.AddPair("Engine.Graphics.ScreenX", tempb);
        sprintf(tempb,"%i",m_Settings.iScreen_y);
        m_SettingsFile.AddPair("Engine.Graphics.ScreenY", tempb);
        //sprintf(tempb,"%i",m_Settings.iBackbuf_x);
        //m_SettingsFile.AddPair("Engine.Graphics.BackBufferX", tempb);
        //sprintf(tempb,"%i",m_Settings.iBackbuf_y);
        //m_SettingsFile.AddPair("Engine.Graphics.BackBufferY", tempb);

        // GET FOG SETTINGS
        temps = (m_Settings.bFog) ? "true" : "false";
        m_SettingsFile.AddPair("Engine.Fog.bEnable", temps.ToChar());
        sprintf(tempb,"%f",m_Settings.fogParams.start);
        m_SettingsFile.AddPair("Engine.Fog.fStart", tempb);
        sprintf(tempb,"%f",m_Settings.fogParams.end);
        m_SettingsFile.AddPair("Engine.Fog.fEnd", tempb);
        sprintf(tempb,"%f",m_Settings.fogParams.density);
        m_SettingsFile.AddPair("Engine.Fog.fDensity", tempb);
        sprintf(tempb,"(%u,%u,%u,%u)",m_Settings.fogParams.col.r,
                                      m_Settings.fogParams.col.g,
                                      m_Settings.fogParams.col.b,
                                      m_Settings.fogParams.col.a);
        m_SettingsFile.AddPair("Engine.Fog.Color", tempb);
        sprintf(tempb,"%d",m_Settings.fogParams.type);
        m_SettingsFile.AddPair("Engine.Fog.ucType", tempb);

        // GET AUDIO SETTINGS
        temps = (m_Settings.bMusic) ? "true" : "false";
        m_SettingsFile.AddPair("Game.Audio.bEnableMusic", temps.ToChar());
        temps = (m_Settings.bSfx) ? "true" : "false";
        m_SettingsFile.AddPair("Game.Audio.bEnableSfx", temps.ToChar());
        sprintf(tempb,"%f",m_Settings.fMusic_vol);
        m_SettingsFile.AddPair("Game.Audio.fMusicVol", tempb);
        sprintf(tempb,"%f",m_Settings.fSfx_vol);
        m_SettingsFile.AddPair("Game.Audio.fSfxVol", tempb);

        // GET FREE CAM SETTINGS
        temps = (m_Settings.bFreeCam) ? "true" : "false";
        m_SettingsFile.AddPair("Game.bFreeCam",temps.ToChar());

        // GET WII MOTE SETTINGS
        temps = (m_Settings.bWiiMote) ? "true" : "false";
        m_SettingsFile.AddPair("Game.bWiiMote",temps.ToChar());

        // GET HIGH SCORE
        sprintf(tempb,"%i",m_Settings.iHighScore);
        m_SettingsFile.AddPair("Game.iHighScore",tempb);

        m_SettingsFile.Write();
    }

    icPrintf("SkeeBall::Cleanup succeeded");

    return IC_OK;
}// END FUNCTION Cleanup(void)


/*! Initialised the airhockey game
 *
 *  @param          argc            Number of commandl-ine arguments
 *  @param          args            Command-line arguments
 *  @returns        ICRESULT        Success/failure of initialization
**/
ICRESULT SkeeBall::Init(int argc, char* args[])
{
    ICRESULT res = IC_OK;

    // Initialize SkeeBall Settings
    ProcessSettings();

    HACK_FULLSCREEN = m_Settings.bWiiMote;

#ifdef WIIMOTE
    if (m_Settings.bWiiMote)
    {
        // spawn console window and attache input/output streams
        AllocConsole();

        console = GetStdHandle(STD_OUTPUT_HANDLE);
        int hCrt = _open_osfhandle((long) console, _O_TEXT);
        FILE* hf_out = _fdopen(hCrt, "w");
        setvbuf(hf_out, NULL, _IONBF, 1);
        *stdout = *hf_out;

        HANDLE handle_in = GetStdHandle(STD_INPUT_HANDLE);
        hCrt = _open_osfhandle((long) handle_in, _O_TEXT);
        FILE* hf_in = _fdopen(hCrt, "r");
        setvbuf(hf_in, NULL, _IONBF, 128);
        *stdin = *hf_in;
    }
#endif

    // create the main window
#if defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW32__)
    uint winFlags = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    //uint winFlags = WS_CAPTION ;
    //uint winFlags = WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
#elif defined __APPLE__
    uint winFlags = kWindowCloseBoxAttribute |
                    kWindowCollapseBoxAttribute |
                    kWindowStandardHandlerAttribute ;
    //uint winFlags = kWindowNoTitleBarAttribute;
#else
        uint winFlags = 0;
#endif

    icWindowDef window = {winFlags,
                          m_Settings.iScreen_x,
                          m_Settings.iScreen_y,
                          0,0,
                          "Skee Ball",
                          m_Settings.bFullscreen};
    m_pMainWin = icWindow::GetWindow(window);

    // create the graphics device
    m_pGxDevice = new icGXDeviceGL();

    // check that we got the goods
    if (!m_pMainWin || !m_pGxDevice)
	{
#ifdef DEBUG_INSTALL
		MessageBoxA(0,"Failed to window or engine", "Error", 0);
#endif
		return IC_FAIL_GEN;
	}

    // initialize the graphics device
    m_pGxDevice->Init(m_pMainWin);

    //m_pGxDevice->EnableFog(m_Settings.bFog, m_Settings.fogParams);

    // Set the clear color to black
    ((icGXDeviceGL*)m_pGxDevice)->SetClearColor(icColor::BLACK);

    // show the main window
    m_pMainWin->Show();

    // initialize the sound engine
    res = m_SoundDevice.Initialize();
    if (ICEFAIL(res))
	{
#ifdef DEBUG_INSTALL
		MessageBoxA(0,"Failed to init audio device", "Error", 0);
#endif
        return res;
	}

    // Initialize the content manager
    m_Content = new icContentLoader(m_pGxDevice, &m_SoundDevice);

    icSoundParam params;
    params.bPaused = false;
    params.fPitch = 1.0f;
    params.bLoop = true;
    params.fVol = m_Settings.fMusic_vol;
    m_Content->Load("Resource/audio/01castlevania.wav",&m_BackGroundMusic);
    m_BackGroundMusic->SetParams(params);


    // start the background music
    if (m_Settings.bMusic)
        m_BackGroundMusic->Play();

    if (ICEFAIL(res))
	{
#ifdef DEBUG_INSTALL
		MessageBoxA(0,"some other shit failed", "Error", 0);
#endif
        return res;
	}

    // push the main menu (TODO, goes to gamestate right now)
    m_GameState.Init(m_Content);
    m_MainMenu.Init(m_Content);
    m_SettingsMenu.Init(m_Content);
    m_PauseMenu.Init(m_Content);
    m_LastGame.Init(m_Content);
    m_StateMachine.PushState(&m_LastGame);
    m_StateMachine.PushState(&m_MainMenu);

    // start keeping time
    m_GameTime.Elapsed();

    m_font.Initialize("Arial",24,m_Content->GetDevice());

    playing = true;

#ifdef WIIMOTE
    if (m_Settings.bWiiMote)
    {
        icThreadCreate<SkeeBall>(this,&SkeeBall::WiiMoteLoop,NULL,&m_pthread);
    }
#endif

    if (!ICEFAIL(res))
        return icApplication::Init(argc,args);    

    
    return res;
}// END FUNCTION Init(int argc, char* args[])


#ifdef WIIMOTE
ICRESULT SkeeBall::WiiMoteLoop(void*)
{
    // let's load a couple of samples:
    wiimote_sample sine_sample, daisy_sample;

    // simple callback example (we use polling for almost everything here):
    remote.ChangedCallback		= on_state_change;
    //  notify us only when something related to the extension changes
    remote.CallbackTriggerFlags = (state_change_flags)( EXTENSION_CHANGED |
        MOTIONPLUS_CHANGED);

reconnect:
    COORD pos = { 0, 6 };
    SetConsoleCursorPosition(console, pos);

    // try to connect the first available wiimote in the system
    //  (available means 'installed, and currently Bluetooth-connected'):
    COLWHITE; _tprintf(_T("  Looking for a Wiimote     "));

    static const TCHAR* wait_str[] = { _T(".  "), _T(".. "), _T("...") };
    unsigned count = 0;
    while(!remote.Connect(wiimote::FIRST_AVAILABLE)) {
        _tprintf(_T("\b\b\b\b%s "), wait_str[count%3]);
        count++;
#ifdef USE_BEEPS_AND_DELAYS
        Beep(500, 30); Sleep(1000);
#endif
    }

    wiimote = true;

    // connected - light all LEDs
    remote.SetLEDs(0x0f);
    BRIGHT_CYAN; _tprintf(_T("\b\b\b\b... connected!"));
    if(remote.IsBalanceBoard()) {
        BRIGHT_WHITE;_tprintf(_T("  (Balance Board)"));
    }

#ifdef USE_BEEPS_AND_DELAYS
    Beep(1000, 300); Sleep(2000);
#endif

    COORD cursor_pos = { 0, 6 };


    // ask the wiimote to report everything (using the 'non-continous updates'
    //  default mode - updates will be frequent anyway due to the acceleration/IR
    //  values changing):

    // note1: you don't need to set a report type for Balance Board - the library
    //         takes care of it (there is only one).
    // note2: for wiimotes, the report mode that includes the extension data
    //		   unfortunately only reports the 'BASIC' IR info (ie. no dot sizes) -
    //		   so let's choose the best mode based on the extension status (we also
    //		   toggle modes as needed in the callback above):
    if(!remote.IsBalanceBoard())
    {
        if(remote.bExtension)
            remote.SetReportType(wiimote::IN_BUTTONS_ACCEL_IR_EXT); // no IR dots
        else
            remote.SetReportType(wiimote::IN_BUTTONS_ACCEL_IR);		//    IR dots
    }

    // (stuff for animations)
    DWORD	 last_rumble_time = timeGetTime(); // for rumble text animation
    DWORD    last_led_time    = timeGetTime(); // for led         animation
    bool	 rumble_text	  = true;
    unsigned lit_led          = 0;	

    // display the wiimote state data until 'Home' is pressed:
    bool caliubrate_atrest_once = true;
    while(playing)// && !GetAsyncKeyState(VK_ESCAPE))
    {
        // the wiimote state needs to be refreshed for each pass
        while(remote.RefreshState() == NO_CHANGE)
            Sleep(1); // // don't hog the CPU if nothing changed

        cursor_pos.Y = 8;
        SetConsoleCursorPosition(console, cursor_pos);

        // did we loose the connection?
        if(remote.ConnectionLost())
        {
            BRIGHT_RED; _tprintf(
                _T("   *** connection lost! ***                                          \n")
                BLANK_LINE BLANK_LINE BLANK_LINE BLANK_LINE BLANK_LINE BLANK_LINE
                BLANK_LINE BLANK_LINE BLANK_LINE BLANK_LINE BLANK_LINE BLANK_LINE
                BLANK_LINE BLANK_LINE BLANK_LINE);
            Beep(100, 1000);
            Sleep(2000);
            COORD pos = { 0, 6 };
            SetConsoleCursorPosition(console, pos);
            _tprintf(BLANK_LINE BLANK_LINE BLANK_LINE);
            goto reconnect;
        }

        // rumble if 'B' (trigger) is pressed
        remote.SetRumble(remote.Button.B());

        // disable MotionPlus (if connected) to allow its own extension port
        //  to work
        if(remote.Button.Minus())
            remote.DisableMotionPlus();

        // actions for buttons just pressed/released:
        static bool last_A = false, last_One = false, last_Two = false;

#define ON_PRESS_RELEASE(button, pressed_action, released_action)	\
        { bool pressed = remote.Button.button();						\
        if(pressed)													\
        {				   /* just pressed? */						\
        if(!last_##button) pressed_action;							\
        }															\
        else if(last_##button) /* just released */						\
        released_action;											\
        /* remember the current button state for next time */			\
        last_##button = pressed; }

        // Battery level:
        CYAN; _tprintf(_T("  Battery: "));
        // (the green/yellow colour ranges are rough guesses - my wiimote
        //  with rechargeable battery pack fails at around 15%)
        (remote.bBatteryDrained	    )? BRIGHT_RED   :
            (remote.BatteryPercent >= 30)? BRIGHT_GREEN : BRIGHT_YELLOW;
        _tprintf(_T("%3u%%   "), remote.BatteryPercent);

        DWORD current_time = timeGetTime();

        // LEDs:
        //  animate them every second
        if((current_time - last_led_time) >= 1000) {
            remote.SetLEDs((BYTE)(1<<lit_led));
            lit_led		  = (++lit_led) % 4;
            last_led_time = timeGetTime();
        }

        CYAN; _tprintf(_T("LEDs: ")); COLWHITE; _tprintf(_T("["));
        for(unsigned led=0; led<4; led++)
        {
            if(remote.LED.Lit(led)) {
                BRIGHT_CYAN; _tprintf(_T("*"));
            }
            else{
                COLWHITE      ; _tprintf(_T("-"));//_T("%c"), '0'+led);
            }
        }

        // Rumble
        COLWHITE; _tprintf(_T("]    "));
        if(remote.bRumble) {
            BRIGHT_WHITE; _tprintf(rumble_text? _T(" RUMBLE") : _T("RUMBLE "));
            // animate the text
            if((current_time - last_rumble_time) >= 110) {
                rumble_text		 = !rumble_text;
                last_rumble_time = current_time;
            }
        }
        else
            _tprintf(_T("       "));

        // Output method:
        CYAN; _tprintf( _T("    using %s\n"), (remote.IsUsingHIDwrites()?
            _T("HID writes") : _T("WriteFile()")));

        // Buttons:
        CYAN; _tprintf(_T("\n  Buttons: ")); COLWHITE; _tprintf(_T("["));
        for(unsigned bit=0; bit<16; bit++)
        {
            WORD mask = (WORD)(1 << bit);
            // skip unused bits
            if((wiimote_state::buttons::ALL & mask) == 0)
                continue;

            const TCHAR* button_name = wiimote::ButtonNameFromBit[bit];
            bool		 pressed	 = ((remote.Button.Bits & mask) != 0);
            if(bit > 0) {
                CYAN; _tprintf(_T("|")); // seperator
            }
            if(pressed) {
                BRIGHT_WHITE; _tprintf(_T("%s")  , button_name);
            }
            else{
                COLWHITE       ; _tprintf(_T("%*s"), _tcslen(button_name), _T(""));
            }
        }
        COLWHITE; _tprintf(_T("]\n"));

        // Acceleration:
        CYAN ; _tprintf(_T("    Accel:"));
        remote.IsBalanceBoard()? RED : COLWHITE;
        _tprintf(_T("  X %+2.3f  Y %+2.3f  Z %+2.3f  \n"),
            remote.Acceleration.X,
            remote.Acceleration.Y,
            remote.Acceleration.Z);

        // Orientation estimate (shown red if last valid update is aging):
        CYAN ; _tprintf(_T("   Orient:"));
        remote.IsBalanceBoard()? RED : COLWHITE;
        _tprintf(_T("  UpdateAge %3u  "), remote.Acceleration.Orientation.UpdateAge);

        //  show if the last orientation update is considered out-of-date
        //   (using an arbitrary threshold)
        if(remote.Acceleration.Orientation.UpdateAge > 10)
            RED;

        _tprintf(_T("Pitch:%4ddeg  Roll:%4ddeg  \n")
            _T("                           (X %+.3f  Y %+.3f  Z %+.3f)      \n"),
            (int)remote.Acceleration.Orientation.Pitch,
            (int)remote.Acceleration.Orientation.Roll ,
            remote.Acceleration.Orientation.X,
            remote.Acceleration.Orientation.Y,
            remote.Acceleration.Orientation.Z);

        // IR:
        CYAN ; _tprintf(_T("       IR:"));
        remote.IsBalanceBoard()? RED : COLWHITE;
        _tprintf(_T("  Mode %s  "),
            ((remote.IR.Mode == wiimote_state::ir::OFF     )? _T("OFF  ") :
            (remote.IR.Mode == wiimote_state::ir::BASIC   )? _T("BASIC") :
            (remote.IR.Mode == wiimote_state::ir::EXTENDED)? _T("EXT. ") :
            _T("FULL ")));
        // IR dot sizes are only reported in EXTENDED IR mode (FULL isn't supported yet)
        bool dot_sizes = (remote.IR.Mode == wiimote_state::ir::EXTENDED);

        for(unsigned index=0; index<4; index++)
        {
            wiimote_state::ir::dot &dot = remote.IR.Dot[index];

            if(!remote.IsBalanceBoard()) COLWHITE;
            _tprintf(_T("%u: "), index);

            if(dot.bVisible) {
                COLWHITE; _tprintf(_T("Seen       "));
            }
            else{
                RED  ; _tprintf(_T("Not seen   "));
            }

            _tprintf(_T("Size"));
            if(dot_sizes)
                _tprintf(_T("%3d "), dot.Size);
            else{
                RED; _tprintf(_T(" n/a"));
                if(dot.bVisible) COLWHITE;
            }

            _tprintf(_T("  X %.3f  Y %.3f\n"), dot.X, dot.Y);

            if(index < 3)
                _tprintf(_T("                        "));
        }

        // Speaker:
        CYAN ; _tprintf(_T("  Speaker:"));
        remote.IsBalanceBoard()? RED : COLWHITE;
        _tprintf(_T("  %s | %s    "), (remote.Speaker.bEnabled? _T("On ") :
            _T("Off")),
            (remote.Speaker.bMuted  ? _T("Muted") :
            _T("     ")));
        if(!remote.Speaker.bEnabled || remote.Speaker.bMuted)
            RED;
        else//if(remote.IsPlayingAudio()) BRIGHT_WHITE; else COLWHITE;
            COLWHITE;
        _tprintf(_T("Frequency %4u Hz   Volume 0x%02x\n"),
            wiimote::FreqLookup[remote.Speaker.Freq],
            remote.Speaker.Volume);

        // -- Extensions --:
        CYAN ; _tprintf(_T("__________\n  Extnsn.:  "));
        switch(remote.ExtensionType)
        {
        case wiimote_state::NONE:
            {
                RED;
                _tprintf(_T("None                                                             \n"));
                _tprintf(BLANK_LINE BLANK_LINE BLANK_LINE);
            }
            break;

        case wiimote_state::PARTIALLY_INSERTED:
            {
                BRIGHT_RED;
                _tprintf(_T("Partially Inserted                                               \n"));
                _tprintf(BLANK_LINE BLANK_LINE BLANK_LINE);
            }
            break;

            // -- Nunchuk --
        case wiimote_state::NUNCHUK:
            {
                BRIGHT_WHITE; _tprintf(_T("Nunchuk   "));

                // Buttons:
                CYAN		; _tprintf(_T("Buttons: ")); COLWHITE; _tprintf(_T("["));
                BRIGHT_WHITE; _tprintf(remote.Nunchuk.C? _T("C") : _T(" "));
                CYAN		; _tprintf(_T("|"));
                BRIGHT_WHITE; _tprintf(remote.Nunchuk.Z? _T("Z") : _T(" "));
                COLWHITE		; _tprintf(_T("]   "));
                // Joystick:
                CYAN		; _tprintf(_T("Joystick:  "));
                COLWHITE		; _tprintf(_T("X %+2.3f  Y %+2.3f\n"),
                    remote.Nunchuk.Joystick.X,
                    remote.Nunchuk.Joystick.Y);
                // Acceleration:
                CYAN		; _tprintf(_T("    Accel:"));
                COLWHITE		; _tprintf(_T("  X %+2.3f  Y %+2.3f  Z %+2.3f  \n"),
                    remote.Nunchuk.Acceleration.X,
                    remote.Nunchuk.Acceleration.Y,
                    remote.Nunchuk.Acceleration.Z);

                // Orientation estimate (shown red if last valid update is aging):
                CYAN		; _tprintf(_T("   Orient:"));
                COLWHITE		; _tprintf(_T("  UpdateAge %3u  "),
                    remote.Nunchuk.Acceleration.Orientation.UpdateAge);
                //  show if the last orientation update is aging
                if(remote.Nunchuk.Acceleration.Orientation.UpdateAge > 10)
                    RED;
                _tprintf(_T("Pitch %4ddeg  Roll %4ddeg  \n")
                    _T("                           (X %+.2f  Y %+.2f  Z %+.2f)      \n"),
                    (int)remote.Nunchuk.Acceleration.Orientation.Pitch,
                    (int)remote.Nunchuk.Acceleration.Orientation.Roll ,
                    remote.Nunchuk.Acceleration.Orientation.X,
                    remote.Nunchuk.Acceleration.Orientation.Y,
                    remote.Nunchuk.Acceleration.Orientation.Z);
            }
            break;

            // -- Classic Controller --
        case wiimote_state::CLASSIC:
        case wiimote_state::GH3_GHWT_GUITAR:
        case wiimote_state::GHWT_DRUMS:
            {
                BRIGHT_WHITE;
                // the Guitar Hero controller is just a classic controller with
                //  another ID
                if(remote.ExtensionType == wiimote_state::CLASSIC)
                    _tprintf(_T("Classic Controller   "));
                else if(remote.ExtensionType == wiimote_state::GH3_GHWT_GUITAR)
                    _tprintf(_T("GH3/GHWT Guitar      "));
                else
                    _tprintf(_T("GHWT Drums           "));

                // L: Joystick/Trigger
                COLWHITE; _tprintf(_T("L:  "));
                CYAN ; _tprintf(_T("Joy "));
                COLWHITE; _tprintf(_T("X %+2.3f  Y %+2.3f  "),
                    remote.ClassicController.JoystickL.X,
                    remote.ClassicController.JoystickL.Y);
                CYAN ; _tprintf(_T("Trig "));
                COLWHITE; _tprintf(_T("%+2.3f\n"),
                    remote.ClassicController.TriggerL);
                // R: Joystick/Trigger
                COLWHITE; _tprintf(_T("                                 R:  "));
                CYAN ; _tprintf(_T("Joy "));
                COLWHITE; _tprintf(_T("X %+2.3f  Y %+2.3f  "),
                    remote.ClassicController.JoystickR.X,
                    remote.ClassicController.JoystickR.Y);
                CYAN ; _tprintf(_T("Trig "));
                COLWHITE; _tprintf(_T("%+2.3f\n"),
                    remote.ClassicController.TriggerR);

                // Buttons:
                CYAN; _tprintf(_T("  Buttons: ")); COLWHITE; _tprintf(_T("["));
                for(unsigned bit=0; bit<16; bit++)
                {
                    WORD mask = (WORD)(1 << bit);
                    // skip unused bits
                    if((wiimote_state::classic_controller::buttons::ALL & mask) == 0)
                        continue;

                    const TCHAR* button_name = wiimote::ClassicButtonNameFromBit[bit];
                    const TCHAR* seperator	 = (bit==0)? _T("") : _T("|");
                    bool		 pressed	 = ((remote.ClassicController.Button.Bits & mask) != 0);
                    CYAN; _tprintf(seperator);
                    if(pressed) {
                        BRIGHT_WHITE; _tprintf(_T("%s")  , button_name);
                    }
                    else{
                        COLWHITE	; _tprintf(_T("%*s"), _tcslen(button_name), _T(""));
                    }
                }
                COLWHITE; _tprintf(_T("]"));
            }
            break;

        case wiimote_state::BALANCE_BOARD:
            {
                BRIGHT_WHITE; _tprintf(_T("Balance Board"));

                // Weights:
                CYAN ; _tprintf(_T("   Weight: "));
                COLWHITE; _tprintf(_T("TL    "));
                _tprintf(_T("%6.2f"), remote.BalanceBoard.Kg.TopL);
                CYAN ; _tprintf(_T(" kg")); COLWHITE; ;_tprintf(_T("  TR "));
                _tprintf(_T("%6.2f"), remote.BalanceBoard.Kg.TopR);
                CYAN;  _tprintf(_T(" kg\n")); COLWHITE;
                _tprintf(_T("                                    BL    "));
                _tprintf(_T("%6.2f"), remote.BalanceBoard.Kg.BottomL);
                CYAN;  _tprintf(_T(" kg")); COLWHITE; _tprintf(_T("  BR "));
                _tprintf(_T("%6.2f"), remote.BalanceBoard.Kg.BottomR);
                CYAN;  _tprintf(_T(" kg \n")); COLWHITE;
                _tprintf(_T("                                    Total "));
                _tprintf(_T("%6.2f"), remote.BalanceBoard.Kg.Total);
                CYAN;  _tprintf(_T(" kg"));
            }
            break;

        case wiimote_state::MOTION_PLUS:
            {
                BRIGHT_WHITE; _tprintf(_T("Motion Plus"));

                CYAN ; _tprintf(_T("    Raw: "));
                COLWHITE; _tprintf(_T("Yaw: %04hx  ")   , remote.MotionPlus.Raw.Yaw);
                COLWHITE; _tprintf(_T("Pitch: %04hx  ") , remote.MotionPlus.Raw.Pitch);
                COLWHITE; _tprintf(_T("Roll: %04hx  \n"), remote.MotionPlus.Raw.Roll);
                CYAN ; _tprintf(_T("                         Float: "));
                COLWHITE; _tprintf(_T("  %8.3fdeg")     , remote.MotionPlus.Speed.Yaw);
                COLWHITE; _tprintf(_T("  %8.3fdeg")   , remote.MotionPlus.Speed.Pitch);
                COLWHITE; _tprintf(_T(" %8.3fdeg\n")   , remote.MotionPlus.Speed.Roll);
                _tprintf(BLANK_LINE BLANK_LINE);
                //Sleep(500);
            }
            break;
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
ICRESULT SkeeBall::Update(void)
{
    ICRESULT res = IC_OK;


    // current game time
    icReal elapsed = m_GameTime.Elapsed();

    // current input
#ifndef __APPLE__
    icInput* pInput = icInput::Instance();
#endif

    // update the graphics device
    res = m_pGxDevice->Update();


    // update the game
    res |= m_StateMachine.Update(elapsed);

#ifdef WIIMOTE
    if (m_Settings.bWiiMote)
    {
        if (remote.IsConnected())
        {
            float x = 0.0f;
            float y = 0.0f;
            int num=0;

            for(unsigned index=0; index<4; index++)
            {
                wiimote_state::ir::dot &dot = remote.IR.Dot[index];

                if(dot.bVisible)
                {
                    x += dot.X;
                    y += dot.Y;
                    num++;
                }
            }
            
            int x_pos = 0;
            int y_pos = 0;

            if (!num)
            {
                x_pos = m_lastX;
                y_pos = m_lastY;
            }
            else
            {
                x_pos = (x/(float)num) * m_Settings.iScreen_x;
                y_pos = (y/(float)num) * m_Settings.iScreen_y;
            }

            wiimote_state::ir::dot &dot = remote.IR.Dot[0];

            x_pos = icClamp((int)(dot.X * (float)m_Settings.iScreen_x),0,m_Settings.iScreen_x);
            y_pos = icClamp((int)(dot.Y * (float)m_Settings.iScreen_y),0,m_Settings.iScreen_y);

            icInput::Instance()->UpdateMousePos(x_pos,y_pos);

            m_lastX = x_pos;
            m_lastY = y_pos;
        }
    }
#endif

#ifndef __APPLE__
    pInput->Update();
#endif

    //icSoundParam param;

    //param.fPitch = 1.0f;
    //param.bLoop = true;
    //param.fVol = m_Settings.fMusic_vol;

    //m_BackGroundMusic->SetParams(param);
    return res;
}// END FUNCTION Update(void)


/*! Renders the airhockey game
 *
 *  @param          ICRESULT        Success/failure of render
**/
ICRESULT SkeeBall::Render(void)
{
    ICRESULT res = IC_OK;
    m_pGxDevice->FrameBegin();

    // render the game state
    res = m_StateMachine.Render();

    m_pGxDevice->SetProjection(m_pGxDevice->GetOrtho());
    icMatrix44 temp;
    temp.Identity();
    m_pGxDevice->SetWorldTransform(temp);
    m_pGxDevice->SetViewMatrix(temp);

#ifdef WIIMOTE
    // if user has bWiiMote=true in the settings file
    if (m_Settings.bWiiMote)
    {
        if (wiimote)
        {
            wiimote_state::ir::dot &dot = remote.IR.Dot[0];

            int x_pos = icClamp((int)(dot.X * (float)m_Settings.iScreen_x),0,m_Settings.iScreen_x);
            int y_pos = icClamp(m_Settings.iScreen_y - (int)(dot.Y * (float)m_Settings.iScreen_y),0,m_Settings.iScreen_y);
            
            //m_font.Print(x_pos,y_pos,icColor::WHITE,"Wii Pos");

            //m_font.Print(70,60,icColor::WHITE,"Wiimote connected");
        }
        else
            m_font.Print(icVector2Int(10,50),icColor::WHITE,"Looking for wiimote");
    }
#endif

    m_pGxDevice->FrameEnd(true);
    return IC_OK;
}// END FUNCTION Render(void)


/*! Quits the airhockey game
 *
 *  @param          ICRESULT        Success/failure of quit
**/
ICRESULT SkeeBall::Quit(void)
{
    // should probably do some clean-up and what have you here.
    // still not sure about the existance of this function. . . 
    // we'll see. (was thinking it could be called anywere, like exit(0)
    // without being as bad of a hack)
    //
    //  - joel
    return IC_OK;
}// END FUNCTION Quit(void)


/*! Process settings
**/
void SkeeBall::ProcessSettings(void)
{
    // get settings file opened
    if (ICEFAIL(m_SettingsFile.SetFile("settings.ini")))
        return;

    icString val;
    icString temp;
    char tbuff[256];

    // make sure the settins file read ok
    if (ICEOK_NOWARN(m_SettingsFile.Read()))
    {
        // GET SCREEN AND BACK BUFFER RESOLUTION
        val = m_SettingsFile.GetValue("Engine.Graphics.bFullScreen", "false");
        m_Settings.bFullscreen = (strcmp(val.ToChar(),"true") == 0) ? true : false;

        val = m_SettingsFile.GetValue("Engine.Graphics.ScreenX", "1280");
        m_Settings.iScreen_x = atoi(val.ToChar());

        val = m_SettingsFile.GetValue("Engine.Graphics.ScreenY", "720");
        m_Settings.iScreen_y = atoi(val.ToChar());



        // GET FOG SETTINGS
        val = m_SettingsFile.GetValue("Engine.Fog.bEnable", "true");
        m_Settings.bFog = (strcmp(val.ToChar(),"true") == 0) ? true : false;

        val = m_SettingsFile.GetValue("Engine.Fog.fStart", "20.000000");
        m_Settings.fogParams.start = (float)atof(val.ToChar());

        val = m_SettingsFile.GetValue("Engine.Fog.fEnd", "125.000000");
        m_Settings.fogParams.end = (float)atof(val.ToChar());

        val = m_SettingsFile.GetValue("Engine.Fog.fDensity", "0.000001");
        m_Settings.fogParams.density = (float)atof(val.ToChar());

        val = m_SettingsFile.GetValue("Engine.Fog.Color", "(240,156,46,255)");
        sscanf(val.ToChar(),"(%u,%u,%u,%u)",&m_Settings.fogParams.col.r,
                                            &m_Settings.fogParams.col.g,
                                            &m_Settings.fogParams.col.b,
                                            &m_Settings.fogParams.col.a);

        val = m_SettingsFile.GetValue("Engine.Fog.ucType", "1");
        m_Settings.fogParams.type = (uchar)atoi(val.ToChar());


        // GET AUDIO SETTINGS
        val = m_SettingsFile.GetValue("Game.Audio.bEnableMusic", "true");
        m_Settings.bMusic = (strcmp(val.ToChar(),"true") == 0) ? true : false;

        val = m_SettingsFile.GetValue("Game.Audio.bEnableSfx", "true");
        m_Settings.bSfx = (strcmp(val.ToChar(),"true") == 0) ? true : false;

        val = m_SettingsFile.GetValue("Game.Audio.fMusicVol", "0.75");
        m_Settings.fMusic_vol = (float)atof(val.ToChar());

        val = m_SettingsFile.GetValue("Game.Audio.fSfxVol", "0.5");
        m_Settings.fSfx_vol = (float)atof(val.ToChar());

        m_InitialSettings = m_Settings;


        // GET GAME SETTINGS
        val = m_SettingsFile.GetValue("Game.bFreeCam","false");
        m_Settings.bFreeCam = (strcmp(val.ToChar(),"true") == 0) ? true : false;

        // GET WIIMOTE SETTINGS
        val = m_SettingsFile.GetValue("Game.bWiiMote","true");
        m_Settings.bWiiMote = (strcmp(val.ToChar(),"true") == 0) ? true : false;

        // GET HIGH SCORE
        val = m_SettingsFile.GetValue("Game.iHighScore","0");
        m_Settings.iHighScore = atoi(val.ToChar());

    }
}// END FUNCTION ProcessSettings(void)