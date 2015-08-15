
#include "UI/icButton.h"

#include "Core/IO/icInput.h"

/*! c'tor
**/
icButton::icButton(icContentLoader* pContent)
    : icWidget(pContent)
{
    m_pClicked = NULL;
    m_pHovered = NULL;

    m_pTexDown = NULL;

    m_fClickTimer = -1.0f;
    m_fHoverTimer = -1.0f;

    m_bDown = false;
    m_bLastDown = false;
}// END FUNCTION icButton(icContentLoader* pContent)


/*! Cleans up the ui button
 *
 *  @returns        ICRESULT        Success/failure of cleanup
**/
ICRESULT icButton::Cleanup(void)
{
    return IC_OK;
}// END FUNCTION Cleanup(void)


/*! Processes input for the Button.
 *
 *  @param      pInput          Pointer to current input object
 *  @returns    ICRESULT        Success/failure of widget input processing
**/
ICRESULT icButton::ProcessInput(icInput* pInput)
{
    if (!pInput)
        return IC_FAIL_GEN;

    
    if (m_bHighlightOn && !Sticky)
    {
        if (!m_bDown && m_bLastDown && m_pClicked)
            m_pClicked->Play();

        m_bDown = pInput->IsDown(ICMOUSE_L);
        m_bLastDown = m_bDown;
    }
    else if (Sticky && m_bMouseOver)
    {        
        bool temp = pInput->IsPressed(ICMOUSE_L);
        if (temp)
        {
            m_bLastDown = m_bDown;
            m_bDown = !m_bDown;
        }
    }

    //if (Sticky && !m_bDown && !m_bLastDown)
    //    m_bLastDown = true;
    
    return icWidget::ProcessInput(pInput);
}// END FUNCTION ProcessInput(icInput* pInput)


/*! Updates the UI Button
 *
 *  @param          fDeltaTime      Elapsed time since lase update
 *  @returns        ICRESULT        Success/failure of Updates the widget
**/
ICRESULT icButton::Update(const icReal fDeltaTime)
{
    if (m_fClickTimer > 0.0f)
        m_fClickTimer -= fDeltaTime;

    if (m_fHoverTimer > 0.0f)
        m_fHoverTimer -= fDeltaTime;

    if (Sticky) m_bHighlightOn = m_bDown;

    return icWidget::Update(fDeltaTime);
}// END FUNCTION Update(const icReal fDeltaTime)


/*! Renders the UI Button
 *
 *  @returns        ICRESULT        Success/failure of rendering the widget
**/
ICRESULT icButton::Render(void)
{
    // TODO, allow color buttons with text?
    //if (strlen(m_szText) > 0)
    //{
    //}
    return icWidget::Render();
}// END FUNCTION Render(void)


/*! Toggles the highlight state (overloaded from icWidget)
 *
 *  @param          bHighlightOn        true - set for highlight
**/
void icButton::SetHighlight(bool bHighlightOn)
{
    if (m_pHovered && m_fHoverTimer<0.0f)
    {
        // no hover sound for 0.5 seconds        
        if (!m_bHighlightOn && bHighlightOn)
        {
            // TODO: expose this parameter!
            m_fHoverTimer = 0.2f;
            m_pHovered->Play();
        }
        else if(m_bHighlightOn && !bHighlightOn)
        {
            m_fHoverTimer = 0.5f;
            m_pHovered->Play();
        }
    }

    icWidget::SetHighlight(bHighlightOn);
}// END FUNCTION SetHighlight(bool bHighlightOn)