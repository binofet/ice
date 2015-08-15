#ifndef __IC_INDEX_SIZEFER_DX9_H__
#define __IC_INDEX_SIZEFER_DX9_H__

#include "Render/IndexBuffer/icIndexBuf.h"

#ifdef ICDX9

#include "DirectX/Include/d3d9.h"
typedef IDirect3DIndexBuffer9 DX9IB;

class icIndexBufDX9 : public icIndexBuf
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icIndexBufDX9( void );
    ~icIndexBufDX9( void );

    ICRESULT Cleanup( void );

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    ICRESULT Lock( icIndexLock* pIndexLock );
    ICRESULT Unlock( void );

	DX9IB* GetIB() { return (DX9IB*)m_pIndexBuf; };
};

#endif //__IC_INDEX_SIZEFER_DX9_H__
#endif //ICDX9

