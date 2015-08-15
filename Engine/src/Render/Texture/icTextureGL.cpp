
#include "Render/Texture/icTextureGL.h"

#ifdef ICGL

/*! c'tor
**/
icTextureGL::icTextureGL(void) : icTexture()
{
    m_Name = 0xFFFFFFFF;
    m_LockBits = NULL;
}// END FUNCTION icTextureGL(void)


/*! Cleans up memory on system/graphics device
 *
 *  @returns    ICRESULT    Result of cleanup
**/
ICRESULT icTextureGL::Cleanup(void)
{
    if (m_LockBits)
    {
        free(m_LockBits);
        m_LockBits = NULL;
    }
    return IC_OK;
}// END FUNCTION Cleanup(void)


/*! Locks the texture into system memory
 *
 *  @param[in/out]  pTexLock    Holds information about the texture
 *  @returns        ICRESULT    Result of locking the texture
**/
ICRESULT icTextureGL::Lock(icTexLock* pTexLock)
{
    if (!m_bLocked)
    {
        if (!m_LockBits)
        {
            uint depth = 32; //TODO: get from data
            void* mem = malloc(m_TexDef.width * m_TexDef.height * depth);
            if (mem)
            {
                m_LockBits = mem;
                pTexLock->pData = m_LockBits;
                pTexLock->format = m_TexDef.format;
                pTexLock->height = m_TexDef.height;
                pTexLock->width = m_TexDef.width;
                pTexLock->pitch = 0; //TODO
                pTexLock->level = 0; //TODO
                m_bLocked = true;
                return IC_OK;
            }
            return IC_FAIL_OUT_OF_MEM;
        }
        pTexLock->pData = m_LockBits;
        pTexLock->format = m_TexDef.format;
        pTexLock->height = m_TexDef.height;
        pTexLock->width = m_TexDef.width;
        pTexLock->pitch = 0; //TODO
        pTexLock->level = 0; //TODO
        m_bLocked = true;
        return IC_OK;
    }
    return IC_WARN_ALREADY_LOCKED_VB;
}// END FUNCTION Lock(icTexLock* pTexLock)


/*! Unlocks a locked texture
 *
 *  @param      level       What level of texture to unlock
 *  @returns    ICRESULT    Result of unlocking texture
**/
ICRESULT icTextureGL::Unlock(uint level)
{
    if (m_bLocked && m_LockBits)
    {
        glTexImage2D(GL_TEXTURE_2D,level,3,m_TexDef.width,m_TexDef.height,0,GL_RGB,GL_UNSIGNED_BYTE,m_LockBits);
        m_bLocked = false;
        free(m_LockBits);
        m_LockBits = NULL;
        return IC_OK;
    }
    return IC_WARN_GEN;
}// END FUNCTION Unlock(uint level)

#endif //ICGL