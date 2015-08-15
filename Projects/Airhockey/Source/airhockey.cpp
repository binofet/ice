
#include "airhockey.h"

#include "Core/GXDevice/icGXDeviceGL.h"
#include "Core/GXDevice/icGXDeviceDX9.h"
#ifndef __APPLE__ // input is not good on OSX yet
#include "Core/IO/icInput.h"
#endif


#ifdef __APPLE__
#include <OpenGL/glu.h>
#else
#include <gl/GLU.h>
#endif


/*! c'tor
**/
Airhockey::Airhockey(void)
{
    m_pMainWin = NULL;

    m_iGameMode = GAME_MODE_SINGLE;

    m_fNetTimer = 0.016f;
    memset(&m_InitialSettings,0,sizeof(m_InitialSettings));
}// END FUNCTION Airhockey(void)


/*!d'tor
**/
Airhockey::~Airhockey(void)
{
    if (ICEFAIL(Cleanup()))
        icWarning("Airhockey did not cleanup properly");
}// END FUNCTION ~Airhockey(void)


/*! Frees all allocated memory
 *
 *  @returns        ICRESULT        Success/failure code of cleanup
**/
ICRESULT Airhockey::Cleanup(void)
{
    if (m_pMainWin)
        SAFEDEL(m_pMainWin);

    if (m_pGxDevice)
        SAFEDEL(m_pGxDevice);


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

        // GET PADDLE SETTINGS
        sprintf(tempb,"%d",m_Settings.ucPaddle_type);
        m_SettingsFile.AddPair("Game.Paddle.ucType",tempb);

        // GET NETWORK SETTINGS
        m_SettingsFile.AddPair("Game.Network.sConnectIP",
                               m_Settings.sLast_join_ip.ToChar());

        m_SettingsFile.Write();
    }

    icPrintf("Airhockey::Cleanup succeeded");

    return IC_OK;
}// END FUNCTION Cleanup(void)


/*! Initialised the airhockey game
 *
 *  @param          argc            Number of commandl-ine arguments
 *  @param          args            Command-line arguments
 *  @returns        ICRESULT        Success/failure of initialization
**/
ICRESULT Airhockey::Init(int argc, char* args[])
{
    ICRESULT res = IC_OK;

    // Initialize Airhockey Settings
    ProcessSettings();

    char temp[256];
    int l = gethostname(temp,256);
    hostent *phe = gethostbyname(temp);
    if (phe == 0)
	{
#ifdef DEBUG_INSTALL
		MessageBoxA(0,"Failed to get host name", "Error", 0);
#endif
        return IC_FAIL_GEN;
	}

    if (phe->h_addr_list[0] != 0)
    {
        in_addr addr;
        memcpy(&addr, phe->h_addr_list[0], sizeof(struct in_addr));
        strcpy(temp,inet_ntoa(addr));
    }

    Socket.setHomeAddress(AIRPORT,temp);
    Socket.setDestAddress(AIRPORT,m_Settings.sLast_join_ip.ToChar());

    // create the main window
#if defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW32__)
    uint winFlags = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
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
                          "Airhockey",
                          m_Settings.bFullscreen};
    m_pMainWin = icWindow::GetWindow(window);

    // create the graphics device
    m_pGxDevice = new icGXDeviceGL();
    //m_pGxDevice = new icGXDeviceDX9();

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

    m_pGxDevice->EnableFog(m_Settings.bFog, m_Settings.fogParams);

    // Set the clear color to black
    m_pGxDevice->SetClearColor(icColor::BLACK);

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
    params.bLoop = false;
    params.bPaused = false;
    params.fPitch = 1.0f;
    params.fVol = m_Settings.fSfx_vol;

    //m_Content->Load("Resource/audio/welcome_to_my_domain.wav",&m_IntroClip);
    m_Content->Load("Resource/audio/insane_laughter.wav",&m_IntroClip);
    m_IntroClip->SetParams(params);

    params.bLoop = true;
    params.fVol = m_Settings.fMusic_vol;
    m_Content->Load("Resource/audio/09 Lateralus.wav",&m_BackGroundMusic);
    m_BackGroundMusic->SetParams(params);

    // play intro audio
    if (m_Settings.bSfx)
        m_IntroClip->Play();

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
    m_SpinState.Init(m_Content);
    m_MainMenu.Init(m_Content);
    m_SettingsMenu.Init(m_Content);
    m_NetMenu.Init(m_Content);
    m_PauseMenu.Init(m_Content);
    m_StateMachine.PushState(&m_SpinState);
    m_StateMachine.PushState(&m_MainMenu);

    // start keeping time
    m_GameTime.Elapsed();

	icFontParams fontParams = { "Arial", 24 };
	m_pGxDevice->CreateFont(&m_pFont, &fontParams);
    //m_font.Initialize("Arial",24,m_Content);

    if (!ICEFAIL(res))
        return icApplication::Init(argc,args);

    return res;
}// END FUNCTION Init(int argc, char* args[])


/*! Updates the airhockey game
 *
 *  @param          ICRESULT        Success/failure of update
**/
ICRESULT Airhockey::Update(void)
{
    ICRESULT res = IC_OK;


    // current game time
    icReal elapsed = m_GameTime.Elapsed();
    elapsed = icClamp(elapsed,0.0f,0.3f);

    // this is a network game, read shit
    if (m_GameState.GameMode == GAME_MODE_HOST ||
        m_GameState.GameMode == GAME_MODE_CLIENT)
    {
        int read=0;
        char temp[256];
        Socket.read(temp,&read);
        if (read)
        {
            ReceivePacket = *((_airpacket*)temp);
        }
    }

    // current input
#ifndef __APPLE__
    icInput* pInput = icInput::Instance();
#endif

    // update the graphics device
    res = m_pGxDevice->Update();


    // update the game
    res |= m_StateMachine.Update(elapsed);

#ifndef __APPLE__
    pInput->Update();
#endif

    m_fNetTimer -= elapsed;

    if (m_fNetTimer <= 0.0f)
    {
    // this is a network game, send shit
    if (m_GameState.GameMode == GAME_MODE_HOST ||
        m_GameState.GameMode == GAME_MODE_CLIENT)
    {
        if (ICEOK(Socket.sendPacket((char*)&SendPacket,sizeof(_airpacket))))
        {
            int a =0;
        }
    }
    m_fNetTimer = 0.016f;
    }

    return res;
}// END FUNCTION Update(void)


/*! Renders the airhockey game
 *
 *  @param          ICRESULT        Success/failure of render
**/
ICRESULT Airhockey::Render(void)
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
    icInput* input = icInput::Instance();

    //static int state = 0;
    //if (input->IsDown(ICMOUSE_L))
    //{
    //    state = 1;
    //}
    //else if (input->IsPressed(ICMOUSE_L))
    //{
    //    if (state == 2)
    //        state = 3;
    //    if (state == 1)
    //        state = 2;
    //    if (state == 0)
    //        state = 4;
    //}
    //else
    //{
    //    //if (state == 2)
    //        state = 0;
    //}

    //m_pGxDevice->EnableDepthTest(false);
    //switch (state)
    //{
    //case 0: m_font.Print(10,10,icColor::WHITE,"MOUSE UP"); break;
    //case 1: m_font.Print(10,10,icColor::WHITE,"MOUSE DOWN"); break;
    //case 2: m_font.Print(10,10,icColor::WHITE,"MOUSE PRESSED"); break;
    //case 3: m_font.Print(10,10,icColor::WHITE,"MOUSE FUCKING SHIT DOUBLE CLICK"); break;
    //case 4: m_font.Print(10,10,icColor::WHITE,"MOUSE FUCKING SHIT NO DOWN FIRST"); break;
    //}

    //icReal dx = input->GetAxis(ICAXIS_MOUSE_X);
    //icReal dy = input->GetAxis(ICAXIS_MOUSE_Y);

    //m_font.Printf(10,30,icColor::WHITE,"Delta X: %lf", dx);
    //m_font.Printf(10,50,icColor::WHITE,"Delta Y: %lf", dy);
    //m_pGxDevice->EnableDepthTest(true);

    m_pGxDevice->FrameEnd(true);
    return IC_OK;
}// END FUNCTION Render(void)


/*! Quits the airhockey game
 *
 *  @param          ICRESULT        Success/failure of quit
**/
ICRESULT Airhockey::Quit(void)
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
void Airhockey::ProcessSettings(void)
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

        //val = m_SettingsFile.GetValue("Engine.Graphics.BackBufferX", "1280");
        //m_Settings.iBackbuf_x = atoi(val.ToChar());

        //val = m_SettingsFile.GetValue("Engine.Graphics.BackBufferY", "720");
        //m_Settings.iBackbuf_y = atoi(val.ToChar());


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


        // GET PADDLE SETTINGS
        val = m_SettingsFile.GetValue("Game.Paddle.ucType","1");
        m_Settings.ucPaddle_type = (uchar)atoi(val.ToChar());

        // GET NETWORK SETTINGS
        m_Settings.sLast_join_ip = 
            m_SettingsFile.GetValue("Game.Network.sConnectIP", "127.0.0.1");

        m_InitialSettings = m_Settings;
    }
}// END FUNCTION ProcessSettings(void)