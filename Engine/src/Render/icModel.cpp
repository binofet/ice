
#include "Render/icModel.h"

#include "Render/VertBuffer/icVertBuffer.h"
#include "Render/IndexBuffer/icIndexBuf.h"
#include "Render/Texture/icTexture.h"

#include "Core/GXDevice/icGXDevice.h"


icModel::icModel(void)
{
    m_pVB = NULL;
    m_pIB = NULL;
    m_difTex = NULL;
    m_normTex = NULL;
    m_specTex = NULL;

    m_pDevice = NULL;
}

icModel::~icModel(void)
{
}

ICRESULT icModel::Render(const icMatrix44& trans)
{
    if (m_pDevice)
    {
        ICRESULT res = IC_OK;

        m_pDevice->SetWorldTransform(trans);

        //m_pDevice->SetCullMode(IC_CULL_ANTICLOCK);

        if (m_difTex)
            res |= m_pDevice->SetTexture(0, m_difTex);

        //if (m_normTex)
        //    res |= m_pDevice->SetTexture(1, m_normTex);

        //if (m_specTex)
        //    res |= m_pDevice->SetTexture(2, m_specTex);

        if (m_pIB && m_pVB)
        {
            res |= m_pDevice->DrawIndexedVertBuf(m_pVB, m_pIB);
        }
        else if (m_pVB)
        {
            res |= m_pDevice->DrawVertBuf(m_pVB);
        }

        //m_pDevice->SetCullMode(IC_CULL_CLOCKWISE);

        m_pDevice->SetTexture(0, NULL);
        return res;
    }
    return IC_FAIL_NO_DEVICE;
}
