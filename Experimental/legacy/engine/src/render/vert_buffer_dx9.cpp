
#include "ice/render/vert_buffer_dx9.h"
namespace ice::render {

#ifdef ICDX9
} // namespace ice::render

#include "DirectX/Include/d3d9.h"
namespace ice::render {

/*! Default constructor
 **/
vert_buffer_dx9::vert_buffer_dx9(void) : vert_buffer() {} // END FUNCTION icVertBufferDX9( void )

/*! Default destructor
 **/
vert_buffer_dx9::~vert_buffer_dx9(void) { Cleanup(); } // END FUNCTION ~icVertBufferDX9( void )

/*! Cleanups up memory used by the vert buffer
 *
 *    @returns    ICRESULT    Result of cleanup
 *
 **/
ice::result vert_buffer_dx9::Cleanup(void) {
	if (m_ptrVertBuff) {
		DX9VB *pVB = (DX9VB *)m_ptrVertBuff;
		if (m_bLocked) {
			if (FAILED(pVB->Unlock())) {
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
} // END FUNCTION Cleanup( void )

/*! Locks a vertex buffer
 *
 *    @param[in/out]    pVertLock    Contains pointer to locked vert data
 *    @returns        ICRESULT    Result of locking the vert buffer
 *
 **/
ice::result vert_buffer_dx9::Lock(vert_lock *pVertLock) {
	assert(pVertLock);
	if (m_ptrVertBuff) {
		if (!m_bLocked) {
			DX9VB    *pVB    = (DX9VB *)m_ptrVertBuff;
			ice::uint offset = 0;
			ice::uint size   = m_VertDef.uiVertSize * m_VertDef.uiNumVerts;
			DWORD     flags  = 0;
			if (FAILED(pVB->Lock(offset, size, &pVertLock->pData, flags))) {
				pVertLock->uiNumVerts = 0;
				pVertLock->uiVertSize = 0;
				pVertLock->pData      = NULL;
				return IC_FAIL_LOCK_VB;
			}
			pVertLock->uiNumVerts = m_VertDef.uiNumVerts;
			pVertLock->uiVertSize = m_VertDef.uiVertSize;
			m_bLocked             = true;
			return IC_OK;
		}
		return IC_WARN_ALREADY_LOCKED_VB;
	}
	return IC_FAIL_NO_VB;
} // END FUNCTION Lock( icVertLock* pVertLock )

/*! Unlocks a locked vertex buffer
 *
 *    @returns    ICRESULT    Result of unlocking the vert buffer
 *
 **/
ice::result vert_buffer_dx9::Unlock(void) {
	if (m_ptrVertBuff) {
		if (m_bLocked) {
			DX9VB *pVB = (DX9VB *)m_ptrVertBuff;
			if (FAILED(pVB->Unlock()))
				return IC_FAIL_UNLOCK_VB;

			m_bLocked = false;
			return IC_OK;
		}
		return IC_WARN_NOT_LOCKED_VB;
	}
	return IC_FAIL_NO_VB;
} // END FUNCTION Unlock( void )

#endif // ICDX9

} // namespace ice::render
