#ifndef __IC_VECTOR3_H__
#define __IC_VECTOR3_H__

#include "Math/icCommonMath.h"

class icVector4;

class icVector3
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYLCE
    icVector3( void );
    icVector3( const icVector4& Rhs );
    icVector3( const icReal xIn, const icReal yIn, const icReal zIn );

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS

    const icReal Distance( const icVector3& Rhs ) const;
    const icReal DistanceSqr( const icVector3& Rhs ) const;
    const icReal Dot( const icVector3& Rhs ) const;
    const icReal Length( void ) const;
    const icReal LengthSqr( void ) const;

    const icVector3& Normalize( void );

    void Set( const icReal xIn, const icReal yIn, const icReal zIn );

    void MakeCross( const icVector3& v1, const icVector3& v2 );
    friend icVector3 Cross( const icVector3& v1, const icVector3& v2 );
    friend const icReal Dot( const icVector3& v1, const icVector3& v2 );


    //////////////////////////////////////////////////////////////////////////
    // OPERATORS
    const icReal operator[]( const uint Index ) const;
    icVector3 operator+( const icVector3& Rhs ) const;
    icVector3 operator-( const icVector3& Rhs ) const;

    icVector3 operator*( const icReal& Scalar ) const;
    friend icVector3 operator*( const icReal& Lhs, const icVector3& Rhs);

    icVector3 operator/( const icReal& Scalar ) const;

    icVector3 operator-( void ) const;

    const icReal operator*( const icVector3& Rhs ) const;

    

    icReal& operator[]( const uint Index );
    void operator*=( const icReal& Scalar );
    void operator+=( const icVector3& Rhs );
    void operator-=( const icVector3& Rhs );

    void operator=( const icVector4& Rhs );

    operator const icReal*( void ) const;

    //////////////////////////////////////////////////////////////////////////
    // PUBLIC MEMBERS
    icReal    x;
    icReal    y;
    icReal    z;

    //////////////////////////////////////////////////////////////////////////
    // PUBLIC STATIC MEMBERS
    static icVector3 ZERO;
    static icVector3 X_AXIS;
    static icVector3 Y_AXIS;
    static icVector3 Z_AXIS;
};

#include "Math/Vector/icVector3.inl"

#endif //__IC_VECTOR3_H__

