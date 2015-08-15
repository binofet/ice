#ifndef __GAME_STATE_H__
#define __GAME_STATE_H__


//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "Util/icStateMachine.h"
#include "Render/icModel.h"
#include "Content/icContentLoader.h"
#include "Render/Camera/icCamera.h"
#include "Render/Camera/icChaseCam.h"
#include "Render/VertBuffer/icVertBuffer.h"
#include "Render/IndexBuffer/icIndexBuf.h"
#include "Render/Texture/icTexture.h"
#include "Render/Font/icFont.h"
#include "UI/icWidget.h"

//////////////////////////////////////////////////////////////////////////////
// LOCAL INCLUDES
#include "puck.h"
#include "paddle.h"
#include "table.h"

#include "../Box2D/Box2D.h"


#define GAME_MODE_SINGLE        0
#define GAME_MODE_TWO           1
#define GAME_MODE_HOST          2
#define GAME_MODE_CLIENT        3

#define WINING_SCORE            10
#define WIN_STALL_TIMER         3.0f


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

    int GetPlayer1Score() { return m_score[PLAYER_ONE]; };
    int GetPlayer2Score() { return m_score[PLAYER_TWO]; };

    void SetPlayer1Score(int s) {m_score[PLAYER_ONE]=s;};
    void SetPlayer2Score(int s) {m_score[PLAYER_TWO]=s;};

private:
    //////////////////////////////////////////////////////////////////////////
    // PRIVATE METHODS
    ICRESULT CreateGround(void);
    ICRESULT InitPhysics(void);
    ICRESULT UpdateUVs(const float fDeltaTime);

    void UpdatePhysics(const float fDeltaTime);

    void UpdateScore();

    //////////////////////////////////////////////////////////////////////////
    // PRIVATE MEMBERS

    // cameras
    icChaseCam          m_ChaseCam;
    icCamera            m_Camera;

    icMatrix44          m_Transform;

    icContentLoader*    m_pContent;

    // ground
    icTexture*          m_pGroundTex;
    icVertBuffer*       m_pGround;
    icIndexBuf*         m_pGroundIB;    

    // game objects
    icFontI*            m_pFont;
    Table               m_Table;
    Puck                m_Puck;             //! Puck
    Paddle              m_Paddles[2];       //! Paddles

    icWidget*           m_wgtWin;
    icTexture*          m_texPlyr1;
    icTexture*          m_texPlyr2;
    icWidget*           m_wgtScore[2];
    int                 m_score[2];
    float               m_fWinTimer;

    icSoundI*           m_scoreSnd;
    icSoundI*           m_loserSnd;

    // physics
    b2World*            m_World;
};



#endif //__GAME_STATE_H__