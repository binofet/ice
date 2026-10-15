
#include "ice/render/vert_buffer_gles.h"
namespace ice::render {

#ifdef ICGLES
} // namespace ice::render

#include "ice/render/gl/api.h"
namespace ice::render {

/*! c'tor
 **/
vert_buff_gles::vert_buff_gles(void) : vert_buffer() {
	m_LockBuf = NULL;
} // END FUNCTION icVertBuffGLES(void)

/*! Cleanups up memory used by the vert buffer
 *
 *    @returns    ICRESULT    Result of cleanup
 *
 **/
ice::result vert_buff_gles::Cleanup(void) {
	ice::result res = IC_OK;
	if (m_LockBuf) {
		free(m_LockBuf);
		m_LockBuf = NULL;
		res       = IC_WARN_ALREADY_LOCKED_VB;
	}
	return res;
} // END FUNCTION Cleanup( void )

/*! Locks a vertex buffer
 *
 *    @param[in/out]    pVertLock    Contains pointer to locked vert data
 *    @returns        ICRESULT    Result of locking the vert buffer
 *
 **/
ice::result vert_buff_gles::Lock(vert_lock *pVertLock) {
	if (!m_bLocked) {
		if (!m_LockBuf) {
			void *mem = malloc(m_VertDef.uiNumVerts * m_VertDef.uiVertSize);
			if (mem) {
				m_LockBuf             = mem;
				pVertLock->pData      = m_LockBuf;
				pVertLock->uiNumVerts = m_VertDef.uiNumVerts;
				pVertLock->uiVertSize = m_VertDef.uiVertSize;
				m_bLocked             = true;
				return IC_OK;
			}
			return IC_FAIL_OUT_OF_MEM;
		}
		pVertLock->pData      = m_LockBuf;
		pVertLock->uiNumVerts = m_VertDef.uiNumVerts;
		pVertLock->uiVertSize = m_VertDef.uiVertSize;
		m_bLocked             = true;
		return IC_OK;
	}
	return IC_WARN_ALREADY_LOCKED_VB;
} // END FUNCTION Lock( icVertLock* pVertLock )

/*! Unlocks a locked vertex buffer
 *
 *    @returns    ICRESULT    Result of unlocking the vert buffer
 *
 **/
ice::result vert_buff_gles::Unlock(void) {
	if (m_bLocked && m_LockBuf) {
		m_bLocked = false;
		return IC_OK;
	}
	return IC_WARN_GEN;
} // END FUNCTION Unlock( void )

#endif // ICGLES

} // namespace ice::render
