
#include "UI/icWidget.h"

#include "Content/icContentLoader.h"
#include "Core/IO/icInput.h"
#include "Render/VertBuffer/icVertBuffer.h"
#include "Core/GXDevice/icGXDevice.h"

icWidget::icWidget(icContentLoader* pContent)
{
    m_pContent = pContent;

    m_pNext = NULL;
    m_pVB = NULL;
    m_pTex1 = NULL;
    m_pTex2 = NULL;

    m_v2Pos.Set(0.0f,0.0f);
    m_v2Size.Set(0.0f,0.0f);
    m_Color.Set(0,0,0,0);
    m_HighColor.Set(0,0,0,0);
    m_fRotation = 0.0f;

    m_i32TabStop = 0;

    m_bHighlightOn = false;
    m_bUpdateBuffer = true;
    m_bHighlightEnabled = true;
    m_bMouseOver = false;
    Sticky = false;
}// END FUNCTION icWidget(icContentLoader* pContent)


/*! Processes input for the widget.
 *      Default processing includes mouse-over highlighting
 *
 *  @param      pInput          Pointer to current input object
 *  @returns    ICRESULT        Success/failure of widget input processing
**/
ICRESULT icWidget::ProcessInput(icInput* pInput)
{
    if (!pInput)
        return IC_FAIL_GEN;

    short x,y;
    pInput->GetPos(&x,&y);
    icVector2 mouse_pos((icReal)x,(icReal)y);

    // update the input state
    ic2DAABB box;
    box.extents.x = m_v2Size.x/2.0f;
    box.extents.y = m_v2Size.y/2.0f;
    box.pos = m_v2Pos;

    m_bMouseOver = ic2DPointAABB(mouse_pos,box);

    if (m_bHighlightEnabled && !Sticky)
        SetHighlight(m_bMouseOver);

    return IC_OK;
}// END FUNCTION ProcessInput( icInput* pInput )


/*! Updates the UI Widget
 *
 *  @param          fDeltaTime      Elapsed time since lase update
 *  @returns        ICRESULT        Success/failure of Updating the widget
**/
ICRESULT icWidget::Update(const icReal fDeltaTime)
{
    if (m_bUpdateBuffer)
        UpdateBuffer();

    return IC_OK;
}// END FUNCTION Update(const icReal fDeltaTime)


/*! Renders the UI Widget
 *
 *  @returns        ICRESULT        Success/failure of rendering the widget
**/
ICRESULT icWidget::Render(void)
{
    // make sure we have a device
    icGXDevice* pDev = m_pContent->GetDevice();    
    if (pDev)
    {
        if (m_pVB)
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

            // draw the widgets vertex buffer
            pDev->DrawVertBuf(m_pVB);

            pDev->EnableDepthTest(true);
            pDev->EnableAlphaBlend(false);
        }
        return IC_OK;
    }
    return IC_WARN_GEN;
}// END FUNCTION Render(void)


/*! Updates the widgets vertex buffer
**/
void icWidget::UpdateBuffer(void)
{
    if (!m_pContent)
        return;

    icGXDevice* pDev = m_pContent->GetDevice();
    if (!pDev)
        return;

    if (!m_pVB)
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
        
        pDev->CreateVB(&m_pVB, &vert_def);
    }
    
    // error!
    if (!m_pVB)
        return;

    // update the verts
    icVertLock vert_lock = {0};
    if (ICEOK(m_pVB->Lock(&vert_lock)))
    {
        icReal halfx = m_v2Size.x / 2.0f;
        icReal halfy = m_v2Size.y / 2.0f;

        const icColor& col = (m_bHighlightOn) ? m_HighColor : m_Color;
        icVector2* puv = (m_bHighlightOn) ? m_uvs[1] : m_uvs[0];

        if (m_pVB->GetVertDef()->vertType == IC_VERT_2DCOL)
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
        else if (m_pVB->GetVertDef()->vertType == IC_VERT_2DCOL_DIF)
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

        m_pVB->Unlock();
    }
    m_bUpdateBuffer = false;
}// END FUNCTION UpdateBuffer(void)
