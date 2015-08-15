#ifndef __LAST_GAME_H__
#define __LAST_GAME_H__


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


/*! This is the main game state
**/
class LastGame : public icState
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    LastGame(void);
    ~LastGame(void);

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

    void UpdateScore();

    //////////////////////////////////////////////////////////////////////////
    // PRIVATE MEMBERS

    // cameras
    icMaxCam            m_MaxCam;

    icMatrix44          m_Transform;

    icContentLoader*    m_pContent;

    // game objects
    icFontGL            m_ScoreFont;

    Ramp                m_ramp;
};



#endif //__LAST_GAME_H__