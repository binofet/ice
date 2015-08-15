
#include "Render/VertBuffer/icVertBufferDX9.h"

#ifdef ICDX9

#include "DirectX/Include/d3d9.h"

/*! Default constructor
**/
icVertBufferDX9::icVertBufferDX9( void ) : icVertBuffer()
{
}// END FUNCTION icVertBufferDX9( void )


/*! Default destructor
**/
icVertBufferDX9::~icVertBufferDX9( void )
{
    Cleanup();
}// END FUNCTION ~icVertBufferDX9( void )


/*! Cleanups up memory used by the vert buffer
 *
 *    @returns    ICRESULT    Result of cleanup
 *
**/
ICRESULT icVertBufferDX9::Cleanup( void )
{
    if (m_ptrVertBuff)
    {
        DX9VB* pVB = (DX9VB*)m_ptrVertBuff;
        if (m_bLocked)
        {
            if (FAILED(pVB->Unlock()))
            {
                // try to delete?
                m_bLocked = false;
                pVB->Release();
                m_ptrVertBuff = NULL;
                return IC_FAIL_GEN;
            }
            m_bLocked = false;
            pVB->Release();
            m_ptrVertBuff = NULL;
            return IC_WARN_DEL_LOCKED_VB;
        }
        pVB->Release();
        m_ptrVertBuff = NULL;
    }
    return IC_OK;
}// END FUNCTION Cleanup( void )


/*! Locks a vertex buffer
 *
 *    @param[in/out]    pVertLock    Contains pointer to locked vert data
 *    @returns        ICRESULT    Result of locking the vert buffer
 *
**/
ICRESULT icVertBufferDX9::Lock( icVertLock* pVertLock )
{
    assert(pVertLock);
    if (m_ptrVertBuff)
    {
        if (!m_bLocked)
        {
            DX9VB* pVB = (DX9VB*)m_ptrVertBuff;
            uint offset = 0;
            uint size = m_VertDef.uiVertSize*m_VertDef.uiNumVerts;
            DWORD flags = 0;
            if (FAILED(pVB->Lock( offset, size, &pVertLock->pData, flags )))
            {
                pVertLock->uiNumVerts=0;
                pVertLock->uiVertSize=0;
                pVertLock->pData=NULL;
                return IC_FAIL_LOCK_VB;
            }
            pVertLock->uiNumVerts=m_VertDef.uiNumVerts;
            pVertLock->uiVertSize=m_VertDef.uiVertSize;
            m_bLocked = true;
            return IC_OK;
        }
        return IC_WARN_ALREADY_LOCKED_VB;
    }
    return IC_FAIL_NO_VB;
}// END FUNCTION Lock( icVertLock* pVertLock )


/*! Unlocks a locked vertex buffer
 *
 *    @returns    ICRESULT    Result of unlocking the vert buffer
 *
**/
ICRESULT icVertBufferDX9::Unlock( void )
{
    if (m_ptrVertBuff)
    {
        if (m_bLocked)
        {
            DX9VB* pVB = (DX9VB*)m_ptrVertBuff;
            if (FAILED(pVB->Unlock()))
                return IC_FAIL_UNLOCK_VB;

            m_bLocked = false;
            return IC_OK;
        }
        return IC_WARN_NOT_LOCKED_VB;
    }
    return IC_FAIL_NO_VB;
}// END FUNCTION Unlock( void )

#endif //ICDX9

