//////////////////////////////////////////////////////////////////////////////
// LOCAL INCLUDES
#include "gamestate.h"
#include "gameplay.h"
#include "PlayerInput.h"
#include "NetworkInput.h"
#include "AiInput.h"

#include "airhockey.h"

//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "Core/IO/icInput.h"
#include "Math/icRand.h"



icVector2 bl[]={
    icVector2(0.0f,0.0f),
    icVector2(0.119f,0.0f),
    icVector2(0.189f,0.0f),
    icVector2(0.281f,0.0f),
    icVector2(0.400f,0.0f),
    icVector2(0.500f,0.0f),
    icVector2(0.600f,0.0f),
    icVector2(0.700f,0.0f),
    icVector2(0.800f,0.0f),
    icVector2(0.900f,0.0f)
};

icVector2 tl[]={
    icVector2(0.0f,1.0f),
    icVector2(0.119f,1.0f),
    icVector2(0.189f,1.0f),
    icVector2(0.281f,1.0f),
    icVector2(0.400f,1.0f),
    icVector2(0.500f,1.0f),
    icVector2(0.600f,1.0f),
    icVector2(0.700f,1.0f),
    icVector2(0.800f,1.0f),
    icVector2(0.900f,1.0f)
};

icVector2 tr[]={
    icVector2(0.108f,1.0f),
    icVector2(0.192f,1.0f),
    icVector2(0.285f,1.0f),
    icVector2(0.395f,1.0f),
    icVector2(0.500f,1.0f),
    icVector2(0.600f,1.0f),
    icVector2(0.700f,1.0f),
    icVector2(0.800f,1.0f),
    icVector2(0.900f,1.0f),
    icVector2(1.0f,1.0f)
};

icVector2 br[]={
    icVector2(0.108f,0.0f),
    icVector2(0.192f,0.0f),
    icVector2(0.285f,0.0f),
    icVector2(0.395f,0.0f),
    icVector2(0.500f,0.0f),
    icVector2(0.600f,0.0f),
    icVector2(0.700f,0.0f),
    icVector2(0.800f,0.0f),
    icVector2(0.900f,0.0f),
    icVector2(1.0f,0.0f)
};

extern Airhockey& gApp;

/*! c'tor
**/
GameState::GameState(void)
{
    m_pContent = NULL;
    m_pGroundTex = NULL;
    m_pGround = NULL;

    m_World = NULL;
    GameMode = GAME_MODE_SINGLE;
    m_pFont = nullptr;

    m_wgtWin = NULL;
    m_texPlyr1 = NULL;
    m_texPlyr2 = NULL;

    m_fWinTimer = 0.0f;
}// END FUNCTION GameState(void)


/*! d'tor
**/
GameState::~GameState()
{
    if (m_World)
        delete m_World;
}// END FUNCTION ~GameState()


// HACK!
const icReal halfx = 500.0;
const icReal halfy = 500.0;
const int numrow = 50;
const int numcol = 50;
const float edgeDist = (halfx*2)/(numrow-1);
icVector3 cam_pos(0.0f,0.750f,-1.54f);
icVector3 look_pos(0.0f,0.0f,0.0f);


/*! Initializes the games state
 *
 *  @param          pContentLoader      Pointer to main content loader
 *  @returns        ICRESULT            Success/failure/warnings of init
**/
ICRESULT GameState::Init(icContentLoader* pContentLoader)
{
    ICRESULT res = IC_OK;

    if (!pContentLoader)
        return IC_FAIL_GEN;

    m_pContent = pContentLoader;

    icGXDevice* pDev = m_pContent->GetDevice();

    m_Transform.Identity();

    // get us a camera
    icCreatePerspective((40.0f*IC_PI/180.0f),1280.0f/720.0f,1.0f,10000000.0f, m_ChaseCam.GetProjection());
    icCreatePerspective((40.0f*IC_PI/180.0f),1280.0f/720.0f,1.0f,10000000.0f, m_Camera.GetProjection());

    icCreateLookAt(cam_pos,look_pos,icVector3(0.0f,1.0f,0.0f),m_Camera.GetViewMat());
    m_ChaseCam.GetViewMat().MakeTranslation(0.0f,0.0f,0.0f);
    m_ChaseCam.SetChaseXform(&m_Transform);
    

    res |= m_pContent->Load("Resource/audio/excelent.wav",&m_scoreSnd);
    icSoundParam param;
    param.bLoop = false;
    param.fPitch = 1.0f;
    param.fVol = gApp.GetSettings().fSfx_vol;
    m_scoreSnd->SetParams(param);

    // create ground
    res |= CreateGround();

    // physics
    res |= InitPhysics();

    // Initialize table
    res |= m_Table.Init(m_pContent,m_World);

    // Initialize puck
    res |= m_Puck.Init(m_pContent,m_World);

    // Initialize paddles
    res |= m_Paddles[0].Init(m_pContent,m_World);
    res |= m_Paddles[1].Init(m_pContent,m_World);

    m_Paddles[0].player = PLAYER_ONE;
    m_Paddles[1].player = PLAYER_TWO;


	icFontParams fontParams = { "Arial", 36 };
	res |= m_pContent->GetDevice()->CreateFont(&m_pFont, &fontParams);
    //res |= m_Font.Initialize("Arial",36,m_pContent);

    icTexture* tex1;
    res |= m_pContent->Load("Resource/textures/UI_numbers.tga",&tex1);

    res |= m_pContent->Load("Resource/textures/UI_succubus_1.tga",&m_texPlyr1);
    res |= m_pContent->Load("Resource/textures/UI_succubus_2.tga",&m_texPlyr2);

    icReal sx = (icReal)gApp.GetSettings().iScreen_x;
    icReal sy = (icReal)gApp.GetSettings().iScreen_y;

    icReal rat = sy/1024.0f;
    icReal mx = sx/2.0f;

    // player ones score board
    m_wgtScore[PLAYER_ONE] = new icWidget(m_pContent);
    m_wgtScore[PLAYER_ONE]->SetTextures(tex1);
    icVector2 bl(0.0f,0.0f);
    icVector2 tl(0.0f,1.0f);
    icVector2 tr(0.108f,1.0f);
    icVector2 br(0.108f,0.0f);
    m_wgtScore[PLAYER_ONE]->SetUVs(tl,tr,bl,br);
    m_wgtScore[PLAYER_ONE]->SetSize(icVector2(sy*0.12f,sy*0.14f));
    m_wgtScore[PLAYER_ONE]->SetPos(icVector2(sx*0.88f,sy*0.15f));

    // player twos score board
    m_wgtScore[PLAYER_TWO] = new icWidget(m_pContent);
    m_wgtScore[PLAYER_TWO]->SetTextures(tex1);
    bl.Set(0.0f,0.0f);
    tl.Set(0.0f,1.0f);
    tr.Set(0.108f,1.0f);
    br.Set(0.108f,0.0f);
    m_wgtScore[PLAYER_TWO]->SetUVs(tl,tr,bl,br);
    m_wgtScore[PLAYER_TWO]->SetSize(icVector2(sy*0.12f,sy*0.14f));
    m_wgtScore[PLAYER_TWO]->SetPos(icVector2(sx*0.78f,0.70f));

    // winner widget
    m_wgtWin = new icWidget(m_pContent);
    m_wgtWin->SetTextures(m_texPlyr1);
    bl.Set(0.0f,0.0f);
    tl.Set(0.0f,1.0f);
    tr.Set(1.0f,1.0f);
    br.Set(1.0f,0.0f);
    m_wgtWin->SetUVs(tl,tr,bl,br);
    m_wgtWin->SetSize(icVector2(sy*0.75f,sy*0.75f));
    m_wgtWin->SetPos(icVector2(mx,sy*0.5f));

    return res;
}// END FUNCTION Init(icContentLoader* pContentLoader)


/*! Creates the ground object
 *
 *  @param          ICRESULT        Success/failure of ground creation
**/
ICRESULT GameState::CreateGround(void)
{
    ICRESULT res = IC_OK;

    icGXDevice* pDev = m_pContent->GetDevice();

    icVertDef vert_def;
    vert_def.numPrims = (numrow-1)*(numcol-1)*2;
    vert_def.uiVertSize = sizeof(ICVRT_DIF);
    vert_def.uiNumVerts = (numrow-1)*(numcol-1)*4;
    vert_def.usage = IC_VERT_USAGE_STATIC;
    vert_def.primType = IC_PRIM_TLIST;
    vert_def.vertType = IC_VERT_DIF;
    res |= pDev->CreateVB(&m_pGround, &vert_def);

    icVertLock vertLock;
    res |= m_pGround->Lock(&vertLock);

    ICVRT_DIF* vb = (ICVRT_DIF*)vertLock.pData;

    uint cur_vert=0;

    for (int row=0; row<numrow-1; ++row)
    {
        for (int col=0; col<numcol-1; ++col)
        {
            icReal x1 = (col*edgeDist)-halfx;
            icReal y1 = (row*edgeDist)-halfy;
            icReal x2 = ((col+1)*edgeDist)-halfx;
            icReal y2 = ((row+1)*edgeDist)-halfy;

            vb[cur_vert].pos.Set(x1,-10.0f,y1);
            vb[cur_vert++].uv.Set(0.0f,0.0f);

            vb[cur_vert].pos.Set(x1,-10.0f,y2);
            vb[cur_vert++].uv.Set(0.0f,1.0f);

            vb[cur_vert].pos.Set(x2,-10.0f,y2);
            vb[cur_vert++].uv.Set(1.0f,1.0f);

            vb[cur_vert].pos.Set(x2,-10.0f,y1);
            vb[cur_vert++].uv.Set(1.0f,0.0f);
        }
    }

    res |= m_pGround->Unlock();
    res |= m_pContent->Load("Resource/textures/fire.tga", &m_pGroundTex);


    icIndexDef ind_def;
    ind_def.indexSize = IC_INDEX_SIZE_16;
    ind_def.numIndex = (numrow-1)*(numcol-1)*6;
    ind_def.usage = IC_INDEX_USAGE_STATIC;
    res |= pDev->CreateIB(&m_pGroundIB,&ind_def);

    icIndexLock iblock;
    res |= m_pGroundIB->Lock(&iblock);

    short* ib = (short*)iblock.pData;
    uint cur_index = 0;


    for (int row=0; row<numrow-1; ++row)
    {
        for (int col=0; col<numcol-1; ++col)
        {
            ib[cur_index++] = (numcol-1)*row*4 + col*4 ;
            ib[cur_index++] = (numcol-1)*row*4 + col*4 + 1;
            ib[cur_index++] = (numcol-1)*row*4 + col*4 + 2;

            ib[cur_index++] = (numcol-1)*row*4 + col*4 + 2;
            ib[cur_index++] = (numcol-1)*row*4 + col*4 + 3;
            ib[cur_index++] = (numcol-1)*row*4 + col*4;
        }
    }
    res |= m_pGroundIB->Unlock();

    return res;
}// END FUNCTINO CreateGround(void)


/*! Initializes the physics
 *
 *  @returns        ICRESULT        Success/failure of physics initialization
**/
ICRESULT GameState::InitPhysics(void)
{
    b2AABB aabb;
    aabb.lowerBound.Set(-10.0f,-10.0f);
    aabb.upperBound.Set(10.0f,10.0f);

    b2Vec2 grav(0.0f,0.0f);
    m_World = new b2World(aabb,grav,false);

	return m_World ? IC_OK : IC_FAIL_GEN;
}// END FUNCTION InitPhysics(void)


/*! Updates the UVs on the ground plane
 *
 *  @param          fDeltaTime          Elapsed time since last update
 *  @returns        ICRESULT            Success/failure of updating UVs
**/
ICRESULT GameState::UpdateUVs(const float fDeltaTime)
{
    ICRESULT res = IC_OK;

    // ANIMATED UVs
    icVertLock vlock;
    res |= m_pGround->Lock(&vlock);

    ICVRT_DIF* vb = (ICVRT_DIF*)vlock.pData;

    static float curdir = 0.0f;

    icReal u1 = icCos(curdir) * 0.01f * fDeltaTime;
    icReal v2 = icSin(curdir) * 0.01f * fDeltaTime;

    icReal t = icRandF(0.0001f,0.001f);
    curdir += icRandF(0.0f,10000000.0f) > 500000.0f ? t : -t;

    uint cur_vert = 0;
    for (int row=0; row<numrow-1; ++row)
    {
        for (int col=0; col<numcol-1; ++col)
        {
            vb[cur_vert].uv.x += u1;
            vb[cur_vert++].uv.y += v2;
            vb[cur_vert].uv.x += u1;
            vb[cur_vert++].uv.y += v2;
            vb[cur_vert].uv.x += u1;
            vb[cur_vert++].uv.y += v2;
            vb[cur_vert].uv.x += u1;
            vb[cur_vert++].uv.y += v2;
        }
    }

    res |= m_pGround->Unlock();

    return res;
}// END FUNCTION UpdateUVs(const float fDeltaTime)


/*! Updates the current scores
**/
void GameState::UpdateScore()
{
    if (GameMode == GAME_MODE_CLIENT) {
        for(int player=0;player<2;++player){
            int score=std::max(0,std::min(9,m_score[player]));
            m_wgtScore[player]->SetUVs(tl[score],tr[score],bl[score],br[score]);
        }
        return;
    }
    if(m_score[0]>=WINING_SCORE||m_score[1]>=WINING_SCORE)return;
    b2Vec2 pos=m_Puck.GetPhys()->GetPosition();
    int scorer=pos.y>=0.87f?PLAYER_ONE:pos.y<=-0.74f?PLAYER_TWO:-1;
    if(scorer<0)return;
    if(m_scoreSnd&&gApp.GetSettings().bSfx)m_scoreSnd->Play();
    ++m_score[scorer];
    if(m_score[scorer]>=WINING_SCORE){
        m_wgtWin->SetTextures(scorer==PLAYER_ONE?m_texPlyr1:m_texPlyr2);
        m_fWinTimer=WIN_STALL_TIMER;
        m_Puck.GetPhys()->SetLinearVelocity(b2Vec2(0,0));
    }else{
        int score=m_score[scorer];
        m_wgtScore[scorer]->SetUVs(tl[score],tr[score],bl[score],br[score]);
        m_Puck.Reset();m_Paddles[0].Reset();m_Paddles[1].Reset();
        m_serveTimer=0.8f;
        // Rebase player targets after a goal instead of snapping to the old shot.
        for(auto& paddle:m_Paddles) if(auto handler=paddle.GetInputHandler())handler->ResetTarget();
    }
    gApp.SendPacket.score[PLAYER_ONE]=m_score[PLAYER_TWO];
    gApp.SendPacket.score[PLAYER_TWO]=m_score[PLAYER_ONE];
}


/*! Updates the current game that is in session
 *
 *  @param          fDeltaTime          Elapsed time since last update
 *  @param          bFinished           Pointer, set to true if ste is done
 *  @returns        ICRESULT            Success/failure/warnings of update
**/
ICRESULT GameState::Update(const float fDeltaTime, bool *bFinished)
{
    ICRESULT res = IC_OK;

    // update the game objects
    res |= m_Table.Update(fDeltaTime);

    // Update the camera
    m_ChaseCam.Update(fDeltaTime);
    m_Camera.Update(fDeltaTime);

    // update the physics
    if (m_score[PLAYER_ONE] < WINING_SCORE && m_score[PLAYER_TWO] < WINING_SCORE)
    {
        auto input=icInput::Instance();
        for(auto& paddle:m_Paddles) if(auto handler=paddle.GetInputHandler())handler->PrepareFrame(input);
        const float fixedStep = gameplay::step;
        float& accum = m_Accumulator;
        accum += std::min(fDeltaTime,0.1f);
        if (accum >= fixedStep)
        {
            int numsteps = accum/fixedStep;
            for (int i=0; i<numsteps; ++i)
            {
                if(m_serveTimer>0){m_serveTimer=std::max(0.0f,m_serveTimer-fixedStep);continue;}
                if(m_score[0]>=WINING_SCORE||m_score[1]>=WINING_SCORE)break;
                // Input targets are sampled once per render frame, then simulated at 120 Hz.
                res |= m_Paddles[0].Update(fixedStep);
                res |= m_Paddles[1].Update(fixedStep);
                res |= m_Puck.Update(fixedStep);

                m_World->Step(fixedStep, 8);
                UpdateScore();
                ++m_PhysicsSteps;
            }

            accum -= numsteps*fixedStep;
        }


        res |= UpdateUVs(fDeltaTime);

        if (icInput::Instance()->IsPressed(ICKEY_ESCAPE))
        {
            m_bRenderTopOnly = false;
            icInput::Instance()->ShowCurs();
            gApp.GetStateMachine().PushState(&gApp.m_PauseMenu);

            //icInput::Instance()->Update();
        }
    }
    else
    {
        m_fWinTimer -= fDeltaTime;
        if (m_fWinTimer <= 0.0f)
        {
            m_Puck.Reset();m_Paddles[0].Reset();m_Paddles[1].Reset();
            m_serveTimer=0.8f;
            m_score[PLAYER_ONE] = 0;m_score[PLAYER_TWO] = 0;
            for(int player=0;player<2;++player)m_wgtScore[player]->SetUVs(tl[0],tr[0],bl[0],br[0]);
            for(auto& paddle:m_Paddles)if(auto handler=paddle.GetInputHandler())handler->ResetTarget();
        }
    }
    UpdateScore();


    if (GameMode == GAME_MODE_CLIENT)
    {
        m_score[PLAYER_ONE] = gApp.ReceivePacket.score[PLAYER_ONE];
        m_score[PLAYER_TWO] = gApp.ReceivePacket.score[PLAYER_TWO];
    }

    m_wgtScore[PLAYER_ONE]->Update(fDeltaTime);
    m_wgtScore[PLAYER_TWO]->Update(fDeltaTime);
    m_wgtWin->Update(fDeltaTime);

    // TODO: if score, win state and such
    return res;
}// END FUNCTION Update(const float fDeltaTime, bool *bFinished)


/*! Renders the current game that is in session
 *
 *  @returns        ICRESULT        Success/failure/warnings of render
**/
ICRESULT GameState::Render(void)
{
    //icInput::Instance()->HideCurs();

    ICRESULT res = IC_OK;

    icGXDevice* pDev = m_pContent->GetDevice();

    // set projection and view matrix
    pDev->SetProjection(m_Camera.GetProjection());
    pDev->SetViewMatrix(m_Camera.GetViewMat());

    // draw the temporary ground
    pDev->SetWorldTransform(m_Transform);
    pDev->SetTexture(0, m_pGroundTex);
    res |= pDev->DrawIndexedVertBuf(m_pGround,m_pGroundIB);

    // draw the table
    res |= m_Table.Render();

    // draw the paddles
    res |= m_Paddles[0].Render();
    res |= m_Paddles[1].Render();

    // draw the puck
    res |= m_Puck.Render();

    //pDev->EnableDepthTest(false);
    //pDev->SetCullMode(IC_CULL_OFF);
    pDev->SetProjection(pDev->GetOrtho());
    pDev->SetViewMatrix(m_Transform);
    //pDev->SetWorldTransform(m_Transform);

    //icColor b = icColor::BLUE;
    //int score = 0;
    //// WTF FONTS
    //m_Font.Printf(0,0,b,"Player 1: %d", score);
    //m_Font.Printf(1000,100,b,"Player 2: %d", score);
    //pDev->EnableDepthTest(true);

    m_wgtScore[PLAYER_ONE]->Render();
    m_wgtScore[PLAYER_TWO]->Render();


    icReal sx = (icReal)gApp.GetSettings().iScreen_x;
    icReal sy = (icReal)gApp.GetSettings().iScreen_y;

    icReal rat = sy/1024.0f;
    icReal mx = sx/2.0f;

    
    m_wgtScore[PLAYER_ONE]->SetPos(icVector2(sx*0.85f,sy*0.20f));
    m_wgtScore[PLAYER_TWO]->SetPos(icVector2(sx*0.57f,sy*0.84f));

    if(m_pFont){
        m_pFont->Print(icVector2Int(24,(int)sy-40),icColor::WHITE,GameMode==GAME_MODE_SINGLE?"Mouse: your paddle   |   Opponent: CPU   |   Esc: pause":"Mouse: player 1   |   Arrow keys: player 2   |   Esc: pause");
        if(m_serveTimer>0)m_pFont->Print(icVector2Int((int)(sx*0.43f),(int)(sy*0.50f)),icColor::WHITE,"GET READY");
    }
    if (m_score[PLAYER_ONE] >= WINING_SCORE|| m_score[PLAYER_TWO] >= WINING_SCORE)
    {
        pDev->SetProjection(pDev->GetOrtho());
        icMatrix44 temp;
        temp.Identity();
        pDev->SetViewMatrix(temp);
        pDev->SetWorldTransform(temp);
        m_wgtWin->Render();
    }

    return res;
}// END FUNCTION Render(void)


/*! This gets called when the gamestate gets pushed on the stack
 *  and should reset anything that needs to be reset for each game
**/
void GameState::OnPush(void)
{
    m_Accumulator = 0;
    m_PhysicsSteps = 0;
    m_serveTimer=0.8f;
    m_fWinTimer=0;
    // reset physics
    m_Puck.Reset();
    m_Paddles[0].Reset();
    m_Paddles[1].Reset();

    // setup input handlers
    InputHandler* p0 = m_Paddles[0].GetInputHandler();
    InputHandler* p1 = m_Paddles[1].GetInputHandler();

    if (p0) delete p0;
    if (p1) delete p1;

    switch (GameMode)
    {
    case GAME_MODE_SINGLE:
        p0 = new PlayerInput(PLAYER_ONE, m_Puck.GetPhys(), m_Paddles[0].GetPhys());
        p1 = new AiInput(PLAYER_TWO, m_Puck.GetPhys(), m_Paddles[1].GetPhys());
        break;
    case GAME_MODE_TWO:
        p0 = new PlayerInput(PLAYER_ONE, m_Puck.GetPhys(), m_Paddles[0].GetPhys());
        p1 = new PlayerInput(PLAYER_TWO, m_Puck.GetPhys(), m_Paddles[1].GetPhys());
        break;
    case GAME_MODE_HOST:
    case GAME_MODE_CLIENT:
        p0 = new PlayerInput(PLAYER_ONE, m_Puck.GetPhys(), m_Paddles[0].GetPhys());
        p1 = new NetworkInput(PLAYER_TWO, m_Puck.GetPhys(), m_Paddles[1].GetPhys());
        break;
    default:
        p0 = new PlayerInput(PLAYER_ONE, m_Puck.GetPhys(), m_Paddles[0].GetPhys());
        p1 = new PlayerInput(PLAYER_TWO, m_Puck.GetPhys(), m_Paddles[1].GetPhys());
        icWarning("invalid game mode entered!");
    }

    m_Paddles[0].SetInputHandler(p0);
    m_Paddles[1].SetInputHandler(p1);

    m_score[PLAYER_ONE] = 0;
    m_score[PLAYER_TWO] = 0;

    if (m_wgtScore[PLAYER_TWO])
        m_wgtScore[PLAYER_TWO]->SetUVs(tl[0],tr[0],bl[0],br[0]);
    if (m_wgtScore[PLAYER_ONE])
        m_wgtScore[PLAYER_ONE]->SetUVs(tl[0],tr[0],bl[0],br[0]);

    icInput::Instance()->HideCurs();
}// END FUNCTION OnPush(void)


/*! This gets called when the gamestate gets popped off the stack
 *  we might not need it
**/
void GameState::OnPop(void)
{
    icInput::Instance()->ShowCurs();
}// END FUNCTION OnPop(void)