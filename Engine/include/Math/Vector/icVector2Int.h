#ifndef __IC_VECTOR2_INT_H__
#define __IC_VECTOR2_INT_H__

#include "Math/icCommonMath.h"

class icVector2Int
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYLCE
    icVector2Int( void );
    icVector2Int( const int xIn, const int yIn );

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    const icReal Distance( const icVector2Int& Rhs ) const;
    const icReal DistanceSqr( const icVector2Int& Rhs ) const;
    const icReal Dot( const icVector2Int& Rhs ) const;
    const icReal Length( void ) const;
    const icReal LengthSqr( void ) const;

    void Set( const int xIn, const int yIn );

    friend const icReal Dot( const icVector2Int& v1, const icVector2Int& v2 );

    bool IsZero(void);

    //////////////////////////////////////////////////////////////////////////
    // OPERATORS
    const int operator[]( const uint Index ) const;
    icVector2Int operator+( const icVector2Int& Rhs ) const;
    icVector2Int operator-( const icVector2Int& Rhs ) const;

    icVector2Int operator*( const int& Scalar ) const;
    friend icVector2Int operator*( const int Lhs, const icVector2Int& Rhs);

    icVector2Int operator/( const icReal& Scalar ) const;

    icVector2Int operator-( void ) const;

    int& operator[]( const uint Index );
    void operator*=( const int& Scalar );
    void operator+=( const icVector2Int& Rhs );
    void operator-=( const icVector2Int& Rhs );

    operator const int*( void ) const;

    //////////////////////////////////////////////////////////////////////////
    // PUBLIC MEMBERS
    int     x;
    int     y;

    //////////////////////////////////////////////////////////////////////////
    // PUBLIC STATIC MEMBERS
    static const icVector2Int ZERO;
    static const icVector2Int X_AXIS;
    static const icVector2Int Y_AXIS;
};

#include "Math/Vector/icVector2Int.inl"

#endif //__IC_VECTOR2_INT_H__

