
#include "Render/Camera/icChaseCam.h"

/*! default c'tor
**/
icChaseCam::icChaseCam(void)
: m_rMass(5), m_rDamping(600), m_rStiffness(100)
{
    m_v3Velocity.Set(0.0f,0.0f,0.0f);

    m_v3DesiredPos.Set(0.0f,0.0f,0.0f);
    m_v3LookOffset.Set(0.0f,0.2f,0.0f);
    m_v3ChaseOffset.Set(0.0f,0.0f,-2.0f);
    m_pChaseObjectTrans = NULL;
}// END FUNCTION icChaseCam(void)


/*! Updates the chase camera using a dampened mass-spring model
 *
 *  @param          fDeltaTime      Elapsed time since last update (seconds)
**/
void icChaseCam::Update(const float fDeltaTime)
{
    if (!m_pChaseObjectTrans)
        return;

    icVector3 chasePos = m_pChaseObjectTrans->GetPos();

    icVector3 chaseUp = m_pChaseObjectTrans->GetCol(1);

    icVector3 xformedOffset = m_pChaseObjectTrans->TransformVect(m_v3ChaseOffset);
    icVector3 xformedLook = m_pChaseObjectTrans->TransformVect(m_v3LookOffset);

    m_v3DesiredPos = chasePos + xformedOffset;

    // calc spring force
    icVector3 curPos = m_m44ViewMat.GetPos();
    icVector3 stretch = curPos - m_v3DesiredPos;
    icVector3 force = -(m_rStiffness * stretch) - (m_rDamping * m_v3Velocity);

    // acceleration
    icVector3 accel = force / m_rMass;
    m_v3Velocity += accel * fDeltaTime;

    // update position
    icVector3 calcPos = curPos + m_v3Velocity;
    calcPos += xformedLook;

    m_m44ViewMat.SetPos(calcPos);
    icCreateLookAt(calcPos, chasePos + xformedLook, chaseUp, m_m44ViewMat);
}// END FUNCTION Update(const float fDeltaTime)
