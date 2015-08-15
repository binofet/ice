#ifndef __IC_MATERIAL_H__
#define __IC_MATERIAL_H__

#include "Render/Texture/icTexture.h"

class icMaterial
{
public:
    icMaterial( void );
    virtual ~icMaterial( void );


    virtual ICRESULT StartPass( void );
    virtual ICRESULT EndPass( void );

protected:

    icTexture*    m_pTex0;
    icTexture*    m_pTex1;
    icTexture*    m_pTex2;
    icTexture*    m_pTex3;

    uint    m_ptrEffect;

    char    m_szTechnique[32];
};

#endif //__IC_MATERIAL_H__