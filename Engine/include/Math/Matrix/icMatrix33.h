#ifndef __IC_MATRIX33_H__
#define __IC_MATRIX33_H__

#include "Math/icCommonMath.h"

class icVector3;
class icQuat;

class icMatrix33
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icMatrix33( void );
    icMatrix33( const icReal* data );
    icMatrix33( const icQuat& q );

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    void Identity( void );

    // quat interchange
    icQuat ToQuat( void ) const;
    void ToQuat( icQuat& q ) const;

    // rotations
    void RotX( const icReal theta );
    void RotY( const icReal theta );
    void RotZ( const icReal theta );
    void Rotate( const icMatrix33& Rhs );

    const icMatrix33& MakeRotX( const icReal theta );
    const icMatrix33& MakeRotY( const icReal theta );
    const icMatrix33& MakeRotZ( const icReal theta );

    const icMatrix33& SetRotation( const icQuat& rot );

    const icMatrix33& Scale( const icVector3& Rhs );
    const icMatrix33& Scale( icReal x, icReal y, icReal z );

    // transformations
    const icMatrix33& Invert( void );
    const icMatrix33& Transpose( void );

    const icReal Determinant( void ) const;
    const icReal Trace( void ) const;

    //////////////////////////////////////////////////////////////////////////
    // ACCESS
    icVector3 GetRow( uchar i ) const;
    icVector3 GetCol( uchar i ) const;

    void SetColumns( const icVector3& col1,
                    const icVector3& col2,
                    const icVector3& col3 );

    //////////////////////////////////////////////////////////////////////////
    // OPERATORS
    icReal& operator()( uchar row, uchar col );
    const icReal operator()( uchar row, uchar col ) const;

    // 3x3 * 3x1
    icVector3 operator*( const icVector3& Rhs ) const;
    friend icVector3 operator*( const icVector3& Lhs,
                                const icMatrix33& Rhs );

    // 3x3 * 3x3
    icMatrix33 operator*( const icMatrix33& Rhs ) const;
    const icMatrix33& operator*=( const icMatrix33& Rhs);

    const icMatrix33& operator=( const icQuat& Rhs );

    operator icReal*( void );
    operator const icReal*( void ) const;    

    static const icMatrix33 IDENTITY;
private:
    icReal    m_el[9];
};

#include "Math/Matrix/icMatrix33.inl"

#endif //__IC_MATRIX33_H__

