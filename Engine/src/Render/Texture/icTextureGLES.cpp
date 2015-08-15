
#include "Render/Texture/icTextureGLES.h"

#ifdef ICGLES

/*! c'tor
**/
icTextureGLES::icTextureGLES(void) : icTexture()
{
    m_Name = 0xFFFFFFFF;
    m_LockBits = NULL;
}// END FUNCTION icTextureGLES(void)


/*! Cleans up memory on system/graphics device
 *
 *  @returns    ICRESULT    Result of cleanup
**/
ICRESULT icTextureGLES::Cleanup(void)
{
    if (m_LockBits)
    {
        free(m_LockBits);
        m_LockBits = NULL;
    }
    return IC_FAIL_GEN;
}// END FUNCTION Cleanup(void)


/*! Locks the texture into system memory
 *
 *  @param[in/out]  pTexLock    Holds information about the texture
 *  @returns        ICRESULT    Result of locking the texture
**/
ICRESULT icTextureGLES::Lock(icTexLock* pTexLock)
{
    return IC_FAIL_GEN;
}// END FUNCTION Lock(icTexLock* pTexLock)


/*! Unlocks a locked texture
 *
 *  @param      level       What level of texture to unlock
 *  @returns    ICRESULT    Result of unlocking texture
**/
ICRESULT icTextureGLES::Unlock(uint level)
{
    return IC_FAIL_GEN;
}// END FUNCTION Unlock(uint level)

#endif //ICGLES