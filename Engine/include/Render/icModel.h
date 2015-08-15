#ifndef __IC_MODEL_H__
#define __IC_MODEL_H__


#include "Core/icGlobals.h"

#include "Math/Matrix/icMatrix44.h"


#define ICE_MODEL_FORMAT  102

struct _ICE_MODEL
{
    uint modelVersion;
    char modelName[64];
    char difTex[128];
    char normTex[128];
    char specTex[128];
    uint numVerts;
    uint numInd;
    uint vertType;
};

struct _ICE_MODEL_100
{
    uint modelVersion;
    char modelName[64];
    char difTex[128];
    char normTex[128];
    char specTex[128];
    uint numVerts;
    uint numInd;
};

class icTexture;
class icVertBuffer;
class icIndexBuf;
class icGXDevice;


/*! Provides an object for interacting with models
**/
class icModel
{
public:
    icModel(void);
    ~icModel(void);


    ICRESULT Render(const icMatrix44& trans);

    int operator<(const icModel& Rhs);
    int operator>(const icModel& Rhs);
    int operator<=(const icModel& Rhs);
    int operator>=(const icModel& Rhs);
    int operator==(const icModel& Rhs);
    int operator!=(const icModel& Rhs);

private:
    icVertBuffer*   m_pVB;
    icIndexBuf*     m_pIB;
    icTexture*      m_difTex;
    icTexture*      m_normTex;
    icTexture*      m_specTex;

    icGXDevice*     m_pDevice;
    
    _ICE_MODEL*     m_pHeader;
    
    friend class icContentLoader;
};

#endif //__IC_MODEL_H__