#ifndef __IC_SLIDER_H__
#define __IC_SLIDER_H__

//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "icWidget.h"


/*! Provides a slider ui widget
**/
class icSlider : public icWidget
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icSlider(icContentLoader* pContent);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT ProcessInput(icInput* pInput);
    virtual ICRESULT Update(const icReal fDeltaTime);
    virtual ICRESULT Render(void);

    //////////////////////////////////////////////////////////////////////////
    // ACCESS
    int GetVal(void) {return m_val;};
    void SetVal(int newVal) {m_val=newVal;};
    void SetMinMax(int min, int max) {m_minval=min;m_maxval=max;};

    void SetSelected(bool bSelected) {m_bSelected = bSelected;};

    void SetSnapInterval(int interval) {m_snapInterval = interval;};

    void SetTickUVs(const icVector2& tl, const icVector2& tr,
                    const icVector2& bl, const icVector2& br);

    void SetTickHighUVs(const icVector2& tl, const icVector2& tr,
                        const icVector2& bl, const icVector2& br);

    void SetTickSize(const icVector2& size)
    {
        m_v2TickSize = size;
    };

protected:
    virtual void UpdateBuffer(void);

    enum MSTATE
    {
        MSTATE_WAITING=0,
        MSTATE_WAITING_OVER_BAR,
        MSTATE_FIRST_DOWN,
        MSTATE_FIRST_DRAG,
        MSTATE_FIRST_UP,
        MSTATE_NUM_STATES
    };


    int             m_snapInterval;
    int             m_val;
    int             m_minval;
    int             m_maxval;
    icVector2       m_click_pos;
    icVector2       m_v2TickSize;

    icVector2       m_tick_uvs[2][4];

    icReal          m_fpos;

    icVertBuffer*   m_pTickerVB;

    bool            m_bSelected;

    MSTATE          m_mstate;

};


/*! Sets the regular uvs for the slider ticker
**/
FORCE_INLINE
void icSlider::SetTickUVs(const icVector2& tl, const icVector2& tr,
                          const icVector2& bl, const icVector2& br)
{
    m_bUpdateBuffer = true;

    m_tick_uvs[0][0] = bl;
    m_tick_uvs[0][1] = tl;
    m_tick_uvs[0][2] = tr;
    m_tick_uvs[0][3] = br;
}// END FUNCTION SetTickUVs(const icVector2& tl, const icVector2& tr,
 //                         const icVector2& bl, const icVector2& br)


/*! Sets the highlight uvs for the slider ticker
**/
FORCE_INLINE
void icSlider::SetTickHighUVs(const icVector2& tl, const icVector2& tr,
                              const icVector2& bl, const icVector2& br)
{
    m_bUpdateBuffer = true;

    m_tick_uvs[1][0] = bl;
    m_tick_uvs[1][1] = tl;
    m_tick_uvs[1][2] = tr;
    m_tick_uvs[1][3] = br;
}// END FUNCTION SetTickHighUVs(const icVector2& tl, const icVector2& tr,
 //                             const icVector2& bl, const icVector2& br)
#endif //__IC_SLIDER_H__