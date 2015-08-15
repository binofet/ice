#ifndef __TABLE_H__
#define __TABLE_H__

//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "Audio/icSound.h"
#include "Render/icModel.h"

#include "Math/Vector/icVector2.h"

class icContentLoader;
class b2World;
class b2Body;

/*! Provides a puck object
**/
class Table
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    Table(void);
    ~Table(void);

    ICRESULT Init(icContentLoader* pContent, b2World* pWorld);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    ICRESULT Update(const float fDeltaTime);
    ICRESULT Render(void);

private:
    ICRESULT InitPhysics(b2World* pWorld);

    icMatrix44          m_Trans;        //! puck transformation matrix

    icContentLoader*    m_pContent;

    icModel*            m_pTable;           //! Table Object
    icModel*            m_pTable2;
    icModel*            m_pTable3;
    icModel*            m_pTable4;

    b2Body*             m_pWall1;
    b2Body*             m_pWall2;
    b2Body*             m_pWall3;
    b2Body*             m_pWall4;
    b2Body*             m_pWall5;
    b2Body*             m_pWall6;
    b2Body*             m_pWall7;
    b2Body*             m_pWall8;
};

#endif //__TABLE_H__