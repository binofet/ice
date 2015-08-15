
#include "net_menu.h"

#include "Core/IO/icInput.h"
#include "Render/Texture/icTexture.h"

#include "Network/icSocket.h"

#include "airhockey.h"

extern Airhockey& gApp;

/*! default c'tor
**/
NetMenu::NetMenu(void)
{
    m_uiFrame;

    m_wgtNetPlay;

    m_btnHost;
    icButton*           m_btnJoinIP;
    icButton*           m_btnFind;
    icButton*           m_btnJoinFirst;
    icButton*           m_btnMainMenu;

    icSoundI*           m_pClick;
    icSoundI*           m_pHover;

    m_bRenderTopOnly = false;
}// END FUNCTION NetMenu(void)


/*! default d'tor
**/
NetMenu::~NetMenu(void)
{
}// END FUNCTION ~NetMenu(void)


/*! Initializes the main menu state
 *
 *  @param          pContentLoader      Pointer to the content loader
 *  @returns        ICRESULT            Success/failure of initialization
**/
ICRESULT NetMenu::Init(icContentLoader* pContentLoader)
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
    if (ICEFAIL(m_pContent->Load("Resource/textures/UI_net_menu.tga", &tex1)))
        return IC_FAIL_GEN;

    if (ICEFAIL(m_pContent->Load("Resource/textures/UI_net_menu_sel.tga", &tex2)))
        return IC_FAIL_GEN;

    // load the sounds
    if (ICEFAIL(m_pContent->Load("Resource/audio/whip_01.wav", &m_pHover)))
        return IC_FAIL_GEN;

    if (ICEFAIL(m_pContent->Load("Resource/audio/weirdbreath.wav", &m_pClick)))
        return IC_FAIL_GEN;

    m_pHover->SetParams(param);
    m_pClick->SetParams(param);

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

    // network play widget
    m_wgtNetPlay = new icWidget(m_pContent);
    m_wgtNetPlay->SetTextures(tex1,tex2);
    size.Set(650.0f*rat,135.0f*rat);
    pos.Set(mx,sy*0.839f);
    //col.Set(0,0,0,255);
    m_wgtNetPlay->SetSize(size);
    m_wgtNetPlay->SetPos(pos);
    m_wgtNetPlay->SetColor(col);
    bl.Set(0.0f,0.861f);
    tl.Set(0.0f,1.0f);
    tr.Set(1.0f,1.0f);
    br.Set(1.0f,0.861f);
    m_wgtNetPlay->SetUVs(tl,tr,bl,br);
    m_wgtNetPlay->SetHighUVs(tl,tr,bl,br);
    m_wgtNetPlay->EnableHighlight(false);
    m_uiFrame->AddChild(m_wgtNetPlay);

    // host game button
    m_btnHost = new icButton(m_pContent);
    m_btnHost->SetTextures(tex1,tex2);
    size.Set(512.0f*rat,115.0f*rat);
    pos.Set(mx,sy*0.661f);
    //col.Set(0,0,0,255);
    m_btnHost->SetSize(size);
    m_btnHost->SetPos(pos);
    m_btnHost->SetColor(col);
    bl.Set(0.0f,0.646f);
    tl.Set(0.0f,0.811f);
    tr.Set(1.0f,0.811f);
    br.Set(1.0f,0.646f);
    m_btnHost->SetUVs(tl,tr,bl,br);
    m_btnHost->SetHighUVs(tl,tr,bl,br);
    m_btnHost->SetSounds(m_pClick,m_pHover);
    m_uiFrame->AddChild(m_btnHost);

    // join ip button
    m_btnJoinIP = new icButton(m_pContent);
    m_btnJoinIP->SetTextures(tex1,tex2);
    size.Set(512.0f*rat,120.0f*rat);
    pos.Set(mx,sy*0.504f);
    //col.Set(0,0,0,255);
    m_btnJoinIP->SetSize(size);
    m_btnJoinIP->SetPos(pos);
    m_btnJoinIP->SetColor(col);
    bl.Set(0.0f,0.436f);
    tl.Set(0.0f,0.611f);
    tr.Set(1.0f,0.611f);
    br.Set(1.0f,0.436f);
    m_btnJoinIP->SetUVs(tl,tr,bl,br);
    m_btnJoinIP->SetHighUVs(tl,tr,bl,br);
    m_btnJoinIP->SetSounds(m_pClick,m_pHover);
    m_uiFrame->AddChild(m_btnJoinIP);

    // find local button
    m_btnFind = new icButton(m_pContent);
    m_btnFind->SetTextures(tex1,tex2);
    size.Set(512.0f*rat,115.0f*rat);
    pos.Set(mx,sy*0.33f);
    //col.Set(0,0,0,255);
    m_btnFind->SetSize(size);
    m_btnFind->SetPos(pos);
    m_btnFind->SetColor(col);
    bl.Set(0.0f,0.238f);
    tl.Set(0.0f,0.406f);
    tr.Set(1.0f,0.406f);
    br.Set(1.0f,0.238f);
    m_btnFind->SetUVs(tl,tr,bl,br);
    m_btnFind->SetHighUVs(tl,tr,bl,br);
    m_btnFind->SetSounds(m_pClick,m_pHover);
    m_uiFrame->AddChild(m_btnFind);

    // main menu button
    m_btnMainMenu = new icButton(m_pContent);
    m_btnMainMenu->SetTextures(tex1,tex2);
    size.Set(512.0f*rat,100.0f*rat);
    pos.Set(mx,sy*0.135f);
    //col.Set(0,0,0,255);
    m_btnMainMenu->SetSize(size);
    m_btnMainMenu->SetPos(pos);
    m_btnFind->SetColor(col);
    bl.Set(0.0f,0.039f);
    tl.Set(0.0f,0.189f);
    tr.Set(1.0f,0.189f);
    br.Set(1.0f,0.039f);
    m_btnMainMenu->SetUVs(tl,tr,bl,br);
    m_btnMainMenu->SetHighUVs(tl,tr,bl,br);
    m_btnMainMenu->SetSounds(m_pClick,m_pHover);
    m_uiFrame->AddChild(m_btnMainMenu);
    return IC_OK;
}// END FUNCTION Init(icContentLoader* pContentLoader)


/*! Update the main menu
 *
 *  @param          fDeltaTime      Elapsed time since last update
 *  @param          bFinished       Pointer to boolean
 *  @returns        ICRESULT        Success/failure of update
**/
ICRESULT NetMenu::Update(const float fDeltaTime, bool* bFinished)
{
    icInput* pInput = icInput::Instance();

    if (pInput->IsPressed(ICKEY_ESCAPE)  || m_btnMainMenu->WasPressed())
    {
        m_btnMainMenu->SetDown(false);
        gApp.GetStateMachine().PopState();

        // yep, hack
        icState* temp = gApp.GetStateMachine().PopState();
        gApp.GetStateMachine().PushState(temp);
    }
    else if (m_btnHost->WasPressed())
    {
        m_btnHost->SetDown(false);
        m_btnHost->SetDown(false);

        gApp.m_GameState.GameMode = GAME_MODE_HOST;
        gApp.GetStateMachine().PopState();
        icState* hack = gApp.GetStateMachine().PopState();
        gApp.GetStateMachine().PushState(hack);
        gApp.GetStateMachine().PushState(&gApp.m_GameState);
    }
    else if (m_btnJoinIP->WasPressed())
    {
        // TODO: start the find process, display the games in a selectable list!

        m_btnFind->SetDown(false);
        m_btnFind->SetDown(false);

        gApp.m_GameState.GameMode = GAME_MODE_CLIENT;
        gApp.GetStateMachine().PopState();
        icState* hack = gApp.GetStateMachine().PopState();
        gApp.GetStateMachine().PushState(hack);
        gApp.GetStateMachine().PushState(&gApp.m_GameState);
    }
    else if (m_btnFind->WasPressed())
    {
        //// TODO: start the find process, display the games in a selectable list!

        //m_btnFind->SetDown(false);
        //m_btnFind->SetDown(false);

        //gApp.m_GameState.GameMode = GAME_MODE_CLIENT;
        //gApp.GetStateMachine().PopState();
        //icState* hack = gApp.GetStateMachine().PopState();
        //gApp.GetStateMachine().PushState(hack);
        //gApp.GetStateMachine().PushState(&gApp.m_GameState);
    }

    if (!m_uiFrame)
        return IC_OK;

    ICRESULT res = m_uiFrame->Update(fDeltaTime);

    res |= m_uiFrame->ProcessInput(pInput);

    return res;
}// END FUNCTION Update(const float fDeltaTime, bool* bFinished)


/*! Render the main menu
 *
 *  @returns        ICRESULT        Success/failure of render
**/
ICRESULT NetMenu::Render(void)
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



void NetMenu::OnPush(void)
{
}

void NetMenu::OnPop(void)
{
}