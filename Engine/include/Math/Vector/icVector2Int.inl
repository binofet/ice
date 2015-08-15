

#include "Math/Vector/icVector2Int.h"


/*! Default Constructor
**/
FORCE_INLINE icVector2Int::icVector2Int(void)
{
}// END FUNCTION icVector2Int(void)


/*! Overloaded Constructor, initializes values
**/
FORCE_INLINE icVector2Int::icVector2Int(const int xIn,
                                        const int yIn )
{
    x = xIn;
    y = yIn;
}// END FUNCTION icVector2Int (icReal,icReal,icReal)


/*! Function to set all 3 values in 1 call
 *
 *    @param[in]    xIn        X value
 *    @param[in]    yIn        Y value
 *
**/
FORCE_INLINE void icVector2Int::Set( const int xIn,
                                     const int yIn )
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
FORCE_INLINE const icReal Dot( const icVector2Int& v1, const icVector2Int& v2 )
{
    return (icReal)(v1.x * v2.x + v1.y * v2.y );
}// END FUNCTION Dot( const icVector2Int& v1, const icVector2Int& v2 )


/*! Returns the distance between this and another vector
 *
 *    @param[in]    Rhs                Another vector
 *    @returns    const icReal    Distance between these vectors
 *
**/
FORCE_INLINE const icReal icVector2Int::Distance( const icVector2Int& Rhs ) const
{
    return icSqrt( (icReal)(x-Rhs.x)*(x-Rhs.x) + (y-Rhs.y)*(y-Rhs.y) );
}// END FUNCTION Distance( const icVector2Int& Rhs ) const


/*! Returns the squared distance between this and another vector
 *
 *    @param[in]    Rhs                Another vector
 *    @returns    const icReal    Squared distance between these vectors
 *
**/
FORCE_INLINE const icReal
icVector2Int::DistanceSqr( const icVector2Int& Rhs ) const
{
    return (icReal)((x-Rhs.x)*(x-Rhs.x) + (y-Rhs.y)*(y-Rhs.y));
}// END FUNCTION DistanceSqr( const icVector2Int& Rhs ) const


/*! Computes the dot product of two vectors
 *
 *    @param[in]    Rhs                Vector to use in dot calc
 *    @returns    const icReal    Dot product of these vectors
 *
**/
FORCE_INLINE const icReal icVector2Int::Dot( const icVector2Int& Rhs ) const
{
    return (icReal)( x*Rhs.x + y*Rhs.y );
}// END FUNCTION Dot( const icVector2Int& Rhs ) const


/*! Determines if the vector is effectively zero
 *
 *  @returns        bool        true if the vector is the zero vector
**/
FORCE_INLINE bool icVector2Int::IsZero(void)
{
    return (LengthSqr() < ICEPSILON);
}// END FUNCTION IsZero(void)


/*! Computes the length of this vector
 *
 *    @returns    const icReal    The length of this vector
 *
**/
FORCE_INLINE const icReal icVector2Int::Length( void ) const
{
    return icSqrt( (icReal)(x*x + y*y) );
}// END FUNCTION Length( void ) const


/*! Computes the squared length of this vector
 *
 *    @returns    const icReal    The squared length of this vector
 *
**/
FORCE_INLINE const icReal icVector2Int::LengthSqr( void ) const
{
    return (icReal)( x*x + y*y );
}// END FUNCTION LengthSqr( void ) const


///*! Normalizes the length of this vector to 1
//**/
//FORCE_INLINE const icVector2Int& icVector2Int::Normalize( void )
//{
//    icReal invLength =  icInvSqrt( x*x + y*y );
//    x *= invLength;
//    y *= invLength;
//    return *this;
//}// END FUNCTION Normalize( void )


/*! Overloaded [] operator
 *
 *    This does no bounds checking
 *
 *    @param[in]    Index            Index to element (0<=index<=2)
 *    @returns    const icReal    Element at index
 *
**/
FORCE_INLINE const int icVector2Int::operator[]( const uint Index ) const
{
    const int* p = &x;
    return p[Index];
}// END FUNCTION operator[]( const uint Index ) const


/*! Overloaded + operator, adds vector to this
 *
 *    @param[in]    Rhs            Vector on the right
 *    @returns    icVector2Int    The sum of the two vectors
**/
FORCE_INLINE icVector2Int icVector2Int::operator+( const icVector2Int& Rhs ) const
{
    icVector2Int v(x+Rhs.x, y+Rhs.y);
    return v;
}// END FUNCTION operator+( const icVector2Int& Rhs ) const


/*! Overloaded - operator, subtracts vector to this
 *
 *    @param[in]    Rhs            Vector on the right
 *    @returns    icVector2Int    The difference of the two vectors
**/
FORCE_INLINE icVector2Int icVector2Int::operator-( const icVector2Int& Rhs ) const
{
    icVector2Int v(x-Rhs.x,y-Rhs.y);
    return v;
}// END FUNCTION operator-( const icVector2Int& Rhs ) const


/*! Overloaded * operator, Scales given vector
 *
 *    @param[in]    Scalar            Scalar on the right
 *    @returns    icVector2Int        The new vector, scaled
**/
FORCE_INLINE icVector2Int icVector2Int::operator*( const int& Scalar ) const
{
    icVector2Int v(x*Scalar,y*Scalar);
    return v;
}// END FUNCTION operator*( const icReal& Rhs ) const


/*! Overloaded * operator, Scales given vector
 *
 *    @param[in]    Scalar            Scalar on the right
 *    @returns    icVector2Int        The new vector, scaled
**/
FORCE_INLINE icVector2Int operator*( const int& Lhs, const icVector2Int& Rhs)
{
    icVector2Int v(Lhs*Rhs.x,Lhs*Rhs.y);
    return v;
}// END FUNCTINO operator*( const icReal& Lhs, const icVector2Int& Rhs)


//FORCE_INLINE icVector2Int icVector2Int::operator/( const int& Scalar ) const
//{
//    icVector2Int v(x/Scalar,y/Scalar);
//    return v;
//}

FORCE_INLINE icVector2Int icVector2Int::operator-( void ) const
{
    icVector2Int v(-x,-y);
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
FORCE_INLINE int& icVector2Int::operator[]( const uint Index )
{
    int* p = &x;
    return p[Index];
}// END FUNCTION operator[]( const uint Index )


/*! Overloaded *= operator, scales the vector
 *
 *    @param[in]    Rhs        Scalar
 *
**/
FORCE_INLINE void icVector2Int::operator*=( const int& Scalar )
{
    x *= Scalar;
    y *= Scalar;
}// END FUNCTION operator*=( const icReal& Rhs )


/*! Overloaded += operator, Adds vectors
 *
 *    @param[in]    Rhs        Vector on the right
 *
**/
FORCE_INLINE void icVector2Int::operator+=( const icVector2Int& Rhs )
{
    x += Rhs.x;
    y += Rhs.y;
}// END FUNCTION operator+=( const icVector2Int& Rhs );


/*! Overloaded += operator, Adds vectors
 *
 *    @param[in]    Rhs        Vector on the right
 *
**/
FORCE_INLINE void icVector2Int::operator-=( const icVector2Int& Rhs )
{
    x -= Rhs.x;
    y -= Rhs.y;
}// END FUNCTION operator-=( const icVector2Int& Rhs )


/*! Auto cast to icReal pointer
**/
FORCE_INLINE icVector2Int::operator const int*( void ) const
{
    return &x;
}// END FUNCTION 