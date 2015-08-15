#ifndef __IC_VERT_BUFFER_DX9_H__
#define __IC_VERT_BUFFER_DX9_H__

#include "Render/VertBuffer/icVertBuffer.h"

#ifdef ICDX9

#include "DirectX/Include/d3d9.h"
typedef IDirect3DVertexBuffer9 DX9VB;

class icVertBufferDX9 : public icVertBuffer
{
public:
    //////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icVertBufferDX9( void );
    ~icVertBufferDX9( void );

    ICRESULT Cleanup( void );

    //////////////////////////////////////////////////////////////////////
    // OPERATIONS
    ICRESULT Lock( icVertLock* pVertLock );
    ICRESULT Unlock( void );

	DWORD _fvf;
};

#endif //__IC_VERT_BUFFER_DX9_H__
#endif //ICDX9

