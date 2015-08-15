#ifndef __SKEE_BALL_RAMP_H__
#define __SKEE_BALL_RAMP_H__

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

//class b2Body;

/*! Provides a puck object
**/
class Ramp
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    Ramp(void);
    ~Ramp(void);

    ICRESULT Init(icContentLoader* pContent, btDiscreteDynamicsWorld* pWorld);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    ICRESULT Update(const float fDeltaTime);
    ICRESULT Render(void);

private:
    ICRESULT InitPhysics(btDiscreteDynamicsWorld* pWorld);

    icMatrix44          m_Trans;        //! puck transformation matrix

    icContentLoader*    m_pContent;

    icModel*            m_pTable;           //! Table Object

    btDiscreteDynamicsWorld*    m_pworld;

    btTriangleIndexVertexArray* m_indexVertexArrays;
    btCollisionShape*           m_colShape;

    btCollisionObject*          m_colObject;
    btCollisionObject*          m_colObject2;
    btCollisionObject*          m_colObject3;

    void*                       m_vb;

    icVector3*                  m_verts;
    int*                        m_ib;
};

#endif //__SKEE_BALL_RAMP_H__