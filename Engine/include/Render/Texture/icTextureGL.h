#ifndef __IC_TEXTURE_GL_H__
#define __IC_TEXTURE_GL_H__

#include "Render/Texture/icTexture.h"

#ifdef ICGL
#include "Core/GXDevice/icGLext.h"

class icTextureGL : public icTexture
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icTextureGL(void);
    virtual ~icTextureGL(void) {Cleanup();};

    virtual ICRESULT Cleanup(void);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT Lock(icTexLock* pTexLock);
    virtual ICRESULT Unlock(uint level=0);

    void* GetLocked(void) {return m_LockBits;};
    GLuint& GetName(void) {return m_Name;};

    static bool m_bHardware;

private:
    GLuint  m_Name;
    void*   m_LockBits;
};

#endif //ICGL
#endif //__IC_TEXTURE_GL_H__