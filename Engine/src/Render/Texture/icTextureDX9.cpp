
#include "Render/Texture/icTextureDX9.h"

#ifdef ICDX9

/*! Default constructor
**/
icTextureDX9::icTextureDX9( void ) : icTexture()
{
}// END FUNCTION icTextureDX9( void )


/*! Default destructor
**/
icTextureDX9::~icTextureDX9( void )
{
    Cleanup();
}// END FUNCTION ~icTextureDX9( void )


/*! Cleans up this texture
 *
 *    @returns    ICRESULT    Result of texture cleanup
 *
**/
ICRESULT icTextureDX9::Cleanup( void )
{
    if (m_ptrTex)
    {
        DX9TEX* tex = (DX9TEX*)m_ptrTex;
        if (m_bLocked)
        {
            //! not sure about this
            tex->Release();
            m_ptrTex = NULL;
            return IC_FAIL_GEN;
        }
        tex->Release();
        m_ptrTex = NULL;
        return IC_OK;
    }
    return IC_OK;
}// END FUNCTION Cleanup( void )


/*! Locks the texture
 *
 *    @param[in/out]    pTexLock    Contains the pointer to data
 *    @returns        ICRESULT    Result of locking the texture
 *
**/
ICRESULT icTextureDX9::Lock( icTexLock* pTexLock )
{
    if (m_ptrTex)
    {
        // Make sure they specified this texture as lockable
        //if (!m_TexDef.lockable)
        //{
        //    pTexLock->format = 0;
        //    pTexLock->height = 0;
        //    pTexLock->width = 0;
        //    pTexLock->pitch = 0;
        //    pTexLock->pData = NULL;
        //    return IC_FAIL_NOT_LOCKABLE_TEX;
        //}

        // Make sure this texture is not already locked
        if (!m_bLocked)
        {
            DX9TEX* tex = (DX9TEX*)m_ptrTex;
            D3DLOCKED_RECT locked;
            // TODO, allow partial locks
            //RECT rect;
            DWORD flags = 0;
            if (FAILED(tex->LockRect(pTexLock->level, &locked, NULL, flags )))
            {
                pTexLock->format = 0;
                pTexLock->height = 0;
                pTexLock->width = 0;
                pTexLock->pitch = 0;
                pTexLock->pData = NULL;
                return IC_FAIL_LOCK_TEX;
            }
            pTexLock->format = m_TexDef.format;
            pTexLock->height = m_TexDef.height;
            pTexLock->width = m_TexDef.width;
            pTexLock->pitch = locked.Pitch;
            pTexLock->pData = locked.pBits;
            return IC_OK;
        }
        return IC_WARN_ALREADY_LOCKED_TEX;
    }
    return IC_FAIL_NO_TEX;
}// END FUNCTION Lock( icTexLock* pTexLock )


/*! Unlocks an alread locked texture
 *
 *    @returns    ICRESULT    Result of unlocking the texture
 *
**/
ICRESULT icTextureDX9::Unlock( uint level )
{
    if (m_ptrTex)
    {
        if (m_bLocked)
        {
            DX9TEX* tex = (DX9TEX*)m_ptrTex;
            if (FAILED(tex->UnlockRect( level )))
            {
                return IC_FAIL_UNLOCK_TEX;
            }
            m_bLocked=false;
            return IC_OK;
        }
        return IC_WARN_NOT_LOCKED_TEX;
    }
    return IC_FAIL_NO_TEX;
}// END FUNCTION Unlock( void )

DX9TEX* icTextureDX9::GetTex()
{
	return (DX9TEX*)m_ptrTex;
}

#endif //ICDX9

