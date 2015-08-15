//////////////////////////////////////////////////////////////////////////////
// LOCAL INCLUDES
#include "last_game.h"
#include "skeeball.h"

//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "Core/IO/icInput.h"
#include "Math/icRand.h"



extern SkeeBall& gApp;

/*! c'tor
**/
LastGame::LastGame(void)
{
    m_pContent = NULL;

    m_bRenderTopOnly = false;
    m_bUpdateTopOnly = false;
}// END FUNCTION LastGame(void)


/*! d'tor
**/
LastGame::~LastGame()
{

}// END FUNCTION ~LastGame()


/*! Initializes the games state
 *
 *  @param          pContentLoader      Pointer to main content loader
 *  @returns        ICRESULT            Success/failure/warnings of init
**/
ICRESULT LastGame::Init(icContentLoader* pContentLoader)
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

    int sizex = gApp.GetSettings().iScreen_x;
    int sizey = gApp.GetSettings().iScreen_y;

    if (gApp.GetSettings().bFullscreen)
    {
        if (sizex == 1920 && sizey == 1080)
        {
            res |= m_ScoreFont.Initialize("Arial",48,m_pContent->GetDevice());
        }
        else if (sizex == 1280 && sizey == 720)
        {
            res |= m_ScoreFont.Initialize("Arial",40,m_pContent->GetDevice());
        }
        else if (sizex == 1280 && sizey == 800)
        {
            res |= m_ScoreFont.Initialize("Arial",40,m_pContent->GetDevice());
        }
        else if (sizex == 1280 && sizey == 1024)
        {
            res |= m_ScoreFont.Initialize("Arial",48,m_pContent->GetDevice());
        }
        else if (sizex == 1024 && sizey == 768)
        {
            res |= m_ScoreFont.Initialize("Arial",48,m_pContent->GetDevice());
        }
    }
    else
    {
        if (sizex == 1920 && sizey == 1080)
        {
            res |= m_ScoreFont.Initialize("Arial",48,m_pContent->GetDevice());
        }
        else if (sizex == 1280 && sizey == 720)
        {
            res |= m_ScoreFont.Initialize("Arial",40,m_pContent->GetDevice());
        }
        else if (sizex == 1280 && sizey == 800)
        {
            res |= m_ScoreFont.Initialize("Arial",40,m_pContent->GetDevice());
        }
        else if (sizex == 1280 && sizey == 1024)
        {
            res |= m_ScoreFont.Initialize("Arial",48,m_pContent->GetDevice());
        }
        else if (sizex == 1024 && sizey == 768)
        {
            res |= m_ScoreFont.Initialize("Arial",40,m_pContent->GetDevice());
        }
    }


    icReal sx = (icReal)gApp.GetSettings().iScreen_x;
    icReal sy = (icReal)gApp.GetSettings().iScreen_y;

    icReal rat = sy/1024.0f;
    icReal mx = sx/2.0f;

    m_ramp.Init(m_pContent,NULL);

    return res;
}// END FUNCTION Init(icContentLoader* pContentLoader)


/*! Updates the current scores
**/
void LastGame::UpdateScore()
{

}// END FUNCTION UpdateScore()


/*! Updates the current game that is in session
 *
 *  @param          fDeltaTime          Elapsed time since last update
 *  @param          bFinished           Pointer, set to true if ste is done
 *  @returns        ICRESULT            Success/failure/warnings of update
**/
ICRESULT LastGame::Update(const float fDeltaTime, bool *bFinished)
{
    ICRESULT res = IC_OK;

    icInput* input = icInput::Instance();

    // Update the camera
    m_MaxCam.Update(fDeltaTime);

    return res;
}// END FUNCTION Update(const float fDeltaTime, bool *bFinished)


/*! Renders the current game that is in session
 *
 *  @returns        ICRESULT        Success/failure/warnings of render
**/
ICRESULT LastGame::Render(void)
{
    ICRESULT res = IC_OK;

    icGXDevice* pDev = m_pContent->GetDevice();

    // set projection and view matrix
    pDev->SetProjection(m_MaxCam.GetProjection());
    pDev->SetViewMatrix(m_MaxCam.GetViewMat());

    res |= m_ramp.Render();

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
    int high = gApp.GetSettings().iHighScore;
    m_ScoreFont.Printf(icVector2Int(x_pos,y_pos),icColor::BRED, "%03d",high);

    return res;
}// END FUNCTION Render(void)


/*! This gets called when the gamestate gets pushed on the stack
 *  and should reset anything that needs to be reset for each game
**/
void LastGame::OnPush(void)
{
}// END FUNCTION OnPush(void)


/*! This gets called when the gamestate gets popped off the stack
 *  we might not need it
**/
void LastGame::OnPop(void)
{
    //icInput::Instance()->ShowCurs();
}// END FUNCTION OnPop(void)