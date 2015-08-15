#ifndef __IC_TEXTURE_GLES_H__
#define __IC_TEXTURE_GLES_H__

#include "Render/Texture/icTexture.h"

#ifdef ICGLES
#include "Core/GXDevice/icGLext.h"

class icTextureGLES : public icTexture
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icTextureGLES(void);
    virtual ~icTextureGLES(void) {Cleanup();};

    virtual ICRESULT Cleanup(void);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT Lock(icTexLock* pTexLock);
    virtual ICRESULT Unlock(uint level=0);

    void* GetLocked(void) {return m_LockBits;};
    GLuint& GetName(void) {return m_Name;};

private:
    GLuint  m_Name;
    void*   m_LockBits;
};

#endif //ICGLES
#endif //__IC_TEXTURE_GLES_H__