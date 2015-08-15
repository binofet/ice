#ifndef __IC_VERT_BUFFER_GLES_H_
#define __IC_VERT_BUFFER_GLES_H_

#include "Render/VertBuffer/icVertBuffer.h"

#ifdef ICGLES
#include "Core/GXDevice/icGLext.h"

class icVertBuffGLES : public icVertBuffer
{
public:
    //////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icVertBuffGLES(void);
    virtual ~icVertBuffGLES( void ){ Cleanup(); };

    virtual ICRESULT Cleanup( void );

    //////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT Lock( icVertLock* pVertLock );
    virtual ICRESULT Unlock( void );

    void* GetLocked() {return m_LockBuf;};

private:
    void*       m_LockBuf;
};

#endif //ICGLES
#endif //__IC_VERT_BUFFER_GLES_H_

