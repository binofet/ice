#ifndef __GAME_STATE_H__
#define __GAME_STATE_H__


//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "Util/icStateMachine.h"
#include "Render/icModel.h"
#include "Content/icContentLoader.h"
#include "Render/Camera/icMaxCam.h"
#include "Render/VertBuffer/icVertBuffer.h"
#include "Render/IndexBuffer/icIndexBuf.h"
#include "Render/Texture/icTexture.h"
#include "Render/Font/icFontGL.h"
#include "UI/icWidget.h"

//////////////////////////////////////////////////////////////////////////////
// LOCAL INCLUDES
#include "btBulletDynamicsCommon.h"
#include "skeeball_ramp.h"
#include "ball.h"

#define GAME_MODE_SINGLE        0
#define GAME_MODE_TWO           1
#define GAME_MODE_HOST          2
#define GAME_MODE_CLIENT        3

#define WINING_SCORE            10
#define WIN_STALL_TIMER         3.0f

#define MAX_BALLS               5

#define MAX_BALLS_OUT           3


/*! This is the main game state
**/
class GameState : public icState
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    GameState(void);
    ~GameState(void);

    ICRESULT Init(icContentLoader* pContentLoader);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    ICRESULT Update(const float fDeltaTime, bool* bFinished);
    ICRESULT Render(void);

    void OnPush(void);
    void OnPop(void);

    int GameMode;


private:
    //////////////////////////////////////////////////////////////////////////
    // PRIVATE METHODS
    ICRESULT InitPhysics(void);

    void UpdatePhysics(const float fDeltaTime);

    void UpdateScore(const float fDeltaTime);

    //////////////////////////////////////////////////////////////////////////
    // PRIVATE MEMBERS

    // cameras
    icMaxCam            m_MaxCam;

    icMatrix44          m_Transform;
    icMatrix44          m_m44Tickets;

    icContentLoader*    m_pContent;

    // game objects
    icFontGL            m_Font;
    icFontGL            m_ScoreFont;

    icModel*            m_pTicketStart;
    icModel*            m_pTicketRoll;

    icVector3           m_v3Accel;

    float               m_fDy;
    float               m_fTicketRot;
    float               m_fPercent;
    float               m_fGameOverTimer;
    float               m_fPlayTimer;

    int                 m_i32Score;
    int                 m_i32NumTickets;

    bool                m_bLastLMB;
    bool                bScored;

    bool                bInPlay;
    bool                bWaiting;

    Ball                m_balls[MAX_BALLS];
    int                 m_curBall;
    Ramp                m_ramp;

    icSoundI*           m_scoreSnd;
    icSoundI*           m_loserSnd;
    icSoundI*           m_score2nd;

    // physics
    btDiscreteDynamicsWorld*                m_physWorld;
    btCollisionDispatcher*                  m_dispatcher;
    btAxisSweep3*                           m_pairCache;
    btSequentialImpulseConstraintSolver*    m_constraintSolver;
    btDefaultCollisionConfiguration*        m_collisionConfig;
};



#endif //__GAME_STATE_H__