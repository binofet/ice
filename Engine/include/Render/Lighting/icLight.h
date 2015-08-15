#ifndef __IC_LIGHT_H__
#define __IC_LIGHT_H__

//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "Math/Vector/icVector3.h"
#include "Math/icColor.h"


/*! Provides an interface to a light object
**/
class icLight
{
public:
    icVector3       pos;
    icVector3       dir;

    icColor         ambient;
    icColor         diffuse;
    icColor         specular;

    icReal          spot_exponent;
    icReal          spot_cutoff;
    icReal          const_atten;
    icReal          linear_atten;
    icReal          quad_atten;
};

#endif //__IC_LIGHT_H__