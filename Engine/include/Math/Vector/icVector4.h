#ifndef __IC_VECTOR4_H__
#define __IC_VECTOR4_H__

#include "Math/icCommonMath.h"

class icVector4
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYLCE
    icVector4( void );
    icVector4( const icReal xIn, const icReal yIn,
               const icReal zIn, const icReal wIn );

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    const icReal Distance( const icVector4& Rhs ) const;
    const icReal DistanceSqr( const icVector4& Rhs ) const;
    const icReal Dot( const icVector4& Rhs ) const;
    const icReal Length( void ) const;
    const icReal LengthSqr( void ) const;

    const icVector4& Normalize( void );

    void Set( const icReal xIn, const icReal yIn,
              const icReal zIn, const icReal wIn );

    friend const icReal Dot( const icVector4& v1, const icVector4& v2 );


    //////////////////////////////////////////////////////////////////////////
    // OPERATORS
    const icReal operator[]( const uint Index ) const;
    icVector4 operator+( const icVector4& Rhs ) const;
    icVector4 operator-( const icVector4& Rhs ) const;
    icVector4 operator*( const icReal& Scalar ) const;
    const icReal operator*( const icVector4& Rhs ) const;

    icReal& operator[]( const uint Index );
    void operator*=( const icReal& Scalar );
    void operator+=( const icVector4& Rhs );
    void operator-=( const icVector4& Rhs );

    operator const icReal*( void ) const;

    //////////////////////////////////////////////////////////////////////////
    // PUBLIC MEMBERS
    icReal    x;
    icReal    y;
    icReal    z;
    icReal    w;

    //////////////////////////////////////////////////////////////////////////
    // PUBLIC STATIC MEMBERS
    static icVector4 ZERO;
    static icVector4 X_AXIS;
    static icVector4 Y_AXIS;
    static icVector4 Z_AXIS;
    static icVector4 W_AXIS;
};

#include "Math/Vector/icVector4.inl"

#endif //__IC_VECTOR4_H__

