

#include "Math/Vector/icVector2.h"


/*! Default Constructor
**/
FORCE_INLINE icVector2::icVector2( void )
{
}// END FUNCTION icVector2( void )


/*! Overloaded Constructor, initializes values
**/
FORCE_INLINE icVector2::icVector2( const icReal xIn,
                                   const icReal yIn )
{
    x = xIn;
    y = yIn;
}// END FUNCTION icVector2 ( icReal,icReal,icReal )


/*! Function to set all 3 values in 1 call
 *
 *    @param[in]    xIn        X value
 *    @param[in]    yIn        Y value
 *
**/
FORCE_INLINE void icVector2::Set(    const icReal xIn,
                                    const icReal yIn )
{
    x = xIn;
    y = yIn;
}// END FUNCTION Set


/*! Computes the dot product of two vectors
 *
 *    @param[in]    v1                Vector1
 *    @param[in]    v2                Vector2
 *    @returns    const icReal    Dot product of these vectors
 *
**/
FORCE_INLINE const icReal Dot( const icVector2& v1, const icVector2& v2 )
{
    return (v1.x * v2.x + v1.y * v2.y );
}// END FUNCTION Dot( const icVector2& v1, const icVector2& v2 )


/*! Returns the distance between this and another vector
 *
 *    @param[in]    Rhs                Another vector
 *    @returns    const icReal    Distance between these vectors
 *
**/
FORCE_INLINE const icReal icVector2::Distance( const icVector2& Rhs ) const
{
    return icSqrt(    (x-Rhs.x)*(x-Rhs.x) + (y-Rhs.y)*(y-Rhs.y) );
}// END FUNCTION Distance( const icVector2& Rhs ) const


/*! Returns the squared distance between this and another vector
 *
 *    @param[in]    Rhs                Another vector
 *    @returns    const icReal    Squared distance between these vectors
 *
**/
FORCE_INLINE const icReal
icVector2::DistanceSqr( const icVector2& Rhs ) const
{
    return ((x-Rhs.x)*(x-Rhs.x) + (y-Rhs.y)*(y-Rhs.y));
}// END FUNCTION DistanceSqr( const icVector2& Rhs ) const


/*! Computes the dot product of two vectors
 *
 *    @param[in]    Rhs                Vector to use in dot calc
 *    @returns    const icReal    Dot product of these vectors
 *
**/
FORCE_INLINE const icReal icVector2::Dot( const icVector2& Rhs ) const
{
    return ( x*Rhs.x + y*Rhs.y );
}// END FUNCTION Dot( const icVector2& Rhs ) const


/*! Determines if the vector is effectively zero
 *
 *  @returns        bool        true if the vector is the zero vector
**/
FORCE_INLINE bool icVector2::IsZero(void)
{
    return (LengthSqr() < ICEPSILON);
}// END FUNCTION IsZero(void)


/*! Computes the length of this vector
 *
 *    @returns    const icReal    The length of this vector
 *
**/
FORCE_INLINE const icReal icVector2::Length( void ) const
{
    return icSqrt( x*x + y*y );
}// END FUNCTION Length( void ) const


/*! Computes the squared length of this vector
 *
 *    @returns    const icReal    The squared length of this vector
 *
**/
FORCE_INLINE const icReal icVector2::LengthSqr( void ) const
{
    return ( x*x + y*y );
}// END FUNCTION LengthSqr( void ) const


/*! Normalizes the length of this vector to 1
**/
FORCE_INLINE const icVector2& icVector2::Normalize( void )
{
    icReal invLength =  icInvSqrt( x*x + y*y );
    x *= invLength;
    y *= invLength;
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
FORCE_INLINE const icReal icVector2::operator[]( const uint Index ) const
{
    const icReal* p = &x;
    return p[Index];
}// END FUNCTION operator[]( const uint Index ) const


/*! Overloaded + operator, adds vector to this
 *
 *    @param[in]    Rhs            Vector on the right
 *    @returns    icVector2    The sum of the two vectors
**/
FORCE_INLINE icVector2 icVector2::operator+( const icVector2& Rhs ) const
{
    icVector2 v(x+Rhs.x, y+Rhs.y);
    return v;
}// END FUNCTION operator+( const icVector2& Rhs ) const


/*! Overloaded - operator, subtracts vector to this
 *
 *    @param[in]    Rhs            Vector on the right
 *    @returns    icVector2    The difference of the two vectors
**/
FORCE_INLINE icVector2 icVector2::operator-( const icVector2& Rhs ) const
{
    icVector2 v(x-Rhs.x,y-Rhs.y);
    return v;
}// END FUNCTION operator-( const icVector2& Rhs ) const


/*! Overloaded * operator, Scales given vector
 *
 *    @param[in]    Scalar            Scalar on the right
 *    @returns    icVector2        The new vector, scaled
**/
FORCE_INLINE icVector2 icVector2::operator*( const icReal& Scalar ) const
{
    icVector2 v(x*Scalar,y*Scalar);
    return v;
}// END FUNCTION operator*( const icReal& Rhs ) const


/*! Overloaded * operator, Scales given vector
 *
 *    @param[in]    Scalar            Scalar on the right
 *    @returns    icVector2        The new vector, scaled
**/
FORCE_INLINE icVector2 operator*( const icReal& Lhs, const icVector2& Rhs)
{
    icVector2 v(Lhs*Rhs.x,Lhs*Rhs.y);
    return v;
}// END FUNCTINO operator*( const icReal& Lhs, const icVector2& Rhs)


FORCE_INLINE icVector2 icVector2::operator/( const icReal& Scalar ) const
{
    icVector2 v(x/Scalar,y/Scalar);
    return v;
}

FORCE_INLINE icVector2 icVector2::operator-( void ) const
{
    icVector2 v(-x,-y);
    return v;
}


/*! Overloaded [] operator
 *
 *    This does no bounds checking
 *
 *    @param[in]    Index            Index to element (0<=index<=2)
 *    @returns    const icReal    Element at index
 *
**/
FORCE_INLINE icReal& icVector2::operator[]( const uint Index )
{
    icReal* p = &x;
    return p[Index];
}// END FUNCTION operator[]( const uint Index )


/*! Overloaded *= operator, scales the vector
 *
 *    @param[in]    Rhs        Scalar
 *
**/
FORCE_INLINE void icVector2::operator*=( const icReal& Scalar )
{
    x *= Scalar;
    y *= Scalar;
}// END FUNCTION operator*=( const icReal& Rhs )


/*! Overloaded += operator, Adds vectors
 *
 *    @param[in]    Rhs        Vector on the right
 *
**/
FORCE_INLINE void icVector2::operator+=( const icVector2& Rhs )
{
    x += Rhs.x;
    y += Rhs.y;
}// END FUNCTION operator+=( const icVector2& Rhs );


/*! Overloaded += operator, Adds vectors
 *
 *    @param[in]    Rhs        Vector on the right
 *
**/
FORCE_INLINE void icVector2::operator-=( const icVector2& Rhs )
{
    x -= Rhs.x;
    y -= Rhs.y;
}// END FUNCTION operator-=( const icVector2& Rhs )


/*! Auto cast to icReal pointer
**/
FORCE_INLINE icVector2::operator const icReal*( void ) const
{
    return &x;
}// END FUNCTION 