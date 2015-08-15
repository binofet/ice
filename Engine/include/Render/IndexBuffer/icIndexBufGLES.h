#ifndef __IC_INDEX_BUFFER_GLES_H__
#define __IC_INDEX_BUFFER_GLES_H__

#include "Render/IndexBuffer/icIndexBuf.h"

#ifdef ICGLES
#include "Core/GXDevice/icGLext.h"


class icIndexBufGLES : public icIndexBuf
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icIndexBufGLES(void);
    virtual ~icIndexBufGLES(void) {Cleanup();};

    virtual ICRESULT Cleanup(void);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT Lock(icIndexLock* pIndexLock);
    virtual ICRESULT Unlock(void);

    void* GetLocked() {return m_LockBuf;};

private:
    void*       m_LockBuf;
};

#endif //ICGL
#endif //__IC_INDEX_BUFFER_GLES_H__