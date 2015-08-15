#ifndef __PUCK_H__
#define __PUCK_H__

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
class Puck
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    Puck(void);
    ~Puck(void);

    ICRESULT Init(icContentLoader* pContent, b2World* pWorld);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    ICRESULT Update(const float fDeltaTime);
    ICRESULT Render(void);

    void Reset(void);

    b2Body* GetPhys(void) {return m_pPhys;};

private:
    ICRESULT InitPhysics(b2World* pWorld);

    icMatrix44          m_Trans;    //! puck transformation matrix

    float               m_fTimer;

    icContentLoader*    m_pContent;

    icModel*            m_pPuck;
    icSoundI*           m_pSound;   //! Sound of puck hitting the boundries
    b2Body*             m_pPhys;
};

#endif //__PUCK_H__