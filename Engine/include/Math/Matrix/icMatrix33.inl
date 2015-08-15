
#include "Math/Matrix/icMatrix33.h"
#include "Math/Vector/icVector3.h"
#include "Math/Vector/icQuat.h"

/*! Default Constructor
**/
FORCE_INLINE icMatrix33::icMatrix33( void )
{
}// END FUNCTION icMatrix33( void )


/*! Initializes the matrix with array data
**/
FORCE_INLINE icMatrix33::icMatrix33( const icReal* data )
{
    memcpy(m_el,data,sizeof(icReal)*9);
}// END FUNCTION icMatrix33( const icReal* data)


/*! Overloaded Constructor
 *
 *    Initializes rotation with given quat
 *
 *    @param[in]    q    Rotation
 *
**/
FORCE_INLINE icMatrix33::icMatrix33( const icQuat& q )
{
    icReal xs, ys, zs, wx, wy, wz, xx, xy, xz, yy, yz, zz;

    xs = q.x+q.x;
    ys = q.y+q.y;
    zs = q.z+q.z;
    wx = q.w*xs;
    wy = q.w*ys;
    wz = q.w*zs;
    xx = q.x*xs;
    xy = q.x*ys;
    xz = q.x*zs;
    yy = q.y*ys;
    yz = q.y*zs;
    zz = q.z*zs;

    m_el[0] = 1.0f - (yy + zz);
    m_el[3] = xy - wz;
    m_el[6] = xz + wy;

    m_el[1] = xy + wz;
    m_el[4] = 1.0f - (xx + zz);
    m_el[7] = yz - wx;

    m_el[2] = xz - wy;
    m_el[5] = yz + wx;
    m_el[8] = 1.0f - (xx + yy);
}// END FUNCTION icMatrix33( const icQuat& q )


/*! Makes the matrix the identity
**/
FORCE_INLINE void icMatrix33::Identity()
{
    m_el[0] = 1.0f; m_el[1] = 0.0f; m_el[2] = 0.0f;
    m_el[3] = 0.0f; m_el[4] = 1.0f; m_el[5] = 0.0f;
    m_el[6] = 0.0f; m_el[7] = 0.0f; m_el[8] = 1.0f;
}// END FUNCTION Identity()


/*! Makes a quaternion from this rotation matrix
 *
 *    @returns    icQuat    Quaternion from this matrix
 *
**/
FORCE_INLINE icQuat icMatrix33::ToQuat( void ) const
{
    icQuat q;

    icReal trace = Trace();
    if ( trace > 0.0f )
    {
        icReal s = icSqrt( trace + icReal(1) );
        q.w = s*icReal(0.5);
        icReal recip = icReal(0.5)/s;
        q.x = m_el[5] - m_el[7]*recip;
        q.y = m_el[6] - m_el[2]*recip;
        q.z = m_el[1] - m_el[3]*recip;
    }
    else
    {
        unsigned int i = 0;
        if ( m_el[4] > m_el[0] )
            i = 1;
        if ( m_el[8] > m_el[i+3*i] )
            i = 2;
        unsigned int j = (i+1)%3;
        unsigned int k = (j+1)%3;
        icReal s =  m_el[i+3*i] - m_el[j+3*j] - m_el[k+3*k] + icReal(1.0);
        s = icSqrt(s);
        q[i] = icReal(0.5)*s;
        icReal recip = icReal(0.5)/s;
        q.w = m_el[k+3*j] - m_el[j+3*k]*recip;
        q[j] = m_el[j+3*i] + m_el[i+3*j]*recip;
        q[k] = m_el[k+3*i] + m_el[i+3*k]*recip;
    }

    return q;
}// END FUNCTION ToQuat( void )


/*! Converts matrix to a quaternion
 *
 *    @param[out]    q    Quaternion from this matrix
 *
**/
FORCE_INLINE void icMatrix33::ToQuat( icQuat& q ) const
{
    icReal trace = Trace();
    if ( trace > 0.0f )
    {
        icReal s = icSqrt( trace + icReal(1) );
        q.w = s*icReal(0.5);
        icReal recip = icReal(0.5)/s;
        q.x = m_el[5] - m_el[7]*recip;
        q.y = m_el[6] - m_el[2]*recip;
        q.z = m_el[1] - m_el[3]*recip;
    }
    else
    {
        unsigned int i = 0;
        if ( m_el[4] > m_el[0] )
            i = 1;
        if ( m_el[8] > m_el[i+3*i] )
            i = 2;
        unsigned int j = (i+1)%3;
        unsigned int k = (j+1)%3;
        icReal s =  m_el[i+3*i] - m_el[j+3*j] - m_el[k+3*k] + icReal(1.0);
        s = icSqrt(s);
        q[i] = icReal(0.5)*s;
        icReal recip = icReal(0.5)/s;
        q.w = m_el[k+3*j] - m_el[j+3*k]*recip;
        q[j] = m_el[j+3*i] + m_el[i+3*j]*recip;
        q[k] = m_el[k+3*i] + m_el[i+3*k]*recip;
    }
}// END FUNCTION ToQuat( icQuat& q ) const


/*! Rotates this matrix theta radians about the X axis
 *
 *    @param[in]    theta    Angle to rotate about the X axis
**/
FORCE_INLINE void icMatrix33::RotX( const icReal theta )
{
    icReal c = icCos( theta );
    icReal s = icSin( theta );
    icReal t4 = m_el[4];
    icReal t5 = m_el[5];
    m_el[4] = m_el[4] * c + m_el[7] * s;
    m_el[5] = m_el[5] * c + m_el[8] * s;

    m_el[7] = t4 * -s + m_el[7] * c;
    m_el[8] = t5 * -s + m_el[8] * c;
}// END FUNCTION RotX( const icReal theta )


/*! Rotates this matrix theta radians about the Y axis
 *
 *    @param[in]    theta    Angle to rotate about the Y axis
**/
FORCE_INLINE void icMatrix33::RotY( const icReal theta )
{
    icReal c = icCos( theta );
    icReal s = icSin( theta );
    icReal t0 = m_el[0];
    icReal t2 = m_el[2];
    m_el[0] = m_el[0] * c - m_el[6] * s;
    m_el[2] = m_el[2] * c - m_el[8] * s;

    m_el[6] = t0 * s + m_el[6] * c;
    m_el[8] = t2 * s + m_el[8] * c;
}// END FUNCTION RotY( const icReal theta )


/*! Rotates this matrix theta radians about the Z axis
 *
 *    @param[in]    theta    Angle to rotate about the Z axis
**/
FORCE_INLINE void icMatrix33::RotZ( const icReal theta )
{
    icReal c = icCos( theta );
    icReal s = icSin( theta );
    icReal t0 = m_el[0];
    icReal t1 = m_el[1];
    m_el[0] = m_el[0] * c + m_el[3] * s;
    m_el[1] = m_el[1] * c + m_el[4] * s;

    m_el[3] = t0 * -s + m_el[3] * c;
    m_el[4] = t1 * -s + m_el[4] * c;
}// END FUNCTION RotZ( icReal theta )


/*! Multiplies this matrix by another
 *
 *    @param[in]    Rhs        Matrix to multiply
 *
**/
FORCE_INLINE void icMatrix33::Rotate( const icMatrix33& Rhs )
{
    icMatrix33 m;
    m(0,0)=m_el[0]*Rhs(0,0)+m_el[3]*Rhs(0,1)+m_el[6]*Rhs(0,2);
    m(0,1)=m_el[1]*Rhs(0,0)+m_el[4]*Rhs(0,1)+m_el[7]*Rhs(0,2);
    m(0,2)=m_el[2]*Rhs(0,0)+m_el[5]*Rhs(0,1)+m_el[8]*Rhs(0,2);

    m(1,0)=m_el[0]*Rhs(1,0)+m_el[3]*Rhs(1,1)+m_el[6]*Rhs(1,2);
    m(1,1)=m_el[1]*Rhs(1,0)+m_el[4]*Rhs(1,1)+m_el[7]*Rhs(1,2);
    m(1,2)=m_el[2]*Rhs(1,0)+m_el[5]*Rhs(1,1)+m_el[8]*Rhs(1,2);

    m(2,0)=m_el[0]*Rhs(2,0)+m_el[3]*Rhs(2,1)+m_el[6]*Rhs(1,2);
    m(2,1)=m_el[1]*Rhs(2,0)+m_el[4]*Rhs(2,1)+m_el[7]*Rhs(1,2);
    m(2,2)=m_el[2]*Rhs(2,0)+m_el[5]*Rhs(2,1)+m_el[8]*Rhs(1,2);
    *this = m;
}// END FUNCTION Rotate( const icMatrix& Rhs )


/*! Makes this a roatation matrix, rotated around the x-axis
 *
 *  @param          theta       Angle in radians
**/
FORCE_INLINE const icMatrix33& icMatrix33::MakeRotX( const icReal theta )
{
    icReal s = icSin(theta);
    icReal c = icCos(theta);

    m_el[0] = 1.0f;
    m_el[1] = 0.0f;
    m_el[2] = 0.0f;
    m_el[3] = 0.0f;
    m_el[4] = c;
    m_el[5] = s;
    m_el[6] = 0.0f;
    m_el[7] = -s;
    m_el[8] = c;

    return *this;
}// END FUNCTION MakeRotX( const icReal theta )


/*! Makes this a roatation matrix, rotated around the y-axis
 *
 *  @param          theta       Angle in radians
**/
FORCE_INLINE const icMatrix33& icMatrix33::MakeRotY( const icReal theta )
{
    icReal s = icSin(theta);
    icReal c = icCos(theta);

    m_el[0] = c;
    m_el[1] = 0.0f;
    m_el[2] = -s;
    m_el[3] = 0.0f;
    m_el[4] = 1.0f;
    m_el[5] = 0.0f;
    m_el[6] = s;
    m_el[7] = 0.0f;
    m_el[8] = c;

    return *this;
}// END FUNCTION MakeRotY( const icReal theta )


/*! Makes this a roatation matrix, rotated around the z-axis
 *
 *  @param          theta       Angle in radians
**/
FORCE_INLINE const icMatrix33& icMatrix33::MakeRotZ( const icReal theta )
{
    icReal s = icSin(theta);
    icReal c = icCos(theta);

    m_el[0] = c;
    m_el[1] = s;
    m_el[2] = 0.0f;
    m_el[3] = -s;
    m_el[4] = c;
    m_el[5] = 0.0f;
    m_el[6] = 0.0f;
    m_el[7] = 0.0f;
    m_el[8] = 1.0f;

    return *this;
}// END FUNCTION MakeRotZ( const icReal theta )


/*! Sets the rotation of this matrix from a quaternion
 *
 *  @param          rot         Quaternion to set rotation from
**/
FORCE_INLINE const icMatrix33& icMatrix33::SetRotation( const icQuat& rot )
{
    icReal xs, ys, zs, wx, wy, wz, xx, xy, xz, yy, yz, zz;

    xs = rot.x+rot.x;
    ys = rot.y+rot.y;
    zs = rot.z+rot.z;
    wx = rot.w*xs;
    wy = rot.w*ys;
    wz = rot.w*zs;
    xx = rot.x*xs;
    xy = rot.x*ys;
    xz = rot.x*zs;
    yy = rot.y*ys;
    yz = rot.y*zs;
    zz = rot.z*zs;

    m_el[0] = 1.0f - (yy + zz);
    m_el[3] = xy - wz;
    m_el[6] = xz + wy;

    m_el[1] = xy + wz;
    m_el[4] = 1.0f - (xx + zz);
    m_el[7] = yz - wx;

    m_el[2] = xz - wy;
    m_el[5] = yz + wx;
    m_el[8] = 1.0f - (xx + yy);

    return *this;
}// END FUNCTION SetRotation( const icQuat& rot )


/*! Scales the matrix
 *
 *  @param          Rhs         Vector to scale the matrix by
**/
FORCE_INLINE const icMatrix33& icMatrix33::Scale( const icVector3& Rhs )
{
    m_el[0] *= Rhs.x;
    m_el[4] *= Rhs.y;
    m_el[8] *= Rhs.z;

    return *this;
}// END FUNCTION Scale( const icVector3& Rhs )


/*! Scales the matrix
 *
 *  @param          x           Amount to scale in X
 *  @param          y           Amount to scale in Y
 *  @param          z           Amount to scale in Z
**/
FORCE_INLINE
const icMatrix33& icMatrix33::Scale( icReal x, icReal y, icReal z )
{
    m_el[0] *= x;
    m_el[4] *= y;
    m_el[8] *= z;

    return *this;
}// END FUNCTION Scale( icReal x, icReal y, icReal z)


/*! Calculates the inverse of this matrix
 *
 *  @returns        const icMatrix44&       The inverse of this matrix
**/
FORCE_INLINE const icMatrix33& icMatrix33::Invert( void )
{
    icMatrix33 m;

    // compute determinant
    icReal cofactor0 = m_el[4]*m_el[8] - m_el[5]*m_el[7];
    icReal cofactor3 = m_el[2]*m_el[7] - m_el[1]*m_el[8];
    icReal cofactor6 = m_el[1]*m_el[5] - m_el[2]*m_el[4];
    icReal det = m_el[0]*cofactor0 + m_el[3]*cofactor3 + m_el[6]*cofactor6;
    if (icIsZero( det ))
    {
        icWarningf("icMatrix33::Invert() -- singular matrix");
        return *this;
    }

    // create adjoint matrix and multiply by 1/det to get inverse
    icReal invDet = 1.0f/det;
    m.m_el[0] = invDet*cofactor0;
    m.m_el[1] = invDet*cofactor3;
    m.m_el[2] = invDet*cofactor6;
   
    m.m_el[3] = invDet*(m_el[5]*m_el[6] - m_el[3]*m_el[8]);
    m.m_el[4] = invDet*(m_el[0]*m_el[8] - m_el[2]*m_el[6]);
    m.m_el[5] = invDet*(m_el[2]*m_el[3] - m_el[0]*m_el[5]);

    m.m_el[6] = invDet*(m_el[3]*m_el[7] - m_el[4]*m_el[6]);
    m.m_el[7] = invDet*(m_el[1]*m_el[6] - m_el[0]*m_el[7]);
    m.m_el[8] = invDet*(m_el[0]*m_el[4] - m_el[1]*m_el[3]);

    return *this;
}// END FUNCTION Invert( void )


/*! Makes this matrix the transpose of itself
 *
 *  @returns        const icMatrix44&       Transpose of this matrix
**/
FORCE_INLINE const icMatrix33& icMatrix33::Transpose( void )
{
    icReal temp = m_el[1];
    m_el[1] = m_el[3];
    m_el[3] = temp;

    temp = m_el[2];
    m_el[2] = m_el[6];
    m_el[6] = temp;

    temp = m_el[5];
    m_el[5] = m_el[7];
    m_el[7] = temp;

    return *this;
}// END FUNCTION Transpose( void )


/*!    Returns the determinant of the matrix
 *
 *  @returns        const icReal        The determinant
**/
FORCE_INLINE const icReal icMatrix33::Determinant( void ) const
{
    return m_el[0]*(m_el[4]*m_el[8] - m_el[5]*m_el[7])
        + m_el[3]*(m_el[2]*m_el[7] - m_el[1]*m_el[8])
        + m_el[6]*(m_el[1]*m_el[5] - m_el[2]*m_el[4]);
}// END FUNCTION Determinant( void ) const


/*!    Returns the trace of the matrix
 *
 *  @returns        const icReal        The trace
**/
FORCE_INLINE const icReal icMatrix33::Trace( void ) const
{
    return m_el[0] + m_el[4] + m_el[8];
}// END FUNCTION Trace( void ) const


/*! Returns requested row
 *
 *    There is no range checking.
 *
 *    @param[in]    i                   Index to requested row
 *    @returns      const icVector3&    Reference to requested row
 *
**/
FORCE_INLINE icVector3 icMatrix33::GetRow( uchar i ) const
{
    assert(i<3);
    icVector3 v(m_el[i],m_el[3+i],m_el[6+i]);
    return v;
}// END FUNCTION GetRow( uint i)


/*! Returns requested column
 *
 *    There is no range checking.
 *
 *    @param[in]    i                   Index to requested column
 *    @returns      const icVector3&    Reference to requested column
 *
**/
FORCE_INLINE icVector3 icMatrix33::GetCol( uchar i ) const
{
    assert(i<3);
    icVector3 v(m_el[i*3],m_el[i*3+1],m_el[i*3+2]);
    return v;
}// END FUNCTION GetCol( uint i )


/*! Sets the columns of the matrix
 *
 *  @param          col1                Column 1
 *  @param          col2                Column 1
 *  @param          col3                Column 1
**/
FORCE_INLINE void icMatrix33::SetColumns( const icVector3& col1,
                                          const icVector3& col2,
                                          const icVector3& col3 )
{
    m_el[0] = col1.x;
    m_el[1] = col1.y;
    m_el[2] = col1.z;

    m_el[3] = col2.x;
    m_el[4] = col2.y;
    m_el[5] = col2.z;

    m_el[6] = col3.x;
    m_el[7] = col3.y;
    m_el[8] = col3.z;
}// END FUNCTION SetColumns( const icVector3& col1, const icVector3& col2,
 //                          const icVector3& col3 )


/*! Returns the element at [row,col]
 *
 *    There is no range checking.
 *
 *    @param[in]    row                Specifies the row
 *    @param[in]    col                Specifies the col
 *    @returns      const icReal       The element at [row,col]
 *
**/
FORCE_INLINE icReal& icMatrix33::operator()( uchar row, uchar col )
{
    return m_el[row + 3*col];
}// END FUNCTION operator()( uchar row, uchar col )


/*! Returns the element at [row,col]
 *
 *    There is no range checking.
 *
 *    @param[in]    row                Specifies the row
 *    @param[in]    col                Specifies the col
 *    @returns      const icReal       The element at [row,col]
 *
**/
FORCE_INLINE const icReal
icMatrix33::operator()( uchar row, uchar col ) const
{
    return m_el[row + 3*col];
}// END FUNCTION operator()( uchar row, uchar col ) const


/*! 1x3 * 3x3
 *
 *    @param        vector        Vector
 *    @param        matrix        Matrix
 *    @returns      icVector3     1x3 * 3x3 == 1x3
 *
**/
FORCE_INLINE icVector3 operator*( const icVector3& Lhs,
                                  const icMatrix33& Rhs )
{
    icVector3 v;
    v.x = Rhs.m_el[0]*Lhs.x+Rhs.m_el[1]*Lhs.y+Rhs.m_el[2]*Lhs.z;
    v.y = Rhs.m_el[3]*Lhs.x+Rhs.m_el[4]*Lhs.y+Rhs.m_el[5]*Lhs.z;
    v.z = Rhs.m_el[6]*Lhs.x+Rhs.m_el[7]*Lhs.y+Rhs.m_el[8]*Lhs.z;
    return v;
}// END FUNCTION operator*( const icVector3& vector,
 //                         const icMatrix33&  matrix ) const


/*! Overloaded operator, * Multiplies two matricies and returns new one
 *
 *    @param[in]    Rhs            Matrix on the right
 *    @returns      icMatrix33     The new matrix
 *
**/
FORCE_INLINE icMatrix33 icMatrix33::operator*( const icMatrix33& Rhs ) const
{
    icMatrix33 m;
    m(0,0)=m_el[0]*Rhs(0,0)+m_el[3]*Rhs(0,1)+m_el[6]*Rhs(0,2);
    m(0,1)=m_el[1]*Rhs(0,0)+m_el[4]*Rhs(0,1)+m_el[7]*Rhs(0,2);
    m(0,2)=m_el[2]*Rhs(0,0)+m_el[5]*Rhs(0,1)+m_el[8]*Rhs(0,2);

    m(1,0)=m_el[0]*Rhs(1,0)+m_el[3]*Rhs(1,1)+m_el[6]*Rhs(1,2);
    m(1,1)=m_el[1]*Rhs(1,0)+m_el[4]*Rhs(1,1)+m_el[7]*Rhs(1,2);
    m(1,2)=m_el[2]*Rhs(1,0)+m_el[5]*Rhs(1,1)+m_el[8]*Rhs(1,2);

    m(2,0)=m_el[0]*Rhs(2,0)+m_el[3]*Rhs(2,1)+m_el[6]*Rhs(1,2);
    m(2,1)=m_el[1]*Rhs(2,0)+m_el[4]*Rhs(2,1)+m_el[7]*Rhs(1,2);
    m(2,2)=m_el[2]*Rhs(2,0)+m_el[5]*Rhs(2,1)+m_el[8]*Rhs(1,2);
    return m;
}// END FUNCTION operator*( const icMatrix33& Rhs ) const


/*! Overloaded operator, *= Multiplies this matrix by the Rhs
 *
 *    @param[in]    Rhs                    Matrix on the right
 *    @returns      const icMatrix33&      The result
 *
**/
FORCE_INLINE const icMatrix33& icMatrix33::operator*=( const icMatrix33& Rhs)
{
    icMatrix33 m;
    m(0,0)=m_el[0]*Rhs(0,0)+m_el[3]*Rhs(0,1)+m_el[6]*Rhs(0,2);
    m(0,1)=m_el[1]*Rhs(0,0)+m_el[4]*Rhs(0,1)+m_el[7]*Rhs(0,2);
    m(0,2)=m_el[2]*Rhs(0,0)+m_el[5]*Rhs(0,1)+m_el[8]*Rhs(0,2);

    m(1,0)=m_el[0]*Rhs(1,0)+m_el[3]*Rhs(1,1)+m_el[6]*Rhs(1,2);
    m(1,1)=m_el[1]*Rhs(1,0)+m_el[4]*Rhs(1,1)+m_el[7]*Rhs(1,2);
    m(1,2)=m_el[2]*Rhs(1,0)+m_el[5]*Rhs(1,1)+m_el[8]*Rhs(1,2);

    m(2,0)=m_el[0]*Rhs(2,0)+m_el[3]*Rhs(2,1)+m_el[6]*Rhs(1,2);
    m(2,1)=m_el[1]*Rhs(2,0)+m_el[4]*Rhs(2,1)+m_el[7]*Rhs(1,2);
    m(2,2)=m_el[2]*Rhs(2,0)+m_el[5]*Rhs(2,1)+m_el[8]*Rhs(1,2);
    *this = m;
    return *this;
}// END FUNCTION operator*=( const icMatrix& Rhs )


/*! Overloaded assignment for quaternions
 *
 *    @param        Rhs                     Quaternion to convert to matrix33
 *    @returns      const icMatrix33&       const refernce to this
 *
**/
FORCE_INLINE const icMatrix33& icMatrix33::operator=( const icQuat& Rhs )
{
    float xs, ys, zs, wx, wy, wz, xx, xy, xz, yy, yz, zz;

    xs = Rhs.x+Rhs.x;
    ys = Rhs.y+Rhs.y;
    zs = Rhs.z+Rhs.z;
    wx = Rhs.w*xs;
    wy = Rhs.w*ys;
    wz = Rhs.w*zs;
    xx = Rhs.x*xs;
    xy = Rhs.x*ys;
    xz = Rhs.x*zs;
    yy = Rhs.y*ys;
    yz = Rhs.y*zs;
    zz = Rhs.z*zs;

    m_el[0] = 1.0f - (yy + zz);
    m_el[3] = xy - wz;
    m_el[6] = xz + wy;

    m_el[1] = xy + wz;
    m_el[4] = 1.0f - (xx + zz);
    m_el[7] = yz - wx;

    m_el[2] = xz - wy;
    m_el[5] = yz + wx;
    m_el[8] = 1.0f - (xx + yy);

    return *this;
}// END FUNCTION operator=( const icQuat& Rhs )


/* Cast overload, for implicit casting
 *
 *    @returns    const icReal*        Pointer to data
**/
FORCE_INLINE icMatrix33::operator icReal*( void )
{
    return m_el;
}// END FUNCTION operator icReal*( void )


/* Cast overload, for implicit casting
 *
 *    @returns    const icReal*        Pointer to data
**/
FORCE_INLINE icMatrix33::operator const icReal*( void ) const
{
    return m_el;
}// END FUNCTION operator const icReal*( void )

