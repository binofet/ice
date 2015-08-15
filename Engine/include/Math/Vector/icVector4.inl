

#include "Math/Vector/icVector4.h"


/*! Default Constructor
**/
FORCE_INLINE icVector4::icVector4( void )
{
}// END FUNCTION icVector4( void )


/*! Overloaded Constructor, initializes values
**/
FORCE_INLINE icVector4::icVector4( const icReal xIn,
                                   const icReal yIn,
                                   const icReal zIn,
                                   const icReal wIn )
{
    x = xIn;
    y = yIn;
    z = zIn;
    w = wIn;
}// END FUNCTION icVector4 ( icReal,icReal,icReal )


/*! Function to set all 3 values in 1 call
 *
 *    @param[in]    xIn        X value
 *    @param[in]    yIn        Y value
 *    @param[in]    zIn        Z value
 *
**/
FORCE_INLINE void icVector4::Set(    const icReal xIn,
                                    const icReal yIn,
                                    const icReal zIn,
                                    const icReal wIn )
{
    x = xIn;
    y = yIn;
    z = zIn;
    w = wIn;
}// END FUNCTION Set


/*! Computes the dot product of two vectors
 *
 *    @param[in]    v1                Vector1
 *    @param[in]    v2                Vector2
 *    @returns    const icReal    Dot product of these vectors
 *
**/
FORCE_INLINE const icReal Dot( const icVector4& v1, const icVector4& v2 )
{
    return (v1.x * v2.x + v1.y * v2.y + v1.z * v2.z + v1.w * v2.w);
}// END FUNCTION Dot( const icVector4& v1, const icVector4& v2 )


/*! Returns the distance between this and another vector
 *
 *    @param[in]    Rhs                Another vector
 *    @returns    const icReal    Distance between these vectors
 *
**/
FORCE_INLINE const icReal icVector4::Distance( const icVector4& Rhs ) const
{
    return icSqrt(    (x-Rhs.x)*(x-Rhs.x) +
                    (y-Rhs.y)*(y-Rhs.y) +
                    (z-Rhs.z)*(z-Rhs.z) +
                    (w-Rhs.w)*(w-Rhs.w) );
}// END FUNCTION Distance( const icVector4& Rhs ) const


/*! Returns the squared distance between this and another vector
 *
 *    @param[in]    Rhs                Another vector
 *    @returns    const icReal    Squared distance between these vectors
 *
**/
FORCE_INLINE const icReal
icVector4::DistanceSqr( const icVector4& Rhs ) const
{
    return ((x-Rhs.x)*(x-Rhs.x) +
            (y-Rhs.y)*(y-Rhs.y) +
            (z-Rhs.z)*(z-Rhs.z) +
            (w-Rhs.w)*(w-Rhs.w) );
}// END FUNCTION DistanceSqr( const icVector4& Rhs ) const


/*! Computes the dot product of two vectors
 *
 *    @param[in]    Rhs                Vector to use in dot calc
 *    @returns    const icReal    Dot product of these vectors
 *
**/
FORCE_INLINE const icReal icVector4::Dot( const icVector4& Rhs ) const
{
    return ( x*Rhs.x + y*Rhs.y + z*Rhs.z + w*Rhs.w);
}// END FUNCTION Dot( const icVector4& Rhs ) const


/*! Computes the length of this vector
 *
 *    @returns    const icReal    The length of this vector
 *
**/
FORCE_INLINE const icReal icVector4::Length( void ) const
{
    return icSqrt( x*x + y*y + z*z + w*w);
}// END FUNCTION Length( void ) const


/*! Computes the squared length of this vector
 *
 *    @returns    const icReal    The squared length of this vector
 *
**/
FORCE_INLINE const icReal icVector4::LengthSqr( void ) const
{
    return ( x*x + y*y + z*z + w*w);
}// END FUNCTION LengthSqr( void ) const


/*! Normalizes the length of this vector to 1
**/
FORCE_INLINE const icVector4& icVector4::Normalize( void )
{
    icReal invLength =  icInvSqrt( x*x + y*y + z*z + w*w);
    x *= invLength;
    y *= invLength;
    z *= invLength;
    w *= invLength;
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
FORCE_INLINE const icReal icVector4::operator[]( const uint Index ) const
{
    const icReal* p = &x;
    return p[Index];
}// END FUNCTION operator[]( const uint Index ) const


/*! Overloaded + operator, adds vector to this
 *
 *    @param[in]    Rhs            Vector on the right
 *    @returns    icVector4    The sum of the two vectors
**/
FORCE_INLINE icVector4 icVector4::operator+( const icVector4& Rhs ) const
{
    icVector4 v(x+Rhs.x, y+Rhs.y, z*Rhs.z, w*Rhs.z);
    return v;
}// END FUNCTION operator+( const icVector4& Rhs ) const


/*! Overloaded - operator, subtracts vector to this
 *
 *    @param[in]    Rhs            Vector on the right
 *    @returns    icVector4    The difference of the two vectors
**/
FORCE_INLINE icVector4 icVector4::operator-( const icVector4& Rhs ) const
{
    icVector4 v(x-Rhs.x,y-Rhs.y,z-Rhs.z, w-Rhs.w);
    return v;
}// END FUNCTION operator-( const icVector4& Rhs ) const


/*! Overloaded * operator, Dot product of two vectors
 *
 *    @param[in]    Rhs                Vector on the right
 *    @returns    const icReal    The dot product of these vectors
**/
FORCE_INLINE const icReal icVector4::operator*( const icVector4& Rhs ) const
{
    return (x*Rhs.x+y*Rhs.y+z*Rhs.z+w*Rhs.w);
}// END FUNCTION operator*( const icVector4& Rhs ) const


/*! Overloaded * operator, Scales given vector
 *
 *    @param[in]    Scalar            Scalar on the right
 *    @returns    icVector4        The new vector, scaled
**/
FORCE_INLINE icVector4 icVector4::operator*( const icReal& Scalar ) const
{
    icVector4 v(x*Scalar,y*Scalar,z*Scalar,w*Scalar);
    return v;
}// END FUNCTION operator*( const icReal& Rhs ) const


/*! Overloaded [] operator
 *
 *    This does no bounds checking
 *
 *    @param[in]    Index            Index to element (0<=index<=2)
 *    @returns    const icReal    Element at index
 *
**/
FORCE_INLINE icReal& icVector4::operator[]( const uint Index )
{
    icReal* p = &x;
    return p[Index];
}// END FUNCTION operator[]( const uint Index )


/*! Overloaded *= operator, scales the vector
 *
 *    @param[in]    Rhs        Scalar
 *
**/
FORCE_INLINE void icVector4::operator*=( const icReal& Scalar )
{
    x *= Scalar;
    y *= Scalar;
    z *= Scalar;
    w *= Scalar;
}// END FUNCTION operator*=( const icReal& Rhs )


/*! Overloaded += operator, Adds vectors
 *
 *    @param[in]    Rhs        Vector on the right
 *
**/
FORCE_INLINE void icVector4::operator+=( const icVector4& Rhs )
{
    x += Rhs.x;
    y += Rhs.y;
    z += Rhs.z;
    w += Rhs.w;
}// END FUNCTION operator+=( const icVector4& Rhs );


/*! Overloaded += operator, Adds vectors
 *
 *    @param[in]    Rhs        Vector on the right
 *
**/
FORCE_INLINE void icVector4::operator-=( const icVector4& Rhs )
{
    x -= Rhs.x;
    y -= Rhs.y;
    z -= Rhs.z;
    w -= Rhs.w;
}// END FUNCTION operator-=( const icVector4& Rhs )


/*! Auto cast to icReal pointer
**/
FORCE_INLINE icVector4::operator const icReal*( void ) const
{
    return &x;
}// END FUNCTION 