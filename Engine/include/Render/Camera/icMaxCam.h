#ifndef __IC_MAX_CAMERA_H__
#define __IC_MAX_CAMERA_H__


#include "icCamera.h"



/*! Provides a basic camera object
**/
class icMaxCam : public icCamera
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icMaxCam( void );
    virtual ~icMaxCam( void );

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual void Update( const float fDeltaTime );

    icReal& Zoom(void) { return m_rZoom; };
    icReal& ZoomMin(void) { return m_rZoomMin; };
    icReal& ZoomMax(void) { return m_rZoomMax; };

    icReal& Xrot(void) { return m_rXrot; };
    icReal& Yrot(void) { return m_rYrot; };

    icVector3& Center(void) { return m_v3Center; };

    bool    bEnableInput;

private:

    icReal      m_rZoom;
    icReal      m_rXrot;
    icReal      m_rYrot;

    icReal      m_rZoomMin;
    icReal      m_rZoomMax;

    icVector3   m_v3Center;
};


#endif //__IC_MAX_CAMERA_H__
