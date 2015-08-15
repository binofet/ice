#ifndef __IC_BUTTON_H__
#define __IC_BUTTON_H__

#include "Audio/icSound.h"
#include "Core/IO/icInput.h"

#include "icWidget.h"

/*! Abstract interface for all interface objects
**/
class icButton : public icWidget
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icButton(icContentLoader* pContent);
    virtual ~icButton(void) {Cleanup();};

    virtual ICRESULT Cleanup(void);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT ProcessInput(icInput* pInput);
    virtual ICRESULT Update(const icReal fDeltaTime);
    virtual ICRESULT Render(void);

    virtual void SetHighlight(bool bHighlightOn);

    void SetSounds(icSoundI* pClicked, icSoundI* pHovered);

    void SetTextureDown(icTexture* pDown);

    void SetDown(bool bDown);
    bool IsDown(void) const;

    bool WasPressed(void) const;


protected:
    icSoundI*       m_pClicked;
    icSoundI*       m_pHovered;

    float           m_fClickTimer;
    float           m_fHoverTimer;

    icTexture*      m_pTexDown;

    char            m_szText[64];

    bool            m_bDown;
    bool            m_bLastDown;
};


FORCE_INLINE void icButton::SetDown(bool bDown)
{
    m_bLastDown = m_bDown;
    m_bDown = bDown;
}// END FUNCTION SetDown(bool bDown)


FORCE_INLINE bool icButton::IsDown(void) const
{
    if (!Sticky)
        return (m_bHighlightOn &&
            icInput::Instance()->IsDown(ICMOUSE_L));
    
    return m_bDown;
}// END FUNCTION IsDown(void) const


FORCE_INLINE bool icButton::WasPressed(void) const
{
    return (m_bHighlightOn &&
            icInput::Instance()->IsPressed(ICMOUSE_L));
    //return (!m_bDown && m_bLastDown);
}// END FUNCTION WasPressed(void) const


/*! Set the clicked and hovered sound pointers
 *
 *  @param          pClicked        Pointer to the clicked sound
 *  @param          pHovered        Pointer to the hovered sound
**/
FORCE_INLINE void icButton::SetSounds(icSoundI* pClicked, icSoundI* pHovered)
{
    m_pClicked = pClicked;
    m_pHovered = pHovered;
}// END FUNCTION SetSounds(icSoundI* pClicked, icSoundI* pHovered)

#endif //__IC_WIDGET_H__