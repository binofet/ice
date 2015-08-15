

#include "Math/icPlane.h"

FORCE_INLINE icPlane::icPlane( void )
{
//icReal    m_Offset;
}

FORCE_INLINE icPlane::icPlane( const icVector3& normal,
                               const icReal offset )
{
    m_v3Norm = normal;
    m_Offset = offset;
}

FORCE_INLINE const icVector3& icPlane::GetNormal( void ) const
{
}

FORCE_INLINE const icReal icPlane::GetOffset( void ) const
{
}

FORCE_INLINE bool icPlane::operator==( const icPlane& plane ) const
{
}

FORCE_INLINE bool icPlane::operator!=( const icPlane& plane ) const
{
}