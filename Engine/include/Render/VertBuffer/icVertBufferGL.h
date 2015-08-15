#ifndef __IC_VERT_BUFFER_GL_H_
#define __IC_VERT_BUFFER_GL_H_

#include "Render/VertBuffer/icVertBuffer.h"

#ifdef ICGL
#include "Core/GXDevice/icGLext.h"

class icVertBuffGL : public icVertBuffer
{
public:
    //////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icVertBuffGL(void);
    virtual ~icVertBuffGL( void ){ Cleanup(); };

    virtual ICRESULT Cleanup( void );

    //////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT Lock( icVertLock* pVertLock );
    virtual ICRESULT Unlock( void );

    void* GetLocked() {return m_LockBuf;};
    GLuint& GetName() {return m_Name;};

    static bool m_bHardware;

private:
    GLuint      m_Name;
    void*       m_LockBuf;
};

#endif //ICGL
#endif //__IC_VERT_BUFFER_GL_H_

