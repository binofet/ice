//////////////////////////////////////////////////////////////////////////////
// LOCAL INCLUDES
#include "gamestate.h"

#include "skeeball.h"

//#include "BulletDynamics/Dynamics/btDiscreteDynamicsWorld.cpp"

//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "Core/IO/icInput.h"
#include "Math/icRand.h"


icVector3 pt_ten(-3.269999f,20.250437f,-112.25557);
icVector3 pt_twenty(-2.799999f,33.980305f,-124.99829f);
icVector3 pt_thirty(-2.3799999f,45.618351f,-137.65363f);
icVector3 pt_fourty(-2.9799999f,57.686325f,-151.56598f);
icVector3 pt_fifty(-3.139999f,68.416206f,-165.14851f);
icVector3 pt_hundred_l(26.960522f,80.188721f,-178.32127f);
icVector3 pt_hundred_r(-32.840496f,80.188721f,-178.32127f);

icVector3 spawn_pos(47.823,-19.383,114.481);

#define SQR_SCORE_DST       (75.0f)

extern SkeeBall& gApp;

/*! c'tor
**/
GameState::GameState(void)
{
    m_pContent = NULL;

    m_physWorld = NULL;
    m_dispatcher = NULL;
    m_pairCache = NULL;
    m_constraintSolver = NULL;
    m_collisionConfig = NULL;
    m_pTicketStart = NULL;
    m_pTicketRoll = NULL;

    bInPlay = false;
    bWaiting = false;
    m_bLastLMB = false;
    bScored = false;

    m_fPercent = 0.0f;
    m_fDy = 0.0f;
    m_fTicketRot = 0.0f;
}// END FUNCTION GameState(void)


/*! d'tor
**/
GameState::~GameState()
{
    if (m_physWorld)
        delete m_physWorld;
    if (m_dispatcher)
        delete m_dispatcher;
    if (m_pairCache)
        delete m_pairCache;
    if (m_constraintSolver)
        delete m_constraintSolver;
    if (m_collisionConfig)
        delete m_collisionConfig;

}// END FUNCTION ~GameState()


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
    icReal ar = (icReal)gApp.GetSettings().iScreen_x/(icReal)gApp.GetSettings().iScreen_y;
    icCreatePerspective((40.0f*IC_PI/180.0f),ar,1.0f,10000000.0f, m_MaxCam.GetProjection());
    
    m_MaxCam.Zoom() = 328.0f;
    m_MaxCam.ZoomMax() = 400.0f;
    m_MaxCam.ZoomMin() = 2.0f;
    m_MaxCam.Xrot() = 1.5584f;
    m_MaxCam.Yrot() = 4.683891f;
    m_MaxCam.Center().Set(-7.341785f, 33.721600f, -8.874823f);

    m_MaxCam.bEnableInput = gApp.GetSettings().bFreeCam;

    // physics
    res |= InitPhysics();


    int sizex = gApp.GetSettings().iScreen_x;
    int sizey = gApp.GetSettings().iScreen_y;

    if (gApp.GetSettings().bFullscreen)
    {
        if (sizex == 1920 && sizey == 1080)
        {
            res |= m_ScoreFont.Initialize("Arial",48,m_pContent->GetDevice());
            res |= m_Font.Initialize("Arial",36,m_pContent->GetDevice());
        }
        else if (sizex == 1280 && sizey == 720)
        {
            res |= m_ScoreFont.Initialize("Arial",40,m_pContent->GetDevice());
            res |= m_Font.Initialize("Arial",26,m_pContent->GetDevice());
        }
        else if (sizex == 1280 && sizey == 800)
        {
            res |= m_ScoreFont.Initialize("Arial",40,m_pContent->GetDevice());
            res |= m_Font.Initialize("Arial",26,m_pContent->GetDevice());
        }
        else if (sizex == 1280 && sizey == 1024)
        {
            res |= m_ScoreFont.Initialize("Arial",48,m_pContent->GetDevice());
            res |= m_Font.Initialize("Arial",36,m_pContent->GetDevice());
        }
        else if (sizex == 1024 && sizey == 768)
        {
            res |= m_ScoreFont.Initialize("Arial",48,m_pContent->GetDevice());
            res |= m_Font.Initialize("Arial",36,m_pContent->GetDevice());
        }
    }
    else
    {
        if (sizex == 1920 && sizey == 1080)
        {
            res |= m_ScoreFont.Initialize("Arial",48,m_pContent->GetDevice());
            res |= m_Font.Initialize("Arial",36,m_pContent->GetDevice());
        }
        else if (sizex == 1280 && sizey == 720)
        {
            res |= m_ScoreFont.Initialize("Arial",40,m_pContent->GetDevice());
            res |= m_Font.Initialize("Arial",26,m_pContent->GetDevice());
        }
        else if (sizex == 1280 && sizey == 800)
        {
            res |= m_ScoreFont.Initialize("Arial",40,m_pContent->GetDevice());
            res |= m_Font.Initialize("Arial",26,m_pContent->GetDevice());
        }
        else if (sizex == 1280 && sizey == 1024)
        {
            res |= m_ScoreFont.Initialize("Arial",48,m_pContent->GetDevice());
            res |= m_Font.Initialize("Arial",36,m_pContent->GetDevice());
        }
        else if (sizex == 1024 && sizey == 768)
        {
            res |= m_ScoreFont.Initialize("Arial",40,m_pContent->GetDevice());
            res |= m_Font.Initialize("Arial",26,m_pContent->GetDevice());
        }
    }

    //ICRESULT r = m_ScoreFont.Initialize("7 Segment", 36, m_pContent->GetDevice(), false);

    //if (ICEFAIL(r))
    // //   m_ScoreFont.Initialize("Arial",36,m_pContent->GetDevice(),false);

    if (ICEFAIL(m_pContent->Load("Resource/models/ticket_start.icm", &m_pTicketStart)))
        return IC_FAIL_GEN;

    if (ICEFAIL(m_pContent->Load("Resource/models/ticket.icm", &m_pTicketRoll)))
        return IC_FAIL_GEN;




    icReal sx = (icReal)gApp.GetSettings().iScreen_x;
    icReal sy = (icReal)gApp.GetSettings().iScreen_y;

    icReal rat = sy/1024.0f;
    icReal mx = sx/2.0f;

    m_ramp.Init(m_pContent,m_physWorld);


    icVector3 pos = spawn_pos;
    int maxballs = icMin(MAX_BALLS, MAX_BALLS_OUT);
    for (int i=0; i<maxballs; ++i)
    {
        m_balls[i].Init(m_pContent,m_physWorld,pos);
        pos.z += 7.95f;
        pos.y -= 1.50f;
    }

    pos.Set(0.0f,-50.0f,0.0f);
    for (int i=maxballs; i<MAX_BALLS; ++i)
    {
        m_balls[i].Init(m_pContent,m_physWorld,pos);
    }

    icSoundParam params;

    params.bLoop = false;
    params.fVol = gApp.GetSettings().fSfx_vol;
    params.fPitch = 1.0f;
   

    m_pContent->Load("Resource/audio/out_of_balls.wav",&m_loserSnd);
    m_loserSnd->SetParams(params);

    m_pContent->Load("Resource/audio/normpt_score.wav",&m_scoreSnd);
    m_scoreSnd->SetParams(params);

    m_pContent->Load("Resource/audio/goodpt_score.wav",&m_score2nd);
    m_score2nd->SetParams(params);

    //m_pContent->Load("Resource/models/ball.icm", &m_pTestBall);
    //m_mat.Identity();

    return res;
}// END FUNCTION Init(icContentLoader* pContentLoader)


/*! Initializes the physics
 *
 *  @returns        ICRESULT        Success/failure of physics initialization
**/
ICRESULT GameState::InitPhysics(void)
{
	m_collisionConfig = new btDefaultCollisionConfiguration();
	m_dispatcher = new btCollisionDispatcher(m_collisionConfig);

	btVector3 worldMin(-1000,-1000,-1000);
	btVector3 worldMax(1000,1000,1000);

	m_pairCache = new btAxisSweep3(worldMin,worldMax);
	m_constraintSolver = new btSequentialImpulseConstraintSolver();
	m_physWorld = new btDiscreteDynamicsWorld(m_dispatcher,m_pairCache,m_constraintSolver,m_collisionConfig);

    btVector3 grav(0.0f,-30.0f,0.0f);
    m_physWorld->setGravity(grav);

    return IC_OK;
}// END FUNCTION InitPhysics(void)


/*! Updates the current scores
**/
void GameState::UpdateScore(const float fDeltaTime)
{
    if (!m_physWorld || bScored)
        return;

    for (int i=0; i<MAX_BALLS; ++i)
    {
        btVector3 bv = m_balls[i].m_body->getWorldTransform().getOrigin();

        icVector3 pos(bv.x(),bv.y(),bv.z());

        if (pos.DistanceSqr(pt_ten) < SQR_SCORE_DST)
        {
            bScored = true;
            m_i32Score += 10;
            m_scoreSnd->Play();
        }
        else if (pos.DistanceSqr(pt_twenty) < SQR_SCORE_DST)
        {
            bScored = true;
            m_i32Score += 20;
            m_scoreSnd->Play();
        }
        else if (pos.DistanceSqr(pt_thirty) < SQR_SCORE_DST)
        {
            bScored = true;
            m_i32Score += 30;
            m_score2nd->Play();
        }
        else if (pos.DistanceSqr(pt_fourty) < SQR_SCORE_DST)
        {
            bScored = true;
            m_i32Score += 40;
            m_score2nd->Play();
        }
        else if (pos.DistanceSqr(pt_fifty) < SQR_SCORE_DST)
        {
            bScored = true;
            m_i32Score += 50;
            m_score2nd->Play();
        }
        else if (pos.DistanceSqr(pt_hundred_l) < SQR_SCORE_DST)
        {
            bScored = true;
            m_i32Score += 100;
            m_score2nd->Play();
        }
        else if (pos.DistanceSqr(pt_hundred_r) < SQR_SCORE_DST)
        {
            bScored = true;
            m_i32Score += 100;
            m_score2nd->Play();
        }
    }

    icReal rot = m_i32NumTickets * 0.09f * 4.0f/3.0f;
    if (m_fTicketRot != rot)
    {
        m_fTicketRot = icLerp(m_fTicketRot, rot, fDeltaTime * 4.0f);
    }

    if (m_i32Score > gApp.GetSettings().iHighScore)
    {
        SkeeBallSettings settings = gApp.GetSettings();
        settings.iHighScore = m_i32Score;
        gApp.UpdateSettings(settings);
    }

    m_i32NumTickets = m_i32Score % 10 + m_i32Score/10;
}// END FUNCTION UpdateScore()


/*! Updates the current game that is in session
 *
 *  @param          fDeltaTime          Elapsed time since last update
 *  @param          bFinished           Pointer, set to true if ste is done
 *  @returns        ICRESULT            Success/failure/warnings of update
**/
ICRESULT GameState::Update(const float fDeltaTime, bool *bFinished)
{
    ICRESULT res = IC_OK;

    icInput* input = icInput::Instance();

    bool ldown = input->IsDown(ICMOUSE_L);

#ifdef WIIMOTE
    if (gApp.GetSettings().bWiiMote)
    {
        wiimote& wii_remote = gApp.remote;
        if (wii_remote.IsConnected())
        {
            ldown |= wii_remote.Button.B();
        }
    }
#endif

    // Update the camera
    m_MaxCam.Update(fDeltaTime);

    // Update the physics world
    m_physWorld->stepSimulation(fDeltaTime * 4.0f,3,1.0f/60.0f);

    Ball* ball = NULL;
    
    if (!bInPlay && !bWaiting && m_curBall < MAX_BALLS  && m_curBall > -1)
    {
        icVector3 spawn = spawn_pos;
        int wrap_index = ( m_curBall + 2 > MAX_BALLS) ? MAX_BALLS - m_curBall : m_curBall;

        if (m_curBall > -1 && MAX_BALLS - m_curBall > MAX_BALLS_OUT)
            m_balls[wrap_index].SetPos(spawn);

        if (m_curBall < 0)
            m_curBall = 0;

        int index = MAX_BALLS - ++m_curBall;
        ball = &m_balls[index];
        bWaiting = true;

        bScored = false;

        icVector3 pos(0.0f,-23.20f,140.0f);

        ball->SetPos(pos);

        btTransform predictedTrans;
        btRigidBody* body = ball->m_body;
		body->predictIntegratedTransform(fDeltaTime, predictedTrans);
		btScalar squareMotion = (predictedTrans.getOrigin()-body->getWorldTransform().getOrigin()).length2();
		
		body->proceedToTransform( predictedTrans);
    }
    else if (bWaiting && m_curBall > -1)
    {
        ball = &m_balls[MAX_BALLS - m_curBall];

        // drop a ball for physics testing
        icMatrix44 mat;
        ball->GetMat(&mat);
        icVector3 pos = mat.GetPos();

        icReal x = 0.0f;
     
#ifdef WIIMOTE
        if (gApp.GetSettings().bWiiMote)
        {
            wiimote& wiimote = gApp.remote;
            if (wiimote.NunchukConnected())
            {
                x = wiimote.Nunchuk.Joystick.X * 100.0f * fDeltaTime;
            }
            if (wiimote.Button.Left())
                x -= 100.0f * fDeltaTime;
            if (wiimote.Button.Right())
                x += 100.0f * fDeltaTime;
        }
        else
            x = icInput::Instance()->GetAxis(ICAXIS_MOUSE_X) * 12000.0f * fDeltaTime;
        
        float new_x = icClamp(pos.x + x, -31.047312f,37.262222f);
        pos.Set(new_x,-23.20f,140.0f);
        
        if (ball && !bInPlay && bWaiting)
        {
            ball->SetPos(pos);
            ball->m_body->activate(true);
        }
#endif
    }

    if (input->IsPressed(ICKEY_ESCAPE))
    {
        m_bRenderTopOnly = false;
        icInput::Instance()->ShowCurs();
        gApp.GetStateMachine().PushState(&gApp.m_PauseMenu);
    }
    else if (ball && !bInPlay && !ldown && m_bLastLMB)
    {
        icVector3 look(0.0f,-0.2f,1.49904881f);
        look.Normalize();

        icReal scale = icClamp(m_fDy,0.0f, 1000.0f);
        if (gApp.GetSettings().bWiiMote)
        {
            scale = m_v3Accel.Length();
            m_v3Accel.Set(0.0f,0.0f,0.0f);
        }

        look *= -scale;
        btVector3 force(look.x,look.y,look.z);

        ball->m_body->activate(true);
        ball->m_body->applyImpulse(force,btVector3(0.0,0.0,0.0));

        m_fPlayTimer = 0.0f;
        bInPlay = true;
        bWaiting = false;

        m_fPercent = 0.0f;
        m_fDy = 0.0f;
    }

    // check for ball status
    if (bInPlay)
    {
        m_fPlayTimer+=fDeltaTime;
        int index = MAX_BALLS - m_curBall;

        if (0 <= index && index < MAX_BALLS)
        {
            Ball* ball = &m_balls[index];

            icMatrix44 mat;
            ball->GetMat(&mat);

            if (mat.GetPos().y < -40.0f || m_fPlayTimer > 5.0f)
            {
                bInPlay = false;
                bWaiting = false;
            }
        }
    }

    if (ldown && m_bLastLMB)
    {
        m_fDy += abs(input->GetAxis(ICAXIS_MOUSE_Y) * 25000.0f * fDeltaTime);

#ifdef WIIMOTE
        if (gApp.GetSettings().bWiiMote)
        {
            wiimote& mote = gApp.remote;
            
            m_fDy += abs(mote.Acceleration.Orientation.Pitch) * 5.0f * fDeltaTime;

            m_v3Accel.x += mote.Acceleration.X * 1000.0f * fDeltaTime;
            m_v3Accel.y += mote.Acceleration.Y * 1000.0f * fDeltaTime;
            m_v3Accel.z += mote.Acceleration.Z * 1000.0f * fDeltaTime;

            m_fDy = m_v3Accel.Length();
        }
#endif

        // make sure you keep fluid movement
        m_fDy -= 50.0f * fDeltaTime;
        float val = icClamp(m_fDy,0.0f, 1000.0f);
        m_fPercent = val/1000.0f * 100.0f;
    }

    m_bLastLMB = ldown;


    for (int i=0; i<MAX_BALLS; ++i)
        m_balls[i].Update(fDeltaTime);


    if (m_curBall == MAX_BALLS && bInPlay == false && bWaiting == false)
    {
        m_loserSnd->Play();

        m_curBall = -1;

        m_fGameOverTimer  = 0.0f;
    }
    else if (m_curBall == -1)
    {
        m_fGameOverTimer += fDeltaTime;

        if (m_fGameOverTimer >= 6.0f)
        {
            icState* state = gApp.GetStateMachine().PopState();
            gApp.GetStateMachine().PushState(state);
        }
    }

    //if (input->IsDown(ICKEY_W))
    //{
    //    m_mat.Translate(0.0f,0.01f,0.0f);
    //}
    //else if (input->IsDown(ICKEY_S))
    //{
    //    m_mat.Translate(0.0f,-0.01f,0.0f);
    //}
    //else if (input->IsDown(ICKEY_D))
    //{
    //    m_mat.Translate(0.01f,0.0f,0.0f);
    //}
    //else if (input->IsDown(ICKEY_A))
    //{
    //    m_mat.Translate(-0.01f,0.0f,0.0f);
    //}
    //else if (input->IsDown(ICKEY_UP))
    //{
    //    m_mat.Translate(0.0f,0.0f,0.01f);
    //}
    //else if (input->IsDown(ICKEY_DOWN))
    //{
    //    m_mat.Translate(0.0f,0.0f,-0.01f);
    //}

    UpdateScore(fDeltaTime);

    icSoundParam param;

    param.bLoop = false;
    param.fPitch = 1.0f;
    param.fVol = gApp.GetSettings().fSfx_vol;
    
    m_scoreSnd->SetParams(param);
    m_score2nd->SetParams(param);
    m_loserSnd->SetParams(param);

#ifdef WIIMOTE
    if (gApp.GetSettings().bWiiMote)
    {
        wiimote& wii_remote = gApp.remote;
        if (wii_remote.IsConnected())
        {
            if (wii_remote.Button.Home())
            {
                m_bRenderTopOnly = false;
                icInput::Instance()->ShowCurs();
                gApp.GetStateMachine().PushState(&gApp.m_PauseMenu);
            }
        }
    }
#endif

    return res;
}// END FUNCTION Update(const float fDeltaTime, bool *bFinished)


/*! Renders the current game that is in session
 *
 *  @returns        ICRESULT        Success/failure/warnings of render
**/
ICRESULT GameState::Render(void)
{
    ICRESULT res = IC_OK;

    icGXDevice* pDev = m_pContent->GetDevice();

    // set projection and view matrix
    pDev->SetProjection(m_MaxCam.GetProjection());
    pDev->SetViewMatrix(m_MaxCam.GetViewMat());

    res |= m_ramp.Render();

    for (int i=0; i<MAX_BALLS; ++i)
        m_balls[i].Render();

    m_m44Tickets.Identity();
    m_m44Tickets.Translate(39.7f,-61.848f,120.62f);//120.62f
    m_m44Tickets.RotX(-1.79f + m_fTicketRot);
    m_m44Tickets.RotY(IC_PI);

    m_m44Tickets.Scale(-1.0f,1.0f,1.0f);

    icReal rot = m_i32NumTickets * 0.09f * 4.0f/3.0f;

    if (m_i32NumTickets < 10 || (m_i32NumTickets < 10 && m_fTicketRot != rot))
    {
        m_pTicketStart->Render(m_m44Tickets);
    }
    else
    {
        m_pTicketRoll->Render(m_m44Tickets);
    }

    //m_pTestBall->Render(m_mat);

    pDev->SetProjection(pDev->GetOrtho());
    pDev->SetViewMatrix(m_Transform);
    pDev->SetWorldTransform(m_Transform);

    int x_pos = 0.0f;
    int y_pos = 0.0f;

    int sizex = gApp.GetSettings().iScreen_x;
    int sizey = gApp.GetSettings().iScreen_y;

    if (gApp.GetSettings().bFullscreen)
    {
        if (sizex == 1920 && sizey == 1080)
        {
            x_pos = gApp.GetSettings().iScreen_x * 0.4735f;
            y_pos = gApp.GetSettings().iScreen_y * 0.847f;
        }
        if (sizex == 1280 && sizey == 720)
        {
            x_pos = gApp.GetSettings().iScreen_x * 0.5f;
            y_pos = gApp.GetSettings().iScreen_y * 0.8f;
        }
        if (sizex == 1280 && sizey == 800)
        {
            x_pos = gApp.GetSettings().iScreen_x * 0.4700f;
            y_pos = gApp.GetSettings().iScreen_y * 0.8435f;
        }
        if (sizex == 1280 && sizey == 1024)
        {
            x_pos = gApp.GetSettings().iScreen_x * 0.5f;
            y_pos = gApp.GetSettings().iScreen_y * 0.8f;
        }
        if (sizex == 1024 && sizey == 768)
        {
            x_pos = gApp.GetSettings().iScreen_x * 0.5f;
            y_pos = gApp.GetSettings().iScreen_y * 0.8f;
        }
    }
    else
    {
        if (sizex == 1920 && sizey == 1080)
        {
            x_pos = gApp.GetSettings().iScreen_x * 0.4735f;
            y_pos = gApp.GetSettings().iScreen_y * 0.847f;
        }
        if (sizex == 1280 && sizey == 720)
        {
            x_pos = gApp.GetSettings().iScreen_x * 0.4700f;
            y_pos = gApp.GetSettings().iScreen_y * 0.8435f;
        }
        if (sizex == 1280 && sizey == 800)
        {
            x_pos = gApp.GetSettings().iScreen_x * 0.4690f;
            y_pos = gApp.GetSettings().iScreen_y * 0.8440f;
        }
        if (sizex == 1280 && sizey == 1024)
        {
            x_pos = gApp.GetSettings().iScreen_x * 0.4600f;
            y_pos = gApp.GetSettings().iScreen_y * 0.8449f;
        }
        if (sizex == 1024 && sizey == 768)
        {
            x_pos = gApp.GetSettings().iScreen_x * 0.4610f;
            y_pos = gApp.GetSettings().iScreen_y * 0.8435f;
        }
    }
    m_ScoreFont.Printf(icVector2Int(x_pos,y_pos),icColor::BRED, "%03d",m_i32Score);



    // HACK!@
    // set the 2d position for the # ball text
    if (gApp.GetSettings().bFullscreen)
    {
        if (sizex == 1920 && sizey == 1080)
        {
            x_pos = gApp.GetSettings().iScreen_x * 0.582f;
            y_pos = gApp.GetSettings().iScreen_y * 0.394f;
        }
        else if (sizex == 1280 && sizey == 720)
        {
            x_pos = gApp.GetSettings().iScreen_x * 0.582f;
            y_pos = gApp.GetSettings().iScreen_y * 0.392f;
        }
        else if (sizex == 1280 && sizey == 800)
        {
            x_pos = gApp.GetSettings().iScreen_x * 0.582f;
            y_pos = gApp.GetSettings().iScreen_y * 0.392f;
        }
        else if (sizex == 1280 && sizey == 1024)
        {
            x_pos = gApp.GetSettings().iScreen_x * 0.582f;
            y_pos = gApp.GetSettings().iScreen_y * 0.392f;
        }
        else if (sizex == 1024 && sizey == 768)
        {
            x_pos = gApp.GetSettings().iScreen_x * 0.582f;
            y_pos = gApp.GetSettings().iScreen_y * 0.392f;
        }
    }
    else
    {
        if (sizex == 1920 && sizey == 1080)
        {
            x_pos = gApp.GetSettings().iScreen_x * 0.582f;
            y_pos = gApp.GetSettings().iScreen_y * 0.394f;
        }
        else if (sizex == 1280 && sizey == 720)
        {
            x_pos = gApp.GetSettings().iScreen_x * 0.5818f;
            y_pos = gApp.GetSettings().iScreen_y * 0.3957f;
        }
        else if (sizex == 1280 && sizey == 800)
        {
            x_pos = gApp.GetSettings().iScreen_x * 0.5896f;
            y_pos = gApp.GetSettings().iScreen_y * 0.3946f;
        }
        else if (sizex == 1280 && sizey == 1024)
        {
            x_pos = gApp.GetSettings().iScreen_x * 0.6139f;
            y_pos = gApp.GetSettings().iScreen_y * 0.3949f;
        }
        else if (sizex == 1024 && sizey == 768)
        {
            x_pos = gApp.GetSettings().iScreen_x * 0.6092f;
            y_pos = gApp.GetSettings().iScreen_y * 0.3935f;
        }
    }
    int balls = (bInPlay || bWaiting) ? MAX_BALLS-m_curBall + 1 : MAX_BALLS-m_curBall;
    m_Font.Printf(icVector2Int(x_pos,y_pos),icColor::BRED, "%02d",balls);

    if (m_bLastLMB)
    {
        int x_pos = (int)((icReal)gApp.GetSettings().iScreen_x * 0.75f);
        int y_pos = (int)((icReal)gApp.GetSettings().iScreen_y * 0.85f);
        m_ScoreFont.Printf(icVector2Int(x_pos,y_pos),icColor::BRED,"POWER - %.0f %%",m_fPercent);
    }
   

    return res;
}// END FUNCTION Render(void)


/*! This gets called when the gamestate gets pushed on the stack
 *  and should reset anything that needs to be reset for each game
**/
void GameState::OnPush(void)
{
    m_curBall = 0;
    bWaiting = false;
    bInPlay = false;
    bScored = false;

    m_fGameOverTimer = 0.0f;

    m_i32Score = 0;

    if (m_physWorld)
    {
        icVector3 pos = spawn_pos;
        int maxball = icMin(MAX_BALLS, MAX_BALLS_OUT);
        for (int i=0; i<maxball ; ++i)
        {
            m_balls[i].SetPos(pos);
            pos.z += 7.95f;
            pos.y -= 1.50f;

            m_balls[i].Enable(true);
        }
        pos.Set(0.0f,-50.0f,0.0f);
        for (int i=maxball; i<MAX_BALLS; ++i)
        {
            m_balls[i].SetPos(pos);
            m_balls[i].Enable(true);
        }
    }

    m_fTicketRot = 0.0f;

    m_m44Tickets.Identity();

    //if (gApp.GetSettings().bSfx)
    //    m_score2nd->Play();

    icInput::Instance()->HideCurs();
}// END FUNCTION OnPush(void)


/*! This gets called when the gamestate gets popped off the stack
 *  we might not need it
**/
void GameState::OnPop(void)
{
    if (m_physWorld)
    {
        icVector3 pos(0.0f,-130.183,0.0f);
        for (int i=0; i<MAX_BALLS; ++i)
        {
            m_balls[i].SetPos(pos);

            m_balls[i].Enable(false);
        }     
    }
}// END FUNCTION OnPop(void)