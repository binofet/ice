#ifndef __SPIN_STATE_H__
#define __SPIN_STATE_H__


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

//////////////////////////////////////////////////////////////////////////////
// LOCAL INCLUDES
#include "puck.h"
#include "paddle.h"

#include "../Box2D/Box2D.h"

class SpinState : public icState
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    SpinState(void);
    ~SpinState(void);

    ICRESULT Init(icContentLoader* pContentLoader);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    ICRESULT Update(const float fDeltaTime, bool* bFinished);
    ICRESULT Render(void);

    void OnPush(void);
    void OnPop(void);

private:
    // cameras
    icCamera            m_Camera;

    icContentLoader*    m_pContent;

    // renderable objects
    icTexture*          m_pGroundTex;       //! Fire ground (temp)
    icVertBuffer*       m_pGround;
    icIndexBuf*         m_pGroundIB;
    icReal              halfx;
    icReal              halfy;
    int                 numrow;
    int                 numcol;
    float               edgeDist;

    icMatrix44          m_Transform;
    icModel*            m_pTable;           //! Table Object
    icModel*            m_pTable2;
    icModel*            m_pTable3;
    icModel*            m_pTable4;

    // camera movement stuff
    icVector3           cam_pos;
    icVector3           look_pos;
    float               org_len;
};



#endif //__SPIN_STATE_H__