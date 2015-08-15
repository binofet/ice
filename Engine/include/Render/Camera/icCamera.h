#ifndef __IC_CAMERA_H__
#define __IC_CAMERA_H__

#include "Math/Matrix/icMatrix44.h"
#include "Math/Vector/icVector2.h"
#include "Math/Vector/icVector2Int.h"



/*! Provides a basic camera object
**/
class icCamera
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icCamera( void ){};
    virtual ~icCamera( void ){};

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual void Update( const float fDeltaTime ){fDeltaTime;};


    //////////////////////////////////////////////////////////////////////////
    // ACCESS
    icMatrix44& GetProjection( void );
    icMatrix44& GetViewMat( void );

    const icMatrix44& GetProjection( void ) const;
    const icMatrix44& GetViewMat( void ) const;

    virtual void SetCameraPos( const icVector3& pos );

    virtual const icVector3& GetCameraPos( void ) const;

    icVector2 WorldToScreen(const icVector3& world, int width, int height);

    icVector3 ScreenToWorldDir(const icVector2& v2Pos,
                               const icVector2Int& v2Size) const;

protected:
    //////////////////////////////////////////////////////////////////////////
    // PROTECTED MEMBERS
    icMatrix44    m_m44Projection;
    icMatrix44    m_m44ViewMat;
};


/*! Returns the projection matrix
 *
 *    @returns    icMatrix44&        Reference to projection
**/
FORCE_INLINE icMatrix44& icCamera::GetProjection( void )
{
    return m_m44Projection;
}// END FUNCTION GetProjection( void )


/*! Returns the view matrix
 *
 *    @returns    icTransform&    Reference to view matrix
**/
FORCE_INLINE icMatrix44& icCamera::GetViewMat( void )
{
    return m_m44ViewMat;
}// END FUNCTION GetViewMat( void )


/*! Returns the projection matrix
 *
 *    @returns    icMatrix44&        const reference to projection
**/
FORCE_INLINE const icMatrix44& icCamera::GetProjection( void ) const
{
    return m_m44Projection;
}// END FUNCTION GetProjection( void ) const


/*! Returns the view matrix
 *
 *    @returns    icTransform&        const reference to projection
 *
**/
FORCE_INLINE const icMatrix44& icCamera::GetViewMat( void ) const
{
    return m_m44ViewMat;
}// END FUNCTION GetViewMat( void ) const


/*! Sets the camera position
 *
 *    @param        pos        The camera position in world space
**/
FORCE_INLINE void icCamera::SetCameraPos( const icVector3& pos )
{
    m_m44ViewMat.SetPos( pos );
}// END FUNCTION SetCameraPos( const icVector3& pos )


/*! Gets the current camera position
 *
 *    @returns      const icVector3&    Current camera position
**/
FORCE_INLINE const icVector3& icCamera::GetCameraPos( void ) const
{
    return m_m44ViewMat.GetPos();
}// END FUNCTION GetCameraPos( void ) const


#endif //__IC_CAMERA_H__

