
#include "settings_menu.h"

#include "Core/IO/icInput.h"
#include "Render/Texture/icTexture.h"

#include "airhockey.h"

extern Airhockey& gApp;

/*! default c'tor
**/
SettingsMenu::SettingsMenu(void)
{
    m_uiFrame = NULL;

    m_ptxtSettings = NULL;
    m_ptxtSfx = NULL;
    m_ptxtMusic = NULL;
    m_ptxtPaddle = NULL;

    m_pbtnBack = NULL;

    m_slidMusicVol = NULL;
    m_slidPaddle = NULL;
    m_slidSfxVol = NULL;

    m_pClick = NULL;
    m_pHover = NULL;

    m_bRenderTopOnly = false;
}// END FUNCTION SettingsMenu(void)


/*! default d'tor
**/
SettingsMenu::~SettingsMenu(void)
{
}// END FUNCTION ~SettingsMenu(void)


/*! Initializes the main menu state
 *
 *  @param          pContentLoader      Pointer to the content loader
 *  @returns        ICRESULT            Success/failure of initialization
**/
ICRESULT SettingsMenu::Init(icContentLoader* pContentLoader)
{
    if (!pContentLoader)
        return IC_FAIL_GEN;

    m_pContent = pContentLoader;

    icTexture *tex1 = NULL;
    icTexture *tex2 = NULL;
    icTexture* bone_theme = NULL;

    icSoundParam param;
    param.bLoop = false;
    param.fPitch = 1.0f;
    param.fVol = gApp.GetSettings().fSfx_vol;

    // load the textures
    if (ICEFAIL(m_pContent->Load("Resource/textures/UI_settings_menu.tga", &tex1)))
        return IC_FAIL_GEN;

    if (ICEFAIL(m_pContent->Load("Resource/textures/UI_settings_menu_sel.tga", &tex2)))
        return IC_FAIL_GEN;

    if (ICEFAIL(m_pContent->Load("Resource/textures/UI_bone_theme.tga", &bone_theme)))
        return IC_FAIL_GEN;

    // load the sounds
    if (ICEFAIL(m_pContent->Load("Resource/audio/whip_01.wav", &m_pHover)))
        return IC_FAIL_GEN;

    if (ICEFAIL(m_pContent->Load("Resource/audio/weirdbreath.wav", &m_pClick)))
        return IC_FAIL_GEN;

    if (ICEFAIL(m_pContent->Load("Resource/models/paddle.icm",&m_Paddles[0])))
        return IC_FAIL_GEN;

    //if (ICEFAIL(m_pContent->Load("Resource/models/paddle.icm",&m_Paddles[0])))
    //    return IC_FAIL_GEN;

    //if (ICEFAIL(m_pContent->Load("Resource/models/paddle.icm",&m_Paddles[0])))
    //    return IC_FAIL_GEN;

    m_pHover->SetParams(param);
    m_pClick->SetParams(param);

    icReal sx = (icReal)gApp.GetSettings().iScreen_x;
    icReal sy = (icReal)gApp.GetSettings().iScreen_y;

    icReal rat = sy/1024.0f;
    icReal mx = sx/2.0f;

    // set up UI Frame
    m_uiFrame = new icFrame(m_pContent);

    //m_uiFrame->SetTextures(tex1,tex2);

    icVector2 bl(0.0f,0.733f);
    icVector2 tl(0.0f,1.0f);
    icVector2 tr(1.0f,1.0f);
    icVector2 br(1.0f,0.733f);
    //m_uiFrame->SetUVs( tl, tr, bl, br );

    icVector2 size(sx,sy);
    icVector2 pos(mx,sy*0.5f);
    icColor col(0,0,0,255);
    m_uiFrame->SetSize(size);
    m_uiFrame->SetPos(pos);
    m_uiFrame->SetColor(col);


    // SFX Slider
    m_slidSfxVol = new icSlider(m_pContent);
    m_slidSfxVol->SetTextures(bone_theme,NULL);
    size.Set(512.0f*rat,71.0f*rat);
    pos.Set(mx + mx*0.35f,sy*0.609f);
    //col.Set(0,0,0,255);
    m_slidSfxVol->SetSize(size);
    m_slidSfxVol->SetPos(pos);
    m_slidSfxVol->SetColor(col);
    bl.Set(0.0f,0.0f);
    tl.Set(0.0f,0.138f);
    tr.Set(1.0f,0.138f);
    br.Set(1.0f,0.0f);
    m_slidSfxVol->SetUVs(tl,tr,bl,br);
    m_slidSfxVol->SetHighUVs(tl,tr,bl,br);
    m_slidSfxVol->EnableHighlight(false);
    m_slidSfxVol->SetMinMax(0,1000);
    bl.Set(0.916f,0.156f);
    tl.Set(0.916f,0.402f);
    tr.Set(1.0f,0.402f);
    br.Set(1.0f,0.156f);
    m_slidSfxVol->SetTickUVs(tl,tr,bl,br);
    m_slidSfxVol->SetTickHighUVs(tl,tr,bl,br);
    m_slidSfxVol->SetVal((int)(gApp.GetSettings().fSfx_vol*1000.0f));
    size.Set(44.0f*rat,132.0f*rat);
    m_slidSfxVol->SetTickSize(size);
    m_uiFrame->AddChild(m_slidSfxVol);

    // Music Slider
    m_slidMusicVol = new icSlider(m_pContent);
    m_slidMusicVol->SetTextures(bone_theme,NULL);
    size.Set(512.0f*rat,71.0f*rat);
    pos.Set(mx + mx*0.35f,sy*0.459f);
    //col.Set(0,0,0,255);
    m_slidMusicVol->SetSize(size);
    m_slidMusicVol->SetPos(pos);
    m_slidMusicVol->SetColor(col);
    bl.Set(0.0f,0.0f);
    tl.Set(0.0f,0.138f);
    tr.Set(1.0f,0.138f);
    br.Set(1.0f,0.0f);
    m_slidMusicVol->SetUVs(tl,tr,bl,br);
    m_slidMusicVol->SetHighUVs(tl,tr,bl,br);
    m_slidMusicVol->EnableHighlight(false);
    m_slidMusicVol->SetMinMax(0,1000);
    bl.Set(0.916f,0.156f);
    tl.Set(0.916f,0.402f);
    tr.Set(1.0f,0.402f);
    br.Set(1.0f,0.156f);
    m_slidMusicVol->SetTickUVs(tl,tr,bl,br);
    m_slidMusicVol->SetTickHighUVs(tl,tr,bl,br);
    m_slidMusicVol->SetVal((int)(gApp.GetSettings().fMusic_vol*1000.0f));
    size.Set(44.0f*rat,132.0f*rat);
    m_slidMusicVol->SetTickSize(size);
    m_uiFrame->AddChild(m_slidMusicVol);

    // Paddle Slider
    m_slidPaddle = new icSlider(m_pContent);
    m_slidPaddle->SetTextures(bone_theme,NULL);
    size.Set(512.0f*rat,71.0f*rat);
    pos.Set(mx + mx*0.35f,sy*0.309f);
    //col.Set(0,0,0,255);
    m_slidPaddle->SetSize(size);
    m_slidPaddle->SetPos(pos);
    m_slidPaddle->SetColor(col);
    bl.Set(0.0f,0.0f);
    tl.Set(0.0f,0.138f);
    tr.Set(1.0f,0.138f);
    br.Set(1.0f,0.0f);
    m_slidPaddle->SetUVs(tl,tr,bl,br);
    m_slidPaddle->SetHighUVs(tl,tr,bl,br);
    m_slidPaddle->EnableHighlight(false);
    m_slidPaddle->SetMinMax(0,2);
    bl.Set(0.916f,0.156f);
    tl.Set(0.916f,0.402f);
    tr.Set(1.0f,0.402f);
    br.Set(1.0f,0.156f);
    m_slidPaddle->SetTickUVs(tl,tr,bl,br);
    m_slidPaddle->SetTickHighUVs(tl,tr,bl,br);
    m_slidPaddle->SetVal((int)(gApp.GetSettings().ucPaddle_type-1));
    size.Set(44.0f*rat,132.0f*rat);
    m_slidPaddle->SetTickSize(size);
    m_uiFrame->AddChild(m_slidPaddle);

    // Settings widget
    m_ptxtSettings = new icWidget(m_pContent);
    m_ptxtSettings->SetTextures(tex1,tex2);
    size.Set(650.0f*rat,175.0f*rat);
    pos.Set(mx,sy*0.799f);
    //col.Set(0,0,0,255);
    m_ptxtSettings->SetSize(size);
    m_ptxtSettings->SetPos(pos);
    m_ptxtSettings->SetColor(col);
    bl.Set(0.0f,0.826f);
    tl.Set(0.0f,1.0f);
    tr.Set(1.0f,1.0f);
    br.Set(1.0f,0.826f);
    m_ptxtSettings->SetUVs(tl,tr,bl,br);
    m_ptxtSettings->SetHighUVs(tl,tr,bl,br);
    m_ptxtSettings->EnableHighlight(false);
    m_uiFrame->AddChild(m_ptxtSettings);

    // SFX Widget
    m_ptxtSfx = new icWidget(m_pContent);
    m_ptxtSfx->SetTextures(tex1,tex2);
    size.Set(512.0f*rat,115.0f*rat);
    pos.Set(mx - mx*0.15f,sy*0.609f);
    //col.Set(0,0,0,255);
    m_ptxtSfx->SetSize(size);
    m_ptxtSfx->SetPos(pos);
    m_ptxtSfx->SetColor(col);
    bl.Set(0.0f,0.627f);
    tl.Set(0.0f,0.826f);
    tr.Set(1.0f,0.826f);
    br.Set(1.0f,0.627f);
    m_ptxtSfx->SetUVs(tl,tr,bl,br);
    m_ptxtSfx->SetHighUVs(tl,tr,bl,br);
    m_ptxtSfx->EnableHighlight(false);
    m_uiFrame->AddChild(m_ptxtSfx);

    // Music Widget
    m_ptxtMusic = new icWidget(m_pContent);
    m_ptxtMusic->SetTextures(tex1,tex2);
    size.Set(512.0f*rat,115.0f*rat);
    pos.Set(mx - mx*0.21f,sy*0.459f);
    //col.Set(0,0,0,255);
    m_ptxtMusic->SetSize(size);
    m_ptxtMusic->SetPos(pos);
    m_ptxtMusic->SetColor(col);
    bl.Set(0.0f,0.420f);
    tl.Set(0.0f,0.627f);
    tr.Set(1.0f,0.627f);
    br.Set(1.0f,0.420f);
    m_ptxtMusic->SetUVs(tl,tr,bl,br);
    m_ptxtMusic->SetHighUVs(tl,tr,bl,br);
    m_ptxtMusic->EnableHighlight(false);
    m_uiFrame->AddChild(m_ptxtMusic);

    // Paddle widget
    m_ptxtPaddle = new icWidget(m_pContent);
    m_ptxtPaddle->SetTextures(tex1,tex2);
    size.Set(512.0f*rat,115.0f*rat);
    pos.Set(mx - mx*0.225f,sy*0.309f);
    //col.Set(0,0,0,255);
    m_ptxtPaddle->SetSize(size);
    m_ptxtPaddle->SetPos(pos);
    m_ptxtPaddle->SetColor(col);
    bl.Set(0.0f,0.221f);
    tl.Set(0.0f,0.420f);
    tr.Set(1.0f,0.420f);
    br.Set(1.0f,0.221f);
    m_ptxtPaddle->SetUVs(tl,tr,bl,br);
    m_ptxtPaddle->SetHighUVs(tl,tr,bl,br);
    m_ptxtPaddle->EnableHighlight(false);
    m_uiFrame->AddChild(m_ptxtPaddle);

    // back button
    m_pbtnBack = new icButton(m_pContent);
    m_pbtnBack->SetTextures(tex1,tex2);
    size.Set(512.0f*rat,115.0f*rat);
    pos.Set(mx,sy*0.109f);
    //col.Set(0,0,0,255);
    m_pbtnBack->SetSize(size);
    m_pbtnBack->SetPos(pos);
    m_pbtnBack->SetColor(col);
    bl.Set(0.0f,0.0f);
    tl.Set(0.0f,0.221f);
    tr.Set(1.0f,0.221f);
    br.Set(1.0f,0.0f);
    m_pbtnBack->SetUVs(tl,tr,bl,br);
    m_pbtnBack->SetHighUVs(tl,tr,bl,br);
    m_pbtnBack->SetSounds(m_pClick,m_pHover);
    m_uiFrame->AddChild(m_pbtnBack);

    return IC_OK;
}// END FUNCTION Init(icContentLoader* pContentLoader)


/*! Update the main menu
 *
 *  @param          fDeltaTime      Elapsed time since last update
 *  @param          bFinished       Pointer to boolean
 *  @returns        ICRESULT        Success/failure of update
**/
ICRESULT SettingsMenu::Update(const float fDeltaTime, bool* bFinished)
{
    icInput* pInput = icInput::Instance();

    AirSettings settings = gApp.GetSettings();

    if (pInput->IsPressed(ICKEY_ESCAPE) || m_pbtnBack->WasPressed())
    {
        m_pbtnBack->SetDown(false);
        gApp.GetStateMachine().PopState();

        // yep, hack
        icState* temp = gApp.GetStateMachine().PopState();
        gApp.GetStateMachine().PushState(temp);
        return IC_OK;
    }

    if (!m_uiFrame)
        return IC_OK;

    ICRESULT res = m_uiFrame->Update(fDeltaTime);

    res |= m_uiFrame->ProcessInput(pInput);

    settings.fMusic_vol = (float)m_slidMusicVol->GetVal()/1000.0f;
    settings.fSfx_vol = (float)m_slidSfxVol->GetVal()/1000.0f;

    gApp.UpdateSettings(settings);
    //settings.ucPaddle_type = 

    return res;
}// END FUNCTION Update(const float fDeltaTime, bool* bFinished)


/*! Render the main menu
 *
 *  @returns        ICRESULT        Success/failure of render
**/
ICRESULT SettingsMenu::Render(void)
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

    //temp.Scale(0.5f,0.5f,0.5f);
    //temp.MakeTranslation(200,200.0f,10.0f);
    //m_Paddles[0]->Render(temp);

    return IC_OK;
}// END FUNCTION Render(void)


/*! This gets called when the SettingsMenu gets pushed on the stack
 *  and should reset anything that needs to be reset for each game
**/ 
void SettingsMenu::OnPush(void)
{
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


void SettingsMenu::OnPop(void)
{
}