
#include "Render/IndexBuffer/icIndexBufDX9.h"

#ifdef ICDX9

/*! Default constructor
**/
icIndexBufDX9::icIndexBufDX9( void ) : icIndexBuf()
{
}// END FUNCTION icIndexBufDX9( void )


/*! Default destructor
**/
icIndexBufDX9::~icIndexBufDX9( void )
{
    Cleanup();
}// END FUNCTION ~icIndexBufDX9( void )


/*! Cleans up the index buffer
 *
 *    @returns    ICRESULT    Result of index buffer cleanup
 *
**/
ICRESULT icIndexBufDX9::Cleanup( void )
{
    if (m_pIndexBuf)
    {
        DX9IB* pIB = (DX9IB*)m_pIndexBuf;
        if (m_bLocked)
        {
            if (FAILED(pIB->Unlock()))
            {
                // try to delete?
                m_bLocked = false;
                pIB->Release();
                m_pIndexBuf = NULL;
                return IC_FAIL_GEN;
            }
            m_bLocked = false;
            pIB->Release();
            m_pIndexBuf = NULL;
            return IC_WARN_DEL_LOCKED_IB;
        }
        pIB->Release();
        m_pIndexBuf = NULL;
    }
    return IC_OK;
}// END FUNCTION Cleanup( void )


/*! Locks the index buffer data
 *
 *    @param[in/out]    pIndexLock    Contains pointer to locked data
 *    @returns        ICRESULT    Result of locking the index data
 *
**/
ICRESULT icIndexBufDX9::Lock( icIndexLock* pIndexLock )
{
    assert( pIndexLock );
    if (m_pIndexBuf)
    {
        if (!m_bLocked)
        {
            DX9IB* pIB = (DX9IB*)m_pIndexBuf;
            uint offset = 0;
            uint size = 2;
            if (m_IndexDef.indexSize == IC_INDEX_SIZE_32)
                size = 4;
            size *= m_IndexDef.numIndex;
            DWORD flags = 0;
            if (FAILED(pIB->Lock(offset,size,&pIndexLock->pData, flags)))
            {
                pIndexLock->numIndex = 0;
                pIndexLock->indexSize = IC_INDEX_SIZE_UNKOWN;
                pIndexLock->pData = NULL;
                return IC_FAIL_LOCK_IB;
            }
			m_bLocked = true;
            pIndexLock->numIndex = m_IndexDef.numIndex;
            pIndexLock->indexSize = m_IndexDef.indexSize;
            return IC_OK;
        }
        return IC_WARN_ALREADY_LOCKED_IB;
    }
    return IC_FAIL_NO_IB;
}// END FUNCTION Lock( icIndexLock* pIndexLock )


/*! Unlocks the index buffer data
 *
 *    @returns    ICRESULT    Result of unlocking data
 *
**/
ICRESULT icIndexBufDX9::Unlock( void )
{
    if (m_pIndexBuf)
    {
        if (m_bLocked)
        {
            DX9IB* pIB = (DX9IB*)m_pIndexBuf;
            if (FAILED(pIB->Unlock()))
                return IC_FAIL_UNLOCK_IB;
            m_bLocked=false;
            return IC_OK;
        }
        return IC_WARN_NOT_LOCKED_IB;
    }
    return IC_FAIL_NO_IB;
}// END FUNCTION Unlock( void )


#endif //ICDX9

