#ifndef __IC_INDEX_BUFFER_GL_H__
#define __IC_INDEX_BUFFER_GL_H__

#include "Render/IndexBuffer/icIndexBuf.h"

#ifdef ICGL
#include "Core/GXDevice/icGLext.h"


class icIndexBufGL : public icIndexBuf
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icIndexBufGL(void);
    virtual ~icIndexBufGL(void) {Cleanup();};

    virtual ICRESULT Cleanup(void);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT Lock(icIndexLock* pIndexLock);
    virtual ICRESULT Unlock(void);

    void* GetLocked() {return m_LockBuf;};
    GLuint& GetName() {return m_Name;};

    static bool m_bHardware;

private:
    GLuint      m_Name;
    void*       m_LockBuf;
};

#endif //ICGL
#endif //__IC_INDEX_BUFFER_GL_H__