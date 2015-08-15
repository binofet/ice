#ifndef __IC_WIDGET_H__
#define __IC_WIDGET_H__

#include "Core/icGlobals.h"
#include "Math/Vector/icVector2.h"
#include "Math/icColor.h"
#include "Math/ic2DCollision.h"

class icVertBuffer;
class icTexture;
class icInput;
class icContentLoader;

/*! Abstract interface for all interface objects
**/
class icWidget
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icWidget(icContentLoader* pContent);
    virtual ~icWidget(void) {Cleanup();};

    virtual ICRESULT Cleanup(void) {m_pNext = NULL;return IC_OK;};

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT ProcessInput(icInput* pInput);
    virtual ICRESULT Update(const icReal fDeltaTime);
    virtual ICRESULT Render(void);

    //////////////////////////////////////////////////////////////////////////
    // ACCESS
    void SetTextures(icTexture* tex1, icTexture* tex2 = NULL);
    void GetTextures(icTexture** pTex1, icTexture** pTex2 = NULL);

    void SetUVs(const icVector2& tl, const icVector2& tr,
                const icVector2& bl, const icVector2& br);

    void SetHighUVs(const icVector2& tl, const icVector2& tr,
                    const icVector2& bl, const icVector2& br);

    virtual void SetHighlight(bool bHighlightOn);
    bool IsHighlight(void);

    void EnableHighlight(bool bEnable)
    {
        m_bHighlightEnabled = bEnable;
    }

    //bool IsPointerOver(void);

    const icVector2& GetPos(void);
    const icVector2& GetSize(void);

    const icColor& GetColor(void);
    const icColor& GetHighColor(void);

    virtual void SetPos(const icVector2& pos);
    virtual void SetSize(const icVector2& size);

    virtual void SetColor(const icColor& col);
    virtual void SetHighColor(const icColor& col);

    virtual void UpdateBuffer(void);

    bool            Sticky;
protected:
    

    icWidget*           m_pNext;

    icContentLoader*    m_pContent;

    icVertBuffer*       m_pVB;
    icTexture*          m_pTex1;
    icTexture*          m_pTex2;

    icVector2           m_v2Pos;
    icVector2           m_v2Size;
    icVector2           m_uvs[2][4];
    icColor             m_Color;
    icColor             m_HighColor;
    icReal              m_fRotation;

    int                 m_i32TabStop;

    bool                m_bHighlightOn;
    bool                m_bUpdateBuffer;
    bool                m_bHighlightEnabled;
    bool                m_bMouseOver;

    friend class icFrame;
};

FORCE_INLINE void icWidget::SetTextures(icTexture* tex1, icTexture* tex2)
{
    m_pTex1 = tex1;
    m_pTex2 = tex2;
}// END FUNCTION SetTextures(icTexture* tex1, icTexture* tex2)


FORCE_INLINE void icWidget::GetTextures(icTexture** pTex1, icTexture** pTex2)
{
    if (pTex1)
        *pTex1 = m_pTex1;

    if (pTex2)
        *pTex2 = m_pTex2;
}// END FUNCTION GetTextures(icTexture** pTex1, icTexture** pTex2)

FORCE_INLINE void icWidget::SetUVs(const icVector2& tl, const icVector2& tr,
                                   const icVector2& bl, const icVector2& br)

{
    m_bUpdateBuffer = true;

    m_uvs[0][0] = bl;
    m_uvs[0][1] = tl;
    m_uvs[0][2] = tr;
    m_uvs[0][3] = br;
}// END FUNCTION SetUVs(const icVector2& tl, const icVector2& tr,
 //                     const icVector2& bl, const icVector2& br)

FORCE_INLINE 
void icWidget::SetHighUVs(const icVector2& tl, const icVector2& tr,
                          const icVector2& bl, const icVector2& br)

{
    m_bUpdateBuffer = true;

    m_uvs[1][0] = bl;
    m_uvs[1][1] = tl;
    m_uvs[1][2] = tr;
    m_uvs[1][3] = br;
}// END FUNCTION SetHighUVs(const icVector2& tl, const icVector2& tr,
 //                         const icVector2& bl, const icVector2& br)

FORCE_INLINE void icWidget::SetHighlight(bool bHighlightOn)
{
    m_bHighlightOn = bHighlightOn;
}// END FUNCTION SetHighlight(bool bHighlightOn)

FORCE_INLINE bool icWidget::IsHighlight(void)
{
    return m_bHighlightOn;
}// END FUNCTION IsHighlight(void)

FORCE_INLINE const icVector2& icWidget::GetPos(void) 
{
    return m_v2Pos;
}// END FUNCTION GetPos(void)

FORCE_INLINE const icVector2& icWidget::GetSize(void) 
{
    return m_v2Size;
}// END FUNCTION GetSize(void)

FORCE_INLINE const icColor& icWidget::GetColor(void) 
{
    return m_Color;
}// END FUNCTION GetColor(void)

FORCE_INLINE const icColor& icWidget::GetHighColor(void) 
{
    m_bUpdateBuffer = true;

    return m_HighColor;
}// END FUNCTION GetHighColor(void)

FORCE_INLINE void icWidget::SetPos(const icVector2& pos)
{
    m_v2Pos = pos;
}// END FUNCTION SetPos(const icVector2& pos)

FORCE_INLINE void icWidget::SetSize(const icVector2& size)
{
    m_bUpdateBuffer = true;
    m_v2Size = size;
}// END FUNCTION SetSize(const icVector2& size)

FORCE_INLINE void icWidget::SetColor(const icColor& col)
{
    m_bUpdateBuffer = true;
    m_Color = col;
}// END FUNCTION SetColor(const icColor& col)

FORCE_INLINE void icWidget::SetHighColor(const icColor& col)
{
    m_bUpdateBuffer = true;
    m_HighColor = col;
}// END FUNCTION SetHighColor(const icColor& col)

#endif //__IC_WIDGET_H__