#ifndef __IC_CHASE_CAMERA_H__
#define __IC_CHASE_CAMERA_H__


#include "icCamera.h"


/*! Provides a mass-spring-dampened chase camera
**/
class icChaseCam : public icCamera
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icChaseCam(void);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual void Update(const float fDeltaTime);

    void SetCameraPos(const icVector3& pos);

    void SetCamPhysics(icReal mass, icReal damping, icReal stiffness);

    void SetChaseXform(icMatrix44* pTrans);
    void SetChaseOffset(const icVector3& posOffset);
    void SetLookOffset(const icVector3& lookOffset);


protected:

    icReal          m_rMass;                //! Mass of the camera
    icReal          m_rDamping;             //! Internal spring friction(aprx)
    icReal          m_rStiffness;           //! Spring stiffness coefficient

    icVector3       m_v3Velocity;           //! Current camera velocity

    icVector3       m_v3DesiredPos;         //! Calculated desired position
    icVector3       m_v3LookOffset;         //! Camera look offset
    icVector3       m_v3ChaseOffset;        //! Offset from chase object
    icMatrix44*     m_pChaseObjectTrans;    //! Pointer to chase objects xform
};


/*! Sets the cameras position, resets current velocity
 *
 *  @param          pos         Position to warp camera to
**/
FORCE_INLINE void icChaseCam::SetCameraPos(const icVector3& pos)
{
    m_m44ViewMat.SetPos(pos);
    m_v3Velocity.Set(0.0f,0.0f,0.0f);
}// END FUNCTION SetCameraPos(const icVector3& pos)


/*! Sets the physics parameters of the chase camera
 *
 *  @param          mass        Mass of the camera
 *  @param          damping     Internal friction of the spring
 *  @param          stiffness   Spring stiffness
**/
FORCE_INLINE 
void icChaseCam::SetCamPhysics(icReal mass, icReal damping, icReal stiffness)
{
    m_v3Velocity.Set(0.0f,0.0f,0.0f);

    m_rMass = mass;
    m_rDamping = damping;
    m_rStiffness = stiffness;
}// END FUNCTION SetCamPhysics(icReal mass, icReal damping, icReal stiffness)


/*! Sets the transform this camera is to be attached to
 *
 *  @param          pTrans      Pointer to chase target transform
**/
FORCE_INLINE void icChaseCam::SetChaseXform(icMatrix44* pTrans)
{
    m_v3Velocity.Set(0.0f,0.0f,0.0f);

    m_pChaseObjectTrans = pTrans;
    m_m44ViewMat = *m_pChaseObjectTrans;
}// END FUNCTION SetChaseXform(icMatrix44* pTrans)


/*! Sets the offset the camera should have from the chase object
 *
 *  @param          posOffset       Positional offset from chase object
**/
FORCE_INLINE void icChaseCam::SetChaseOffset( const icVector3& posOffset )
{
    m_v3Velocity.Set(0.0f,0.0f,0.0f);

    m_v3ChaseOffset = posOffset;
}// END FUNCTION SetChaseOffset(const icVector3& posOffset)


/*! Sets the relative look vector the camera should have from the chase object
 *
 *  @param          lookOffset      Look offset from chase object
**/
FORCE_INLINE void icChaseCam::SetLookOffset(const icVector3& lookOffset)
{
    m_v3Velocity.Set(0.0f,0.0f,0.0f);

    m_v3LookOffset = lookOffset;
}// END FUNCTION SetLookOffset(const icVector3& lookOffset)

#endif //__IC_CHASE_CAMERA_H__