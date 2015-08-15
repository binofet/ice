#ifndef __IC_RENDER_OBJECT_H__
#define __IC_RENDER_OBJECT_H__

//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "Render/VertBuffer/icVertBuffer.h"
#include "Render/IndexBuffer/icIndexBuf.h"
#include "Core/GXDevice/icGXDevice.h"
#include "Math/Matrix/icMatrix44.h"



/*! Provides a simple interface to render objects
**/
class icRenderObject
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icRenderObject(icGXDevice* pDevice);
    virtual ~icRenderObject(void);

    virtual ICRESULT Cleanup(void);


    //////////////////////////////////////////////////////////////////////////
    // FACTORY METHODS
    ICRESULT CreateSphere(const icReal& radius, const icColor& color,
                          int subdiv=10);

    ICRESULT CreateBox(const icVector3& v3Dimensions, const icColor& color,
                       int subdiv=1, class icTexture* pTex=NULL);

    ICRESULT CreatePlane(const icReal& length, const icReal& width,
                         const icColor& color, int numrow=10, int numcol=10);

	ICRESULT CreateLines(icVector3* pVerts, const icColor& color,
						 int numPoints, bool list=false);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT Update(const float fDeltaTime);
    virtual ICRESULT Render(void);

    //////////////////////////////////////////////////////////////////////////
    // ACCESS
    icMatrix44& GetTransform(void) {return m_m44Trans;};

private:
    icGXDevice*         m_pDev;

    icVertBuffer*       m_pVB;
    icIndexBuf*         m_pIB;

    icMatrix44          m_m44Trans;

    class icTexture*    m_pTex;
};

#endif //__IC_RENDER_OBJECT_H__
