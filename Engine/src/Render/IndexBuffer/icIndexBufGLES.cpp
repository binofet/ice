
#include "Render/IndexBuffer/icIndexBufGLES.h"
#ifdef ICGLES

/*! c'tor
**/
icIndexBufGLES::icIndexBufGLES(void) : icIndexBuf()
{
    m_LockBuf = NULL;
}// END FUNCTION icIndexBufGLES(void)


/*! Cleanups up memory used by the index buffer
 *
 *    @returns    ICRESULT    Result of cleanup
 *
**/
ICRESULT icIndexBufGLES::Cleanup(void)
{
    ICRESULT res = IC_OK;
    if (m_LockBuf)
    {
        free(m_LockBuf);
        m_LockBuf = NULL;
        res = IC_WARN_ALREADY_LOCKED_IB;
    }
    return res;
}// END FUNCTION Cleanup(void)


/*! Locks an index buffer
 *
 *    @param[in/out]    pIndexLock  Contains pointer to locked index data
 *    @returns          ICRESULT    Result of locking the index buffer
 *
**/
ICRESULT icIndexBufGLES::Lock(icIndexLock* pIndexLock)
{
    if (!m_bLocked)
    {
        if (!m_LockBuf)
        {
            uint size = (m_IndexDef.indexSize == IC_INDEX_SIZE_32) ? 4 : 2;
            void* mem = malloc(m_IndexDef.numIndex * size);
            if (mem)
            {
                m_LockBuf = mem;
                pIndexLock->pData = m_LockBuf;
                pIndexLock->numIndex = m_IndexDef.numIndex;
                pIndexLock->indexSize = m_IndexDef.indexSize;
                m_bLocked = true;
                return IC_OK;
            }
            return IC_FAIL_OUT_OF_MEM;
        }
        pIndexLock->pData = m_LockBuf;
        pIndexLock->numIndex = m_IndexDef.numIndex;
        pIndexLock->indexSize = m_IndexDef.indexSize;
        m_bLocked = true;
        return IC_OK;
    }
    return IC_WARN_ALREADY_LOCKED_IB;
}// END FUNCTION Lock(icIndexLock* pIndexLock)


/*! Unlocks a locked index buffer
 *
 *    @returns    ICRESULT    Result of unlocking the index buffer
 *
**/
ICRESULT icIndexBufGLES::Unlock(void)
{
    if (m_bLocked && m_LockBuf)
    {
        m_bLocked = false;
        return IC_OK;
    }
    return IC_WARN_GEN;
}// END FUNCTION Unlock(void)

#endif // ICGLES