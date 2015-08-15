
#include "UI/icSlider.h"

#include "Core/IO/icInput.h"
#include "Core/GXDevice/icGXDevice.h"
#include "Content/icContentLoader.h"
#include "Render/VertBuffer/icVertBuffer.h"


/*! c'tor
**/
icSlider::icSlider(icContentLoader* pContent)
    : icWidget(pContent)
{
    m_mstate = MSTATE_WAITING;

    m_v2Size.Set(238.0f,43.0f);
    m_v2Pos.Set(0.0f,0.0);

    m_val = 0;
    m_minval = 0;
    m_maxval = 100;

    m_fpos = 0.0f;

    m_bSelected = false;

    m_pTickerVB = NULL;
}// END FUNCTION icSlider(icContentLoader* pContent)


/*! Input process function
**/
ICRESULT icSlider::ProcessInput(icInput* pInput)
{
    if (!pInput)
        return IC_FAIL_GEN;

    bool ldown = pInput->IsDown(ICMOUSE_L);

    short x,y;
    pInput->GetPos(&x,&y);
    icVector2 mouse_pos((icReal)x,(icReal)y);

    // update the input state
    ic2DAABB box;
    box.extents.x = m_v2Size.x/2.0f;
    m_snapInterval = (m_snapInterval > 1) ? m_snapInterval : 1;
    box.extents.x += ((icReal)m_snapInterval+1.0f)/((icReal)m_maxval+1.0f);
    box.extents.y = m_v2TickSize.y/2.0f;
    box.pos = m_v2Pos;

    bool over_bar = ic2DPointAABB(mouse_pos,box);

    if (ldown)
    {
        if (m_mstate == MSTATE_FIRST_DOWN  && over_bar)
            m_mstate = MSTATE_FIRST_DRAG;
        if (m_mstate == MSTATE_WAITING_OVER_BAR  && over_bar)
            m_mstate = MSTATE_FIRST_DOWN;
    }
    else
    {
        if (m_mstate == MSTATE_FIRST_UP ||
            (m_mstate == MSTATE_WAITING_OVER_BAR && !over_bar))
            m_mstate = MSTATE_WAITING;
        else if (m_mstate == MSTATE_WAITING && !ldown && over_bar)
            m_mstate = MSTATE_WAITING_OVER_BAR;
        else if (m_mstate == MSTATE_FIRST_DRAG)
            m_mstate = MSTATE_FIRST_UP;
    }

    // Update the values
    if (m_mstate == MSTATE_FIRST_DOWN)
    {
        m_fpos = (mouse_pos.x + m_v2Size.x/2.0f - m_v2Pos.x)/m_v2Size.x * (icReal)(m_maxval);
        m_val = (int)floor(m_fpos+0.5f);
        m_click_pos = mouse_pos;
    }
    else if (m_mstate == MSTATE_FIRST_DRAG)
    {
        icReal dif = mouse_pos.x - m_click_pos.x;
        m_fpos += ((icReal)dif/m_v2Size.x)*(icReal)(m_maxval-m_minval);
        
        m_val = (int)floor(m_fpos + 0.5f) + m_minval;
        m_click_pos = mouse_pos;
    }
    else if (m_bSelected && pInput->IsPressed(ICKEY_RIGHT))
        m_val += m_snapInterval;
    else if (m_bSelected && pInput->IsPressed(ICKEY_LEFT))
        m_val -= m_snapInterval;

    m_val = (m_val > m_maxval) ? m_maxval : m_val;
    m_val = (m_val < m_minval) ? m_minval : m_val;

    return icWidget::ProcessInput(pInput);
}// END FUNCTION ProcessInput(icInput* pInput)


/*! Updates the icSlider
 *
 *      if the icSlider has focus, the user can interat with the keyboard
 *      otherwise the mouse drives the icSlider
 *
 *  @param      fDeltaTime      Elapsed time since last update
**/
ICRESULT icSlider::Update(const icReal fDeltaTime)
{
    return icWidget::Update(fDeltaTime);
}// END FUNCTION Update(const float fDeltaTime)


/*! Renders the icSlider
**/
ICRESULT icSlider::Render(void)
{
    ICRESULT res = IC_OK;

    SetPos(m_v2Pos);
    
    // make sure we have a device
    icGXDevice* pDev = m_pContent->GetDevice();    
    if (pDev)
    {
        if (m_pVB && m_pTickerVB)
        {
            // handle textures and highlighting
            if (m_bHighlightOn && m_pTex2)
                pDev->SetTexture(0,m_pTex2);
            else if (m_pTex1)
                pDev->SetTexture(0,m_pTex1);


            icMatrix44 temp;
            temp.MakeTranslation(m_v2Pos.x,m_v2Pos.y,0.0f);
            pDev->SetWorldTransform(temp);

            pDev->EnableDepthTest(false);
            pDev->EnableAlphaBlend(true);

            // draw the widgets vertex buffers
            pDev->DrawVertBuf(m_pVB);

            icReal tempx = (m_v2Size.x)/(icReal)(m_maxval-m_minval);
            temp.MakeTranslation(m_v2Pos.x - m_v2Size.x/2.0f + (icReal)(m_val-m_minval)*tempx,
                                 m_v2Pos.y,
                                 0.0f);
            pDev->SetWorldTransform(temp);
            pDev->DrawVertBuf(m_pTickerVB);

            pDev->EnableDepthTest(true);
            pDev->EnableAlphaBlend(false);
        }
        return res;
    }
    return IC_WARN_GEN;
}// END FUNCTION Render(void)


/*! Called when the vertex buffers need to be updated
**/
void icSlider::UpdateBuffer(void)
{
    if (!m_pContent)
        return;

    icGXDevice* pDev = m_pContent->GetDevice();
    if (!pDev)
        return;

    // make sure we have the ticker vertex buffer
    if (!m_pTickerVB)
    {
        icVertDef vert_def;
        vert_def.numPrims = 2;
        vert_def.primType = IC_PRIM_TLIST;
        vert_def.uiNumVerts = 6;
        if (m_pTex1 || m_pTex2)
        {
            vert_def.uiVertSize = sizeof(ICVRT2DCOL_DIF);
            vert_def.vertType = IC_VERT_2DCOL_DIF;
        }
        else
        {
            vert_def.uiVertSize = sizeof(ICVRT2DCOL);
            vert_def.vertType = IC_VERT_2DCOL;
        }
        vert_def.usage = IC_VERT_USAGE_STATIC;
        
        pDev->CreateVB(&m_pTickerVB, &vert_def);
    }

    // error!
    if (!m_pTickerVB)
        return;

    icVertLock vert_lock;
    if (ICEOK(m_pTickerVB->Lock(&vert_lock)))
    {
        icReal halfx = m_v2TickSize.x / 2.0f;
        icReal halfy = m_v2TickSize.y / 2.0f;

        const icColor& col = (m_bHighlightOn) ? m_HighColor : m_Color;
        icVector2* puv = (m_bHighlightOn) ? m_tick_uvs[1] : m_tick_uvs[0];

        if (m_pTickerVB->GetVertDef()->vertType == IC_VERT_2DCOL)
        {
            ICVRT2DCOL* vb = (ICVRT2DCOL*)vert_lock.pData;

            vb[0].pos.Set(-halfx,-halfy);
            vb[0].col = col;

            vb[1].pos.Set(-halfx,halfy);
            vb[1].col = col;

            vb[2].pos.Set(halfx,halfy);
            vb[2].col = col;

            vb[3].pos.Set(halfx,halfy);
            vb[3].col = col;

            vb[4].pos.Set(halfx,-halfy);
            vb[4].col = col;

            vb[5].pos.Set(-halfx,-halfy);
            vb[5].col = col;
        }
        else if (m_pTickerVB->GetVertDef()->vertType == IC_VERT_2DCOL_DIF)
        {
            ICVRT2DCOL_DIF* vb = (ICVRT2DCOL_DIF*)vert_lock.pData;

            vb[0].pos.Set(-halfx,-halfy);
            vb[0].col = col;
            vb[0].uv = puv[0];

            vb[1].pos.Set(-halfx,halfy);
            vb[1].col = col;
            vb[1].uv = puv[1];

            vb[2].pos.Set(halfx,halfy);
            vb[2].col = col;
            vb[2].uv = puv[2];

            vb[3].pos.Set(halfx,halfy);
            vb[3].col = col;
            vb[3].uv = puv[2];

            vb[4].pos.Set(halfx,-halfy);
            vb[4].col = col;
            vb[4].uv = puv[3];

            vb[5].pos.Set(-halfx,-halfy);
            vb[5].col = col;
            vb[5].uv = puv[0];
        }

        m_pTickerVB->Unlock();
    }

    icWidget::UpdateBuffer();
}// END FUNCTION UpdateBuffer(void)