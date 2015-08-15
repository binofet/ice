
#include "UI/icFrame.h"


/*! c'tor
**/
icFrame::icFrame(icContentLoader* pContent)
    : icWidget(pContent)
{
    m_pHead = NULL;
    m_pSelected = NULL;
}// END FUNCTION icFrame(icContentLoader* pContent)


/*! Cleans up the ui frame
 *
 *  @returns        ICRESULT        Success/failure of cleanup
**/
ICRESULT icFrame::Cleanup(void)
{
    return IC_OK;
}// END FUNCTION Cleanup(void)


/*! Frame processes input, and then all children
 *
 *  @returns        ICRESULT        Success/failure of update operation
**/
ICRESULT icFrame::ProcessInput(icInput* pInput)
{
    ICRESULT res = icWidget::ProcessInput(pInput);

    if (!m_pHead)
        return res;

    icWidget* cur = m_pHead;
    while (cur)
    {
        res |= cur->ProcessInput(pInput);
        cur = cur->m_pNext;
    }
    return res;
}// END FUNCTION ProcessInput(icInput* pInput)


/*! Updates the frame, and then all children
 *
 *  @returns        ICRESULT        Success/failure of update operation
**/
ICRESULT icFrame::Update(const icReal fDeltaTime)
{
    ICRESULT res = icWidget::Update(fDeltaTime);

    if (!m_pHead)
        return res;

    icWidget* cur = m_pHead;
    while (cur)
    {
        res |= cur->Update(fDeltaTime);
        cur = cur->m_pNext;
    }
    return res;
}// END FUNCTION Update(const icReal fDeltaTime)


/*! Renders the frame, and then all children
 *
 *  @returns        ICRESULT        Success/failure of render operation
**/
ICRESULT icFrame::Render(void)
{
    // TODO: figure out this business
    ICRESULT res = IC_OK;//icWidget::Render();

    if (!m_pHead)
        return res;

    icWidget* cur = m_pHead;
    while (cur)
    {
        res |= cur->Render();
        cur = cur->m_pNext;
    }
    return res;
}// END FUNCTION Render(void)


/*! Add a child widget to the frame
 *
 *  @param          pObj        Pointer to widget
 *  @param          relative    Use relative positioning?
 *  @returns        ICRESULT    Success/failure
**/
ICRESULT icFrame::AddChild(icWidget* pObj, bool relative)
{relative; // (warning removal) TODO: allow relative positioning
    if (!m_pHead)
    {
        m_pHead = pObj;
        m_pSelected = m_pHead;
        //m_pSelected->icWidget::SetHighlight(true);
        return IC_OK;
    }

    icWidget* cur = m_pHead->m_pNext;
    icWidget* prev = m_pHead;

    while (cur)
    {
        prev = cur;
        cur = cur->m_pNext;
    }
    prev->m_pNext = pObj;

    return IC_OK;
}// END FUNCTION AddChild(icWidget* pObj, bool relative)


/*! Find a child widget
 *
 *  @param          pObj        Pointer to widget
 *  @returns        ICRESULT    Found(IC_OK)/not found(IC_FAIL_GEN)
**/
ICRESULT icFrame::FindChild(icWidget* pObj)
{
    if (!pObj || !m_pHead)
        return IC_FAIL_GEN;

    icWidget* cur = m_pHead;
    while (cur)
    {
        if (cur == pObj)
            return IC_OK;

        cur = cur->m_pNext;
    }

    return IC_FAIL_GEN;
}// END FUNCTION FindChild(icWidget* pObj)


/*! Removes a child widget
 *
 *  @param          pObj        Pointer to widget
 *  @returns        ICRESULT    Removed(IC_OK)/not found(IC_FAIL_GEN)
**/
ICRESULT icFrame::RemoveChild(icWidget* pObj)
{
    if (!pObj || !m_pHead)
        return IC_FAIL_GEN;

    if (pObj == m_pHead)
    {
        icWidget* temp = m_pHead->m_pNext;
        m_pHead->m_pNext = NULL;
        m_pHead = temp;
        return IC_OK;
    }

    icWidget* cur = m_pHead->m_pNext;
    icWidget* prev = m_pHead;

    while (cur)
    {
        if (cur == pObj)
        {
            prev->m_pNext = cur->m_pNext;
            cur->m_pNext = NULL;
            return IC_OK;
        }
        prev = cur;
        cur = cur->m_pNext;
    }

    return IC_FAIL_GEN;
}// END FUNCTION RemoveChild(icWidget* pObj)