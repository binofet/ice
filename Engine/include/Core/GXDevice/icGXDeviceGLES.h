#ifndef __IC_GDEVICE_GLES_H__
#define __IC_GDEVICE_GLES_H__


#include "Core/GXDevice/icGXDevice.h"

#ifdef ICGLES


#include <OpenGLES/ES1/gl.h>
#include <OpenGLES/ES1/glext.h>

class icGXDeviceGLES : public icGXDevice
    {
    public:
        //////////////////////////////////////////////////////////////////////
        // LIFECYCLE
        icGXDeviceGLES( void );
        virtual ~icGXDeviceGLES( void );

        virtual ICRESULT Cleanup( void );
        virtual ICRESULT Init( void* hWnd, bool bFullScreen );

        //////////////////////////////////////////////////////////////////////
        // OPERATIONS
        virtual ICRESULT Update( void );

        virtual void FrameBegin( void );
        virtual void FrameEnd( void );

        virtual ICRESULT DrawVertBuf( class icVertBuffer* pVB );

        virtual ICRESULT DrawIndexedVertBuf(class icVertBuffer* pVB,
                                            class icIndexBuf* pIB );

        virtual ICRESULT CreateVB(  class icVertBuffer** ppVB,
                                  struct icVertDef* pVertDef );
        virtual ICRESULT ReleaseVB( class icVertBuffer* pVB );

        virtual ICRESULT CreateIB(  class icIndexBuf** ppIB,
                                  struct icIndexDef* pIndexDef );
        virtual ICRESULT ReleaseIB( class icIndexBuf* pIB );

        virtual ICRESULT CreateTex( class icTexture** ppTex,
                                   struct icTexDef* pTexDef );
        virtual ICRESULT ReleaseTex( class icTexture* pTex );

        //////////////////////////////////////////////////////////////////////
        // ACCESS
        virtual ICRESULT SetCullMode( IC_CULL_MODE cMode_e );

    private:
    };

#endif //__IC_GDEVICE_GLES_H__
#endif // ICGLES

