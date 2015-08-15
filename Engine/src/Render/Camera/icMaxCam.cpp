
#include "Render/Camera/icMaxCam.h"

#include "Core/IO/icInput.h"

#include "Math/Matrix/icMatrix44.h"

icMaxCam::icMaxCam( void )
{
    m_rZoom = 100.0f;
    m_rXrot = 0.0f;
    m_rYrot = 0.0f;

    m_rZoomMin = 50.0f;
    m_rZoomMax = 500.0f;

    m_v3Center.Set(0.0f,0.0f,0.0f);

    bEnableInput = true;
}

icMaxCam::~icMaxCam( void )
{
}

void icMaxCam::Update( const float fDeltaTime )
{
    if (bEnableInput)
    {
        icInput* input = icInput::Instance();

        if (input->IsDown(ICMOUSE_M) || input->IsDown(ICMOUSE_R))
        {
            if (input->IsDown(ICKEY_LALT))
            {
                // arc rotate
                icReal x = input->GetAxis(ICAXIS_MOUSE_X) * fDeltaTime * 5000.0f * m_rZoom/m_rZoomMax;
                icReal y = input->GetAxis(ICAXIS_MOUSE_Y) * fDeltaTime * 5000.0f * m_rZoom/m_rZoomMax;

                if (m_rYrot > 0)
                    m_rYrot += y;
                else
                    m_rYrot -= y;

                m_rXrot -= x;
            }
            else
            {
                // pan
                icReal z_amt = (m_rZoom - m_rZoomMin) + 0.05f * (m_rZoomMax - m_rZoomMin);
                icReal x = input->GetAxis(ICAXIS_MOUSE_X) * fDeltaTime * 200.0f * z_amt;
                icReal y = input->GetAxis(ICAXIS_MOUSE_Y) * fDeltaTime * 200.0f * z_amt;

                m_v3Center += m_m44ViewMat.Transform(-x,-y,0.0f);
            }
        }

        // handle zoom
		icReal zoom = input->GetAxis(ICAXIS_MOUSE_Z);
        m_rZoom -= zoom * fDeltaTime * 1000.0f;
        m_rZoom = icClamp(m_rZoom, m_rZoomMin, m_rZoomMax);
    }


    // trimming
    while (m_rYrot > IC_PI * 2.0f)
        m_rYrot -= 2 * IC_PI;
    while (m_rYrot < 0.0f)
        m_rYrot += 2 * IC_PI;

    while (m_rXrot > IC_PI * 2.0f)
        m_rXrot -= 2 * IC_PI;
    while (m_rXrot < 0.0f)
        m_rXrot += 2 * IC_PI;

    icReal theta = m_rXrot;
    icReal phi = m_rYrot - IC_PI;
    icReal Cx, Cy, Cz;  
    Cx = m_rZoom * icCos(theta) * icSin(phi);  
    Cy = m_rZoom * icCos(phi);  
    Cz = m_rZoom * icSin(theta) * icSin(phi);  
    icVector3 point(Cx,Cy,Cz);
    icVector3 position  = point + m_v3Center;

    if (phi < 0.0)
        icCreateLookAt(position, m_v3Center,icVector3(0.0f,-1.0,0.0f),m_m44ViewMat);
    else
        icCreateLookAt(position, m_v3Center,icVector3::Y_AXIS,m_m44ViewMat);
}