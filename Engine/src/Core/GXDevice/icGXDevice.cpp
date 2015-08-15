
#include "Core/GXDevice/icGXDevice.h"


/*! Default c'tor
**/
icGXDevice::icGXDevice(void)
{
    m_pMainWindow = NULL;

    m_iHeight = 0;
    m_iWidth = 0;
    m_ui64CurFrame = 0;

    m_UElapsed = 0.0;
    m_RElapsed = 0.0;

    m_UFPS = 0.0;
    m_UhighFPS = 0.0;
    m_UlowFPS = 10000000.0;

    m_RFPS = 0.0;
    m_RhighFPS = 0.0;
    m_RlowFPS = 100000000.0;

    m_RunCounter = 0.0;
    m_RunTime = 0.0;

    // Initialize the timers
    m_UTimer.Elapsed();
    m_RTimer.Elapsed();

    m_bDepthTest = true;
    m_bFogEnable = false;
}// END FUNCTION icGXDevice(void)


/*! Updates frame information
 *
 *      This should be called by objects implenting the icGXDevice
 *      interface
 *
 *  @returns    ICRESULT    Always returns IC_OK
**/
ICRESULT icGXDevice::Update(void)
{
    m_UFPS = icReal(1)/m_UElapsed;
    m_UElapsed = m_UTimer.Elapsed();
    return IC_OK;
}// END FUNTION Update(void)


/*! Ends the current frame
 *
 *      This should be called by objects implenting the icGXDevice
 *      interface
 *
**/
void icGXDevice::FrameEnd(bool bSwap)
{
    bSwap;
    m_RElapsed = m_RTimer.Elapsed();
    //if (m_RunCounter>=0.0)
    //{
        m_RunCounter = 0.0;
        m_RFPS = icReal(1.0)/m_RElapsed;
    //}
    m_RunTime+=m_RElapsed;
    m_RunCounter+=m_RElapsed;
    // TODO: implement 'high' and 'low' frame values
}// END FUNCTION FrameEnd(bool bSwap)