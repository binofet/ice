#ifndef __PADDLE_H__
#define __PADDLE_H__

//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "Render/icModel.h"
#include "Math/Vector/icVector2.h"


//////////////////////////////////////////////////////////////////////////////
// LOCAL INCLUDES
#include "InputHandler.h"


//////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS
class icContentLoader;
class b2World;
class b2Body;


/*! Provides a Paddle object
**/
class Paddle
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    Paddle(void);
    ~Paddle(void);

    ICRESULT Init(icContentLoader* pContent, b2World* pWorld);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    ICRESULT Update(const float fDeltaTime);
    ICRESULT Render(void);

    void Reset(void);

    void SetInputHandler(InputHandler* pHandler) {m_pHandler = pHandler;};

    InputHandler* GetInputHandler(void) {return m_pHandler;};

    b2Body* GetPhys(void) {return m_pPhys[0];};

    int player;
private:
    ICRESULT InitPhysics(b2World* pWorld);

    icMatrix44          m_Trans;        //! Model transform matrix

    icContentLoader*    m_pContent;

    icModel*            m_pModel[3];           //! puck model
    b2Body*             m_pPhys[3];     //! Phyics 0-disc,1-triangle,2-square

    InputHandler*       m_pHandler;
};

#endif //__PADDLE_H__