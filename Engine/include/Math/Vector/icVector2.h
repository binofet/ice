#ifndef __IC_VECTOR2_H__
#define __IC_VECTOR2_H__

#include "Math/icCommonMath.h"

class icVector2
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYLCE
    icVector2( void );
    icVector2( const icReal xIn, const icReal yIn );

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    const icReal Distance( const icVector2& Rhs ) const;
    const icReal DistanceSqr( const icVector2& Rhs ) const;
    const icReal Dot( const icVector2& Rhs ) const;
    const icReal Length( void ) const;
    const icReal LengthSqr( void ) const;

    const icVector2& Normalize( void );

    void Set( const icReal xIn, const icReal yIn );

    friend const icReal Dot( const icVector2& v1, const icVector2& v2 );

    bool IsZero(void);

    //////////////////////////////////////////////////////////////////////////
    // OPERATORS
    const icReal operator[]( const uint Index ) const;
    icVector2 operator+( const icVector2& Rhs ) const;
    icVector2 operator-( const icVector2& Rhs ) const;

    icVector2 operator*( const icReal& Scalar ) const;
    friend icVector2 operator*( const icReal& Lhs, const icVector2& Rhs);

    icVector2 operator/( const icReal& Scalar ) const;

    icVector2 operator-( void ) const;

    icReal& operator[]( const uint Index );
    void operator*=( const icReal& Scalar );
    void operator+=( const icVector2& Rhs );
    void operator-=( const icVector2& Rhs );

    operator const icReal*( void ) const;

    //////////////////////////////////////////////////////////////////////////
    // PUBLIC MEMBERS
    icReal    x;
    icReal    y;

    //////////////////////////////////////////////////////////////////////////
    // PUBLIC STATIC MEMBERS
    static const icVector2 ZERO;
    static const icVector2 X_AXIS;
    static const icVector2 Y_AXIS;
};

#include "Math/Vector/icVector2.inl"

#endif //__IC_VECTOR2_H__

