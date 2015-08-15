
#include "pause_menu.h"

#include "Core/IO/icInput.h"
#include "Render/Texture/icTexture.h"

#include "skeeball.h"

extern SkeeBall& gApp;

/*! default c'tor
**/
PauseMenu::PauseMenu(void)
{
    m_uiFrame = NULL;

    m_wgtPaused = NULL;

    m_btnMainMenu = NULL;
    m_btnSettingsMenu = NULL;
    m_btnBack = NULL;

    m_pClick = NULL;
    m_pHover = NULL;

    m_bRenderTopOnly = false;
    SkipUpdate = false;
//    m_bRenderTopOnly = true;
}// END FUNCTION PauseMenu(void)


/*! default d'tor
**/
PauseMenu::~PauseMenu(void)
{
}// END FUNCTION ~PauseMenu(void)


/*! Initializes the main menu state
 *
 *  @param          pContentLoader      Pointer to the content loader
 *  @returns        ICRESULT            Success/failure of initialization
**/
ICRESULT PauseMenu::Init(icContentLoader* pContentLoader)
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
    if (ICEFAIL(m_pContent->Load("Resource/textures/UI_paused_menu.tga", &tex1)))
        return IC_FAIL_GEN;

    if (ICEFAIL(m_pContent->Load("Resource/textures/UI_paused_menu_sel.tga", &tex2)))
        return IC_FAIL_GEN;

    // load the sounds
    if (ICEFAIL(m_pContent->Load("Resource/audio/whip_01.wav", &m_pHover)))
        return IC_FAIL_GEN;

    if (ICEFAIL(m_pContent->Load("Resource/audio/menu_click.wav", &m_pClick)))
        return IC_FAIL_GEN;

    if (ICEFAIL(m_pContent->Load("Resource/audio/pause_sound.wav", &m_pPause)))
        return IC_FAIL_GEN;
    
    m_pHover->SetParams(param);
    m_pClick->SetParams(param);
    m_pPause->SetParams(param);


    icReal sx = (icReal)gApp.GetSettings().iScreen_x;
    icReal sy = (icReal)gApp.GetSettings().iScreen_y;

    icReal rat = sy/1024.0f;
    icReal mx = sx/2.0f;

    // set up UI Frame
    m_uiFrame = new icFrame(m_pContent);

    //m_uiFrame->SetTextures(tex1,tex2);

    icVector2 bl(0.0f,0.0f);
    icVector2 tl(0.0f,1.0f);
    icVector2 tr(1.0f,1.0f);
    icVector2 br(1.0f,0.0f);
    m_uiFrame->SetUVs( tl, tr, bl, br );

    icVector2 size(1280.0f,720.0f);
    icVector2 pos(640.0f,360.0f);
    icColor col(0,0,0,255);
    m_uiFrame->SetSize(size);
    m_uiFrame->SetPos(pos);
    m_uiFrame->SetColor(col);

    // pause widget
    m_wgtPaused = new icWidget(m_pContent);
    m_wgtPaused->SetTextures(tex1,tex2);
    size.Set(512.0f*rat,150.0f*rat);
    pos.Set(mx,sy*0.809f);
    //col.Set(0,0,0,255);
    m_wgtPaused->SetSize(size);
    m_wgtPaused->SetPos(pos);
    m_wgtPaused->SetColor(col);
    bl.Set(0.0f,0.699f);
    tl.Set(0.0f,1.0f);
    tr.Set(1.0f,1.0f);
    br.Set(1.0f,0.699f);
    m_wgtPaused->SetUVs(tl,tr,bl,br);
    m_wgtPaused->SetHighUVs(tl,tr,bl,br);
    m_wgtPaused->EnableHighlight(false);
    m_uiFrame->AddChild(m_wgtPaused);

    // main menu button
    m_btnMainMenu = new icButton(m_pContent);
    m_btnMainMenu->SetTextures(tex1,tex2);
    size.Set(512.0f*rat,115.0f*rat);
    pos.Set(mx,sy*0.571f);
    //col.Set(0,0,0,255);
    m_btnMainMenu->SetSize(size);
    m_btnMainMenu->SetPos(pos);
    m_btnMainMenu->SetColor(col);
    bl.Set(0.0f,0.455f);
    tl.Set(0.0f,0.687f);
    tr.Set(1.0f,0.687f);
    br.Set(1.0f,0.455f);
    m_btnMainMenu->SetUVs(tl,tr,bl,br);
    m_btnMainMenu->SetHighUVs(tl,tr,bl,br);
    m_btnMainMenu->SetSounds(m_pClick,m_pHover);
    m_uiFrame->AddChild(m_btnMainMenu);

    // settings menu button
    m_btnSettingsMenu = new icButton(m_pContent);
    m_btnSettingsMenu->SetTextures(tex1,tex2);
    size.Set(512.0f*rat,120.0f*rat);
    pos.Set(mx,sy*0.364f);
    //col.Set(0,0,0,255);
    m_btnSettingsMenu->SetSize(size);
    m_btnSettingsMenu->SetPos(pos);
    m_btnSettingsMenu->SetColor(col);
    bl.Set(0.0f,0.209f);
    tl.Set(0.0f,0.424f);
    tr.Set(1.0f,0.424f);
    br.Set(1.0f,0.209f);
    m_btnSettingsMenu->SetUVs(tl,tr,bl,br);
    m_btnSettingsMenu->SetHighUVs(tl,tr,bl,br);
    m_btnSettingsMenu->SetSounds(m_pClick,m_pHover);
    m_uiFrame->AddChild(m_btnSettingsMenu);

    // back button
    m_btnBack = new icButton(m_pContent);
    m_btnBack->SetTextures(tex1,tex2);
    size.Set(512.0f*rat,115.0f*rat);
    pos.Set(mx,sy*0.17f);
    //col.Set(0,0,0,255);
    m_btnBack->SetSize(size);
    m_btnBack->SetPos(pos);
    m_btnBack->SetColor(col);
    bl.Set(0.0f,0.0f);
    tl.Set(0.0f,0.209f);
    tr.Set(1.0f,0.209f);
    br.Set(1.0f,0.0f);
    m_btnBack->SetUVs(tl,tr,bl,br);
    m_btnBack->SetHighUVs(tl,tr,bl,br);
    m_btnBack->SetSounds(m_pClick,m_pHover);
    m_uiFrame->AddChild(m_btnBack);
    return IC_OK;
}// END FUNCTION Init(icContentLoader* pContentLoader)


/*! Update the main menu
 *
 *  @param          fDeltaTime      Elapsed time since last update
 *  @param          bFinished       Pointer to boolean
 *  @returns        ICRESULT        Success/failure of update
**/
ICRESULT PauseMenu::Update(const float fDeltaTime, bool* bFinished)
{
    if (SkipUpdate)
    {
        SkipUpdate = false;
        return IC_OK;
    }

    icInput* pInput = icInput::Instance();

    SkeeBallSettings settings = gApp.GetSettings();

    if (!m_btnBack)
        return IC_OK;

    if (m_btnMainMenu->WasPressed())
    {
        icState* state;
        state = gApp.GetStateMachine().PopState();
        state = gApp.GetStateMachine().PopState();
        state = gApp.GetStateMachine().PopState();

        gApp.GetStateMachine().PushState(state);

        m_btnMainMenu->SetDown(false);

        m_btnMainMenu->SetDown(false);
    }
    else if (m_btnSettingsMenu->WasPressed())
    {
        gApp.GetStateMachine().PushState(&gApp.m_SettingsMenu);
    }
    else if (pInput->IsPressed(ICKEY_ESCAPE) || m_btnBack->WasPressed())
    {
        m_pPause->Play();

        m_btnBack->SetDown(false);
        gApp.GetStateMachine().PopState();

        icInput::Instance()->HideCurs();
        return IC_OK;
    }

    if (!m_uiFrame)
        return IC_OK;

    ICRESULT res = m_uiFrame->Update(fDeltaTime);

    res |= m_uiFrame->ProcessInput(pInput);   

#ifdef WIIMOTE
    if (gApp.GetSettings().bWiiMote)
    {
        wiimote& wii_remote = gApp.remote;
        if (wii_remote.IsConnected())
        {
            if (m_btnMainMenu->IsHighlight() && wii_remote.Button.A())
            {
                icState* state;
                state = gApp.GetStateMachine().PopState();
                state = gApp.GetStateMachine().PopState();
                state = gApp.GetStateMachine().PopState();

                gApp.GetStateMachine().PushState(state);

                m_btnMainMenu->SetDown(false);

                m_btnMainMenu->SetDown(false);
            }
            else if (m_btnSettingsMenu->IsHighlight() && wii_remote.Button.A())
            {
                gApp.GetStateMachine().PushState(&gApp.m_SettingsMenu);
            }
            else if (m_btnBack->IsHighlight() && wii_remote.Button.A())
            {
                m_pPause->Play();

                m_btnBack->SetDown(false);
                gApp.GetStateMachine().PopState();

                icInput::Instance()->HideCurs();
                return IC_OK;
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
ICRESULT PauseMenu::Render(void)
{
    if (!m_uiFrame)
        return IC_OK;

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


/*! This gets called when the PauseMenu gets pushed on the stack
 *  and should reset anything that needs to be reset for each game
**/ 
void PauseMenu::OnPush(void)
{
    m_pPause->Play();

    icInput::Instance()->ShowCurs();

    SkipUpdate = true;
    icSoundParam param;
    //m_bRenderTopOnly = true;
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
}// END FUNCTION OnPush(void)


void PauseMenu::OnPop(void)
{
}