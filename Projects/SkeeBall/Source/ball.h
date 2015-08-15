#ifndef __BALL_H__
#define __BALL_H__

//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "Audio/icSound.h"
#include "Render/icModel.h"

#include "Math/Vector/icVector2.h"

class icContentLoader;
class btDiscreteDynamicsWorld;
class btTriangleIndexVertexArray;
class btCollisionShape;
class btCollisionObject;
class btMotionState;
class btRigidBody;


class Ball
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    Ball(void);
    ~Ball(void);

    ICRESULT Init(icContentLoader* pContent, btDiscreteDynamicsWorld* pWorld,
                  const icVector3& pos);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    ICRESULT Update(const float fDeltaTime);
    ICRESULT Render(void);

    void Enable(bool bEnable);

    void SetPos(const icVector3& pos);

    void GetMat(icMatrix44* mat);

    btRigidBody*                m_body;

private:
    ICRESULT InitPhysics(btDiscreteDynamicsWorld* pWorld, const icVector3& pos);

    icContentLoader*    m_pContent;

    static icModel*             m_pBallModel;

    btDiscreteDynamicsWorld*    m_world;

    static btCollisionShape*    m_sphereShape;


    btMotionState*              m_motionState;

    bool                        m_bInWorld;
};

#endif //__BALL_H__