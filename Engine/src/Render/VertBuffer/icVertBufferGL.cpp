
#include "Render/VertBuffer/icVertBufferGL.h"

#include "Core/GXDevice/icGLext.h"


bool icVertBuffGL::m_bHardware=false;

/*! c'tor
**/
icVertBuffGL::icVertBuffGL(void) : icVertBuffer()
{
    m_LockBuf=NULL;
    m_Name=0xFFFFFFFF;
}// END FUNCTION icVertBuffGL(void)


/*! Cleanups up memory used by the vert buffer
 *
 *    @returns    ICRESULT    Result of cleanup
 *
**/
ICRESULT icVertBuffGL::Cleanup( void )
{
    ICRESULT res = IC_OK;
    if (m_LockBuf)
    {
        free(m_LockBuf);
        m_LockBuf = NULL;
        res = IC_WARN_ALREADY_LOCKED_VB;
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
}// END FUNCTION Cleanup( void )


/*! Locks a vertex buffer
 *
 *    @param[in/out]    pVertLock   Contains pointer to locked vert data
 *    @returns          ICRESULT    Result of locking the vert buffer
**/
ICRESULT icVertBuffGL::Lock(icVertLock* pVertLock)
{
    if (!m_bLocked)
    {
        // hardware buffers only get a temp system copy from openGL
        if (m_bHardware)
        {
            glBindBuffer(GL_ARRAY_BUFFER, m_Name);
            m_LockBuf = glMapBuffer(GL_ARRAY_BUFFER,GL_READ_WRITE);
            if (!m_LockBuf)
            {
                GLenum err = glGetError();
                icWarningf("icVertBuffGL::Lock() - glMapBuffer failed, error: %i", err);
                return IC_FAIL_LOCK_VB;
            }
        }
        else
        {
            // TODO: check for buffer size changes
            // check if we already have the space allocated
            if (!m_LockBuf)
            {
                // we want to switch this to a memory pool using doug leas allocator
                void* mem = malloc(m_VertDef.uiNumVerts * m_VertDef.uiVertSize);
                if (mem)
                {
                    m_LockBuf = mem;
                    pVertLock->pData = m_LockBuf;
                    pVertLock->uiNumVerts = m_VertDef.uiNumVerts;
                    pVertLock->uiVertSize = m_VertDef.uiVertSize;
                    m_bLocked = true;
                    return IC_OK;
                }
                pVertLock->pData = 0;
                pVertLock->uiNumVerts = 0;
                pVertLock->uiVertSize = 0;
                return IC_FAIL_OUT_OF_MEM;
            }
        }
        pVertLock->pData = m_LockBuf;
        pVertLock->uiNumVerts = m_VertDef.uiNumVerts;
        pVertLock->uiVertSize = m_VertDef.uiVertSize;
        m_bLocked = true;
        return IC_OK;
    }
    return IC_WARN_ALREADY_LOCKED_VB;
}// END FUNCTION Lock( icVertLock* pVertLock )


/*! Unlocks a locked vertex buffer
 *
 *    @returns    ICRESULT    Result of unlocking the vert buffer
 *
**/
ICRESULT icVertBuffGL::Unlock( void )
{
    if (m_bLocked && m_LockBuf)
    {
        // TODO: provide a mechanism to prevent two different Vertex
        //       Buffers from obtaining a lock, opengl does not support
        //       that.
        if (m_bHardware)
        {
            glUnmapBuffer(GL_ARRAY_BUFFER);
            glBindBuffer(GL_ARRAY_BUFFER,0);
            m_LockBuf = NULL;
        }
        else
        {
            // memory stuff?
        }
        m_bLocked = false;
        return IC_OK;
    }
    return IC_WARN_GEN;
}// END FUNCTION Unlock( void )

