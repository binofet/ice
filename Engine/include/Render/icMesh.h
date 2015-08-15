#ifndef __IC_MESH_H__
#define __IC_MESH_H__


#include "Core/icGlobals.h"
#include "Math/Matrix/icMatrix44.h"
#include "Render/VertBuffer/icVertBuffer.h"
#include "Render/IndexBuffer/icIndexBuf.h"
#include "Render/Texture/icTexture.h"

/*! Renderable mesh with a vertex buffer, index buffer (optional),
 *  a texture (optional), and a rendering technique
**/
class icMesh
{
public:
    icMesh(void);
    ~icMesh(void);

    ICRESULT Update(const float fDeltaTime);
    ICRESULT Render(const icMatrix44& transform);

private:
    icVertBuffer*   m_vb;
    icIndexBuf*     m_ib;
    icTexture*      m_difTex;
    icTexture*      m_normTex;
    icTexture*      m_specTex;
};

#endif //__IC_MESH_H__