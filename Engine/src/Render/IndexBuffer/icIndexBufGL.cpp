
#include "Render/IndexBuffer/icIndexBufGL.h"

bool icIndexBufGL::m_bHardware=false;


/*! c'tor
**/
icIndexBufGL::icIndexBufGL(void) : icIndexBuf()
{
    m_Name = 0xFFFFFFFF;
    m_LockBuf = NULL;
}// END FUNCTION icIndexBufGL(void)


/*! Cleanups up memory used by the index buffer
 *
 *    @returns    ICRESULT    Result of cleanup
 *
**/
ICRESULT icIndexBufGL::Cleanup(void)
{
    ICRESULT res = IC_OK;
    if (m_LockBuf)
    {
        if (!m_bHardware)
            free(m_LockBuf);

        m_LockBuf = NULL;
        res = IC_WARN_ALREADY_LOCKED_IB;
    }
    if (m_bHardware)
    {
        if (m_Name != 0xFFFFFFFF)
        {
            glDeleteBuffers(1, &m_Name);
            m_Name = 0xFFFFFFFF;
        }
    }
    return res;
}// END FUNCTION Cleanup(void)


/*! Locks an index buffer
 *
 *    @param[in/out]    pIndexLock  Contains pointer to locked index data
 *    @returns          ICRESULT    Result of locking the index buffer
 *
**/
ICRESULT icIndexBufGL::Lock(icIndexLock* pIndexLock)
{
    if (!m_bLocked)
    {
        // hardware buffers only get a temp system copy from openGL
        if (m_bHardware)
        {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_Name);
            m_LockBuf = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER,GL_READ_WRITE);
            if (!m_LockBuf)
            {
                GLenum err = glGetError();
                icWarningf("icIndexBufGL::Lock() - glMapBuffer failed, error: %i", err);
                return IC_FAIL_LOCK_IB;
            }
        }
        else
        {
            // TODO: check for buffer size changes
            // check if we already have the space allocated
            if (!m_LockBuf)
            {
                // we want to switch this to a memory pool using doug leas allocator
                uint size = (m_IndexDef.indexSize == IC_INDEX_SIZE_16) ? 2 : 4;
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
                pIndexLock->pData = 0;
                pIndexLock->numIndex = 0;
                return IC_FAIL_OUT_OF_MEM;
            }
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
ICRESULT icIndexBufGL::Unlock(void)
{
    if (m_bLocked && m_LockBuf)
    {
        // TODO: provide a mechanism to prevent two different Index
        //       Buffers from obtaining a lock, opengl does not support
        //       that.
        if (m_bHardware)
        {
            glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
            m_LockBuf = NULL;
        }
        m_bLocked = false;
        return IC_OK;
    }
    return IC_WARN_GEN;
}// END FUNCTION Unlock(void)