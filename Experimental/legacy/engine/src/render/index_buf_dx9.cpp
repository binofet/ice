
#include "ice/render/index_buf_dx9.h"
namespace ice::render {

#ifdef ICDX9

/*! Default constructor
 **/
index_buf_dx9::index_buf_dx9(void) : index_buf() {} // END FUNCTION icIndexBufDX9( void )

/*! Default destructor
 **/
index_buf_dx9::~index_buf_dx9(void) { Cleanup(); } // END FUNCTION ~icIndexBufDX9( void )

/*! Cleans up the index buffer
 *
 *    @returns    ICRESULT    Result of index buffer cleanup
 *
 **/
ice::result index_buf_dx9::Cleanup(void) {
	if (m_pIndexBuf) {
		DX9IB *pIB = (DX9IB *)m_pIndexBuf;
		if (m_bLocked) {
			if (FAILED(pIB->Unlock())) {
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
} // END FUNCTION Cleanup( void )

/*! Locks the index buffer data
 *
 *    @param[in/out]    pIndexLock    Contains pointer to locked data
 *    @returns        ICRESULT    Result of locking the index data
 *
 **/
ice::result index_buf_dx9::Lock(index_lock *pIndexLock) {
	assert(pIndexLock);
	if (m_pIndexBuf) {
		if (!m_bLocked) {
			DX9IB    *pIB    = (DX9IB *)m_pIndexBuf;
			ice::uint offset = 0;
			ice::uint size   = 2;
			if (m_IndexDef.indexSize == ic_index_size_32)
				size = 4;
			size *= m_IndexDef.numIndex;
			DWORD flags = 0;
			if (FAILED(pIB->Lock(offset, size, &pIndexLock->pData, flags))) {
				pIndexLock->numIndex  = 0;
				pIndexLock->indexSize = ic_index_size_unkown;
				pIndexLock->pData     = NULL;
				return IC_FAIL_LOCK_IB;
			}
			m_bLocked             = true;
			pIndexLock->numIndex  = m_IndexDef.numIndex;
			pIndexLock->indexSize = m_IndexDef.indexSize;
			return IC_OK;
		}
		return IC_WARN_ALREADY_LOCKED_IB;
	}
	return IC_FAIL_NO_IB;
} // END FUNCTION Lock( icIndexLock* pIndexLock )

/*! Unlocks the index buffer data
 *
 *    @returns    ICRESULT    Result of unlocking data
 *
 **/
ice::result index_buf_dx9::Unlock(void) {
	if (m_pIndexBuf) {
		if (m_bLocked) {
			DX9IB *pIB = (DX9IB *)m_pIndexBuf;
			if (FAILED(pIB->Unlock()))
				return IC_FAIL_UNLOCK_IB;
			m_bLocked = false;
			return IC_OK;
		}
		return IC_WARN_NOT_LOCKED_IB;
	}
	return IC_FAIL_NO_IB;
} // END FUNCTION Unlock( void )

#endif // ICDX9

} // namespace ice::render
