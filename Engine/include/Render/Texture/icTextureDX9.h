#ifndef __IC_TEXTURE_DX9_H__
#define __IC_TEXTURE_DX9_H__

#include "Render/Texture/icTexture.h"

#ifdef ICDX9

#include "DirectX/Include/d3d9.h"
typedef IDirect3DTexture9 DX9TEX;

class icTextureDX9 : public icTexture
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icTextureDX9( void );
    ~icTextureDX9( void );

    ICRESULT Cleanup( void );

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    ICRESULT Lock( icTexLock* pTexLock );
    ICRESULT Unlock( uint level=0 );

	DX9TEX* GetTex();

};

#endif //__IC_TEXTURE_DX9_H__
#endif //ICDX9

