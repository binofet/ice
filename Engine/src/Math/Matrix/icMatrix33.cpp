
#include "Math/Matrix/icMatrix33.h"

icReal _ident33[] = {1.0f,0.0f,0.0f,
                     0.0f,1.0f,0.0f,
                     0.0f,0.0f,1.0f};

const icMatrix33 icMatrix33::IDENTITY(_ident33);

/*  3x3 * 3x1
 *
 *    @param        Rhs             Vector
 *    @returns      icVector3       3x3 * 3x1 == 3x1
 *
**/
icVector3 icMatrix33::operator*( const icVector3& Rhs ) const
{
    icVector3 v;
    v.x = m_el[0]*Rhs.x+m_el[3]*Rhs.y+m_el[6]*Rhs.z;
    v.y = m_el[1]*Rhs.x+m_el[4]*Rhs.y+m_el[7]*Rhs.z;
    v.z = m_el[2]*Rhs.x+m_el[5]*Rhs.y+m_el[8]*Rhs.z;
    return v;
}// END FUNCTION operator*( const icVector3& Rhs ) const