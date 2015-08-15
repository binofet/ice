
#include "main_menu.h"

#include "Core/IO/icInput.h"
#include "Render/Texture/icTexture.h"

#include "skeeball.h"

extern SkeeBall& gApp;

/*! default c'tor
**/
MainMenu::MainMenu(void)
{
    m_uiFrame = NULL;
    m_btnOnePlayer = NULL;
    m_btnTwoPlayer = NULL;
    m_btnNetPlay = NULL;
    m_btnSettings = NULL;
    m_btnExit = NULL;

    m_pHover = NULL;
    m_pClick = NULL;

    m_bRenderTopOnly = true;
    SkipUpdate = false;
}// END FUNCTION MainMenu(void)


/*! default d'tor
**/
MainMenu::~MainMenu(void)
{
}// END FUNCTION ~MainMenu(void)


/*! Initializes the main menu state
 *
 *  @param          pContentLoader      Pointer to the content loader
 *  @returns        ICRESULT            Success/failure of initialization
**/
ICRESULT MainMenu::Init(icContentLoader* pContentLoader)
{
    if (!pContentLoader)
        return IC_FAIL_GEN;

    m_pContent = pContentLoader;

    icTexture *tex1 = NULL;
    icTexture *tex2 = NULL;

    icSoundParam param;
    param.bLoop = false;
    param.fPitch = 1.0f;
    param.fVol = gApp.GetSettings().fSfx_vol;

    // load the textures
    if (ICEFAIL(m_pContent->Load("Resource/textures/UI_mainmenu.tga", &tex1)))
        return IC_FAIL_GEN;

    if (ICEFAIL(m_pContent->Load("Resource/textures/UI_mainmenu_sel.tga", &tex2)))
        return IC_FAIL_GEN;

    // load the sounds
    if (ICEFAIL(m_pContent->Load("Resource/audio/whip_01.wav", &m_pHover)))
        return IC_FAIL_GEN;

    if (ICEFAIL(m_pContent->Load("Resource/audio/menu_click.wav", &m_pClick)))
        return IC_FAIL_GEN;

    m_pHover->SetParams(param);
    m_pClick->SetParams(param);

    // set up UI Frame
    m_uiFrame = new icFrame(m_pContent);

    //m_uiFrame->SetTextures(tex1,tex2);

    icReal sx = (icReal)gApp.GetSettings().iScreen_x;
    icReal sy = (icReal)gApp.GetSettings().iScreen_y;

    icReal rat = sy/1024.0f;
    icReal mx = sx/2.0f;

    icVector2 bl(0.0f,0.0f);
    icVector2 tl(0.0f,1.0f);
    icVector2 tr(1.0f,1.0f);
    icVector2 br(1.0f,0.0f);
    m_uiFrame->SetUVs( tl, tr, bl, br );

    icVector2 size(sx,sy);
    icVector2 pos(mx,360.0f);
    icColor col(0,0,0,0);
    m_uiFrame->SetSize(size);
    m_uiFrame->SetPos(pos);
    m_uiFrame->SetColor(col);

    // add the buttons to the UI frame
    // one player button
    m_btnOnePlayer = new icButton(m_pContent);
    m_btnOnePlayer->SetTextures(tex1,tex2);
    size.Set(512.0f*rat,137.0f*rat);
    pos.Set(mx,sy*0.4f);
    col.Set(0,0,0,255);
    m_btnOnePlayer->SetSize(size);
    m_btnOnePlayer->SetPos(pos);
    m_btnOnePlayer->SetColor(col);
    bl.Set(0.0f,0.857f);
    tl.Set(0.0f,1.0f);
    tr.Set(1.0f,1.0f);
    br.Set(1.0f,0.857f);
    m_btnOnePlayer->SetUVs(tl,tr,bl,br);
    m_btnOnePlayer->SetHighUVs(tl,tr,bl,br);
    m_btnOnePlayer->SetSounds(m_pClick,m_pHover);
    m_uiFrame->AddChild(m_btnOnePlayer);

    // settings button
    m_btnSettings = new icButton(m_pContent);
    m_btnSettings->SetTextures(tex1,tex2);
    size.Set(512.0f*rat,128.0f*rat);
    pos.Set(mx,sy*0.257f);
    //col.Set(0,0,0,255);
    m_btnSettings->SetSize(size);
    m_btnSettings->SetPos(pos);
    m_btnSettings->SetColor(col);
    tl.Set(0.0f,0.322f);
    tr.Set(1.0f,0.322f);
    bl.Set(0.0f,0.163f);
    br.Set(1.0f,0.163f);
    m_btnSettings->SetUVs(tl,tr,bl,br);
    m_btnSettings->SetHighUVs(tl,tr,bl,br);
    m_btnSettings->SetSounds(m_pClick,m_pHover);
    m_uiFrame->AddChild(m_btnSettings);

    // exit button
    m_btnExit = new icButton(m_pContent);
    m_btnExit->SetTextures(tex1,tex2);
    size.Set(512.0f*rat,125.0f*rat);
    pos.Set(mx,sy*0.093f);
    //col.Set(0,0,0,255);
    m_btnExit->SetSize(size);
    m_btnExit->SetPos(pos);
    m_btnExit->SetColor(col);
    tl.Set(0.0f,0.123f);
    tr.Set(1.0f,0.123f);
    bl.Set(0.0f,0.0f);
    br.Set(1.0f,0.0f);
    m_btnExit->SetUVs(tl,tr,bl,br);
    m_btnExit->SetHighUVs(tl,tr,bl,br);
    m_btnExit->SetSounds(m_pClick,m_pHover);
    m_uiFrame->AddChild(m_btnExit);

    return IC_OK;
}// END FUNCTION Init(icContentLoader* pContentLoader)


/*! Update the main menu
 *
 *  @param          fDeltaTime      Elapsed time since last update
 *  @param          bFinished       Pointer to boolean
 *  @returns        ICRESULT        Success/failure of update
**/
ICRESULT MainMenu::Update(const float fDeltaTime, bool* bFinished)
{
    if (SkipUpdate)
    {
        m_btnOnePlayer->SetDown(false);
        //m_btnTwoPlayer->SetDown(false);
        m_btnSettings->SetDown(false);
        //m_btnNetPlay->SetDown(false);
        SkipUpdate = false;
        return IC_OK;
    }

    ICRESULT res = m_uiFrame->Update(fDeltaTime);

    icInput* pInput = icInput::Instance();

    res |= m_uiFrame->ProcessInput(pInput);

    if (m_btnOnePlayer->WasPressed())
    {
        m_btnOnePlayer->SetDown(false);
        m_btnOnePlayer->SetDown(false);

        gApp.GetStateMachine().PushState(&gApp.m_GameState);
    }
    else if (m_btnSettings->WasPressed())
    {
        m_bRenderTopOnly = false;
        gApp.GetStateMachine().PushState(&gApp.m_SettingsMenu);
    }
    else if (pInput->IsPressed(ICKEY_ESCAPE) || m_btnExit->WasPressed())
    {
        // HACK
        exit(0);
    }

#ifdef WIIMOTE
    if (gApp.GetSettings().bWiiMote)
    {
        wiimote& wii_remote = gApp.remote;
        if (wii_remote.IsConnected())
        {
            if (wii_remote.Button.Home())
            {
                exit(0);
            }

            if (m_btnOnePlayer->IsHighlight() && wii_remote.Button.A())
            {
                m_btnOnePlayer->SetDown(false);
                m_btnOnePlayer->SetDown(false);

                gApp.GetStateMachine().PushState(&gApp.m_GameState);
            }
            else if (m_btnSettings->IsHighlight() && wii_remote.Button.A())
            {
                m_bRenderTopOnly = false;
                gApp.GetStateMachine().PushState(&gApp.m_SettingsMenu);
            }
            else if (m_btnExit->IsHighlight() && wii_remote.Button.A())
            {
                exit(0);
            }

        }
    }
#endif

    return res;
}// END FUNCTION Update(const float fDeltaTime, bool* bFinished)


/*! Render the main menu
 *
 *  @returns        ICRESULT        Success/failure of render
**/
ICRESULT MainMenu::Render(void)
{
    icGXDevice* pDev = m_pContent->GetDevice();

    icMatrix44 temp;
    temp.Identity();
    // set projection and view matrix
    pDev->SetProjection(pDev->GetOrtho());
    pDev->SetViewMatrix(temp);

    // render the UI
    m_uiFrame->Render();

    return IC_OK;
}// END FUNCTION Render(void)


/*! This gets called when the MainMenu gets pushed on the stack
 *  and should reset anything that needs to be reset for each game
**/
void MainMenu::OnPush(void)
{
    icSoundParam param;
    m_bRenderTopOnly = true;
    if (m_pHover)
    {
        param = m_pHover->GetParams();
        param.fVol = gApp.GetSettings().fSfx_vol;
        m_pHover->SetParams(param);
    }
    if (m_pClick)
    {
        param = m_pClick->GetParams();
        param.fVol = gApp.GetSettings().fSfx_vol;
        m_pClick->SetParams(param);
    }

    SkipUpdate = true;
}// END FUNCTION OnPush(void)


/*! blas
**/
void MainMenu::OnPop(void)
{
    m_bRenderTopOnly = true;
}// END FUNCTION OnPop(void)
