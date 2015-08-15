
#include "Math/Vector/icVector3.h"

icVector3 icVector3::ZERO(icReal(0),icReal(0),icReal(0));
icVector3 icVector3::X_AXIS(icReal(1),icReal(0),icReal(0));
icVector3 icVector3::Y_AXIS(icReal(0),icReal(1),icReal(0));
icVector3 icVector3::Z_AXIS(icReal(0),icReal(0),icReal(1));



/*! Makes a new vector, the cross product of two given vectors
 *
 *    @param[in]    v1        Vector crossing into v2
 *    @param[in]    v2        Vector crossed into by v1
 *    @returns
 *
**/
icVector3 Cross(const icVector3& v1,
                             const icVector3& v2)
{
    icVector3 v;
    v.x = v2.z*v1.y - v1.z*v2.y;
    v.y = v2.x*v1.z - v1.x*v2.z;
    v.z = v2.y*v1.x - v1.y*v2.x;
    return v;
}// END FUNCTINO Cross( const icVector3& v1, const icVector3& v2 )

