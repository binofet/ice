#ifndef __IC_QUAT_H__
#define __IC_QUAT_H__


class icVector4;
class icMatrix33;
class icMatrix44;

class icQuat : public icVector4
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYLCE
    icQuat( void );
    icQuat( const icMatrix33& mat );
    icQuat( const icReal xIn, const icReal yIn,
            const icReal zIn, const icReal wIn );

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    icMatrix44 MakeTransform( const icVector3& pos ) const;
    icMatrix33 MakeRotation( void ) const;

    const icQuat& Invert( void );

    const icQuat& Conjugate( void );

    icQuat operator*( const icQuat& Rhs ) const;
    const icQuat& operator*=( const icQuat& Rhs );

    // addition/subtraction
    icQuat operator+( const icQuat& Rhs ) const;
    const icQuat& operator+=( const icQuat& Rhs );
    icQuat operator-( const icQuat& Rhs ) const;
    const icQuat& operator-=( const icQuat& Rhs );

    // scaling
    icQuat operator*( const icReal& Rhs ) const;
    friend icQuat operator*( const icReal& Lhs, const icQuat& Rhs );

    const icQuat& Lerp( const icQuat& end, icReal t );
    const icQuat& Slerp( const icQuat& end, icReal t );
    const icQuat& ApproxSlerp( const icQuat& end, icReal t );
};

#include "Math/Vector/icQuat.inl"

#endif //__IC_QUAT_H__

