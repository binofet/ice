

#include "Math/Vector/icVector3.h"

#include "Math/Vector/icVector4.h"


/*! Default Constructor
**/
FORCE_INLINE icVector3::icVector3(void)
{
}// END FUNCTION icVector3( void )


/*! takes the x,y,z components of a vec4
 *
 *  @param          Rhs         Vector4 to take x,y,z from
**/
FORCE_INLINE icVector3::icVector3( const icVector4& Rhs )
{
   x = Rhs.x;
   y = Rhs.y;
   z = Rhs.z;
}// END FUNCTION icVector3( const icVector4& Rhs )


/*! Overloaded Constructor, initializes values
**/
FORCE_INLINE icVector3::icVector3(const icReal xIn,
                                  const icReal yIn,
                                  const icReal zIn)
{
    x = xIn;
    y = yIn;
    z = zIn;
}// END FUNCTION icVector3 ( icReal,icReal,icReal )


/*! Function to set all 3 values in 1 call
 *
 *    @param[in]    xIn        X value
 *    @param[in]    yIn        Y value
 *    @param[in]    zIn        Z value
 *
**/
FORCE_INLINE void icVector3::Set(const icReal xIn,
                                 const icReal yIn,
                                 const icReal zIn)
{
    x = xIn;
    y = yIn;
    z = zIn;
}// END FUNCTION Set


/*! Makes this vector the cross product of two given vectors
 *
 *    v1 is crossed into v2
 *    v1 and v2 cannot == this
 *
 *    @param[in]    v1        Vector crossing into v2
 *    @param[in]    v2        Vector crossed into by v1
 *
**/
FORCE_INLINE void icVector3::MakeCross(const icVector3& v1,
                                       const icVector3& v2)
{
    x = v2.z*v1.y - v1.z*v2.y;
    y = v2.x*v1.z - v1.x*v2.z;
    z = v2.y*v1.x - v1.y*v2.x;
}// END FUNCTION MakeCross( const icVector3& v1, icVector3& v2 )


/*! Computes the dot product of two vectors
 *
 *    @param[in]    v1                Vector1
 *    @param[in]    v2                Vector2
 *    @returns    const icReal    Dot product of these vectors
 *
**/
FORCE_INLINE const icReal Dot(const icVector3& v1, const icVector3& v2)
{
    return (v1.x * v2.x + v1.y * v2.y + v1.z * v2.z);
}// END FUNCTION Dot( const icVector3& v1, const icVector3& v2 )


/*! Returns the distance between this and another vector
 *
 *    @param[in]    Rhs                Another vector
 *    @returns    const icReal    Distance between these vectors
 *
**/
FORCE_INLINE const icReal icVector3::Distance(const icVector3& Rhs) const
{
    return icSqrt(    (x-Rhs.x)*(x-Rhs.x) +
                    (y-Rhs.y)*(y-Rhs.y) +
                    (z-Rhs.z)*(z-Rhs.z) );
}// END FUNCTION Distance( const icVector3& Rhs ) const


/*! Returns the squared distance between this and another vector
 *
 *    @param[in]    Rhs                Another vector
 *    @returns    const icReal    Squared distance between these vectors
 *
**/
FORCE_INLINE const icReal
icVector3::DistanceSqr(const icVector3& Rhs) const
{
    return ((x-Rhs.x)*(x-Rhs.x) +
            (y-Rhs.y)*(y-Rhs.y) +
            (z-Rhs.z)*(z-Rhs.z));
}// END FUNCTION DistanceSqr( const icVector3& Rhs ) const


/*! Computes the dot product of two vectors
 *
 *    @param[in]    Rhs                Vector to use in dot calc
 *    @returns    const icReal    Dot product of these vectors
 *
**/
FORCE_INLINE const icReal icVector3::Dot(const icVector3& Rhs) const
{
    return ( x*Rhs.x + y*Rhs.y + z*Rhs.z );
}// END FUNCTION Dot( const icVector3& Rhs ) const


/*! Computes the length of this vector
 *
 *    @returns    const icReal    The length of this vector
 *
**/
FORCE_INLINE const icReal icVector3::Length(void) const
{
    return icSqrt( x*x + y*y + z*z );
}// END FUNCTION Length( void ) const


/*! Computes the squared length of this vector
 *
 *    @returns    const icReal    The squared length of this vector
 *
**/
FORCE_INLINE const icReal icVector3::LengthSqr( void ) const
{
    return ( x*x + y*y + z*z );
}// END FUNCTION LengthSqr( void ) const


/*! Normalizes the length of this vector to 1
**/
FORCE_INLINE const icVector3& icVector3::Normalize( void )
{
    icReal invLength =  icInvSqrt( x*x + y*y + z*z );
    x *= invLength;
    y *= invLength;
    z *= invLength;
    return *this;
}// END FUNCTION Normalize( void )


/*! Overloaded [] operator
 *
 *    This does no bounds checking
 *
 *    @param[in]    Index            Index to element (0<=index<=2)
 *    @returns    const icReal    Element at index
 *
**/
FORCE_INLINE const icReal icVector3::operator[]( const uint Index ) const
{
    const icReal* p = &x;
    return p[Index];
}// END FUNCTION operator[]( const uint Index ) const


/*! Overloaded + operator, adds vector to this
 *
 *    @param[in]    Rhs            Vector on the right
 *    @returns    icVector3    The sum of the two vectors
**/
FORCE_INLINE icVector3 icVector3::operator+( const icVector3& Rhs ) const
{
    icVector3 v(x+Rhs.x, y+Rhs.y, z+Rhs.z);
    return v;
}// END FUNCTION operator+( const icVector3& Rhs ) const


/*! Overloaded - operator, subtracts vector from this
 *
 *    @param[in]    Rhs            Vector on the right
 *    @returns    icVector3    The sum of the two vectors
**/
FORCE_INLINE icVector3 icVector3::operator-( const icVector3& Rhs ) const
{
    icVector3 v(x-Rhs.x, y-Rhs.y, z-Rhs.z);
    return v;
}// END FUNCTION operator+( const icVector3& Rhs ) const


/*! Overloaded * operator, Dot product of two vectors
 *
 *    @param[in]    Rhs                Vector on the right
 *    @returns    const icReal    The dot product of these vectors
**/
FORCE_INLINE const icReal icVector3::operator*( const icVector3& Rhs ) const
{
    return (x*Rhs.x+y*Rhs.y+z*Rhs.z);
}// END FUNCTION operator*( const icVector3& Rhs ) const


/*! Overloaded * operator, Scales given vector
 *
 *  @param[in]      Scalar          Scalar on the right
 *  @returns        icVector3       The new vector, scaled
**/
FORCE_INLINE icVector3 icVector3::operator*( const icReal& Scalar ) const
{
    icVector3 v(x*Scalar,y*Scalar,z*Scalar);
    return v;
}// END FUNCTION operator*( const icReal& Rhs ) const


/*! Overloaded * operator, Scales given vector
 *
 *  @param[in]      Scalar          Scalar on the right
 *  @returns        icVector3       The new vector, scaled
**/
FORCE_INLINE icVector3 operator*( const icReal& Lhs, const icVector3& Rhs)
{
    icVector3 v(Rhs.x*Lhs,Rhs.y*Lhs,Rhs.z*Lhs);
    return v;
}// END FUNCTION operator*( const icReal& Lhs, const icVector3& Rhs)


/*! Overloaded * operator, Scales given vector
 *
 *  @param[in]      Scalar          Scalar on the right
 *  @returns        icVector3       The new vector, scaled
**/
FORCE_INLINE icVector3 icVector3::operator/( const icReal& Scalar ) const
{
    icVector3 v(x/Scalar,y/Scalar,z/Scalar);
    return v;
}// END FUNCTION operator/( const icReal& Scalar ) const


/*! negates vector
**/
FORCE_INLINE icVector3 icVector3::operator-( void ) const
{
    icVector3 v(-x,-y,-z);
    return v;
}// END FUNCTION operator-( void ) const

/*! Overloaded [] operator
 *
 *    This does no bounds checking
 *
 *    @param[in]    Index            Index to element (0<=index<=2)
 *    @returns    const icReal    Element at index
**/
FORCE_INLINE icReal& icVector3::operator[]( const uint Index )
{
    icReal* p = &x;
    return p[Index];
}// END FUNCTION operator[]( const uint Index )


/*! Overloaded *= operator, scales the vector
 *
 *  @param[in]    Rhs        Scalar
**/
FORCE_INLINE void icVector3::operator*=( const icReal& Scalar )
{
    x *= Scalar;
    y *= Scalar;
    z *= Scalar;
}// END FUNCTION operator*=( const icReal& Rhs )


/*! Overloaded += operator, Adds vectors
 *
 *  @param[in]    Rhs        Vector on the right
**/
FORCE_INLINE void icVector3::operator+=( const icVector3& Rhs )
{
    x += Rhs.x;
    y += Rhs.y;
    z += Rhs.z;
}// END FUNCTION operator+=( const icVector3& Rhs );


/*! Overloaded += operator, Adds vectors
 *
 *  @param[in]    Rhs        Vector on the right
**/
FORCE_INLINE void icVector3::operator-=( const icVector3& Rhs )
{
    x -= Rhs.x;
    y -= Rhs.y;
    z -= Rhs.z;
}// END FUNCTION operator-=( const icVector3& Rhs )


/*! takes the x,y,z components of a vec4
 *
 *  @param          Rhs         Vector4 to take x,y,z from
**/
FORCE_INLINE void icVector3::operator=( const icVector4& Rhs )
{
   x = Rhs.x;
   y = Rhs.y;
   z = Rhs.z;
}// END FUNCTION operator=( const icVector4& Rhs )


/*! Auto cast to icReal pointer
**/
FORCE_INLINE icVector3::operator const icReal*( void ) const
{
    return &x;
}// END FUNCTION 