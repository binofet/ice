
#include "Core/Application/icApplication.h"


/*! Default Constructor
**/
icApplication::icApplication( void )
{
    m_pGxDevice=NULL;
}// END FUNCTION icApplication( void )


/*! Default Destructor
**/
icApplication::~icApplication( void )
{
    Cleanup();
}// END FUNCTION icApplication( void )


/*! Frees application memory
 *
 *    @returns    ICRESULT    Result of Cleanup
 *
**/
ICRESULT icApplication::Cleanup( void )
{
    //if (m_pGxDevice)
    //{
    //    delete m_pGxDevice;
    //    m_pGxDevice = NULL;
    //}
    return IC_OK;
}// END FUNCTION Cleanup( void )


ICRESULT icApplication::Init(int argc, char* args[])
{
    m_iArgc = argc;
    m_pArgsz = args;
    return IC_OK;
}// END FUNCTION Init(int argc, const char** args)


/*! Updates the application
 *
 *    @param[in]    fDeltaTime    Elapsed time since last update
 *    @returns    ICRESULT    Result of update
 *
**/
ICRESULT icApplication::Update(void)
{
    if (m_pGxDevice)
    {
        m_pGxDevice->Update();
    }
    return IC_OK;
}// END FUNCTION Update( const float fDeltaTime )


/*! Renders the application
 *
 *    @returns    ICRESULT    Result of render
 *
**/
ICRESULT icApplication::Render( void )
{
    if (m_pGxDevice)
    {
        m_pGxDevice->FrameBegin();

        m_pGxDevice->FrameEnd();
    }
    return IC_OK;
}// END FUNCTION Render( void )


/*! TODO: remove this or make it useful
 *
 *    @returns    ICRESULT    Returns IC_OK;
 *
**/
ICRESULT icApplication::Quit( void )
{
    return IC_OK;
}// END FUNCTION Quit( void )


//void icApplication::OnChangeSize(const icWindow* const pWin,
//                                 ushort sizex, ushort sizey)
//{
//    if (m_pGxDevice)
//    {
//        m_pGxDevice->
//    }
//}