
#include "Math/Matrix/icMatrix44.h"
#include "Math/Matrix/icMatrix33.h"
#include "Math/Vector/icQuat.h"
#include "Math/Vector/icVector3.h"
#include "Math/Vector/icVector4.h"

/*! Default Constructor
**/
FORCE_INLINE icMatrix44::icMatrix44( void )
{
}// END FUNCTION icMatrix44( void )


/*! Initializes the matrix with array data
**/
FORCE_INLINE icMatrix44::icMatrix44( const icReal* data )
{
    memcpy(m_el,data,sizeof(icReal)*16);
}// END FUNCTION icMatrix44( const icReal* data )

    
/*! quaternion + position constructor
**/
FORCE_INLINE icMatrix44::icMatrix44( const icQuat& rot, const icVector3& pos )
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
    m_el[4] = xy - wz;
    m_el[8] = xz + wy;

    m_el[1] = xy + wz;
    m_el[5] = 1.0f - (xx + zz);
    m_el[9] = yz - wx;

    m_el[2] = xz - wy;
    m_el[6] = yz + wx;
    m_el[10] = 1.0f - (xx + yy);

    m_el[3] = 0;
    m_el[7] = 0;
    m_el[11] = 0;
    m_el[12] = pos.x;
    m_el[13] = pos.y;
    m_el[14] = pos.z;
    m_el[15] = 1;
}// END FUNCTION icMatrix44( const icQuat& rot, const icVector3& pos )


/*! 3x3 rotation + position constructor
**/
FORCE_INLINE
icMatrix44::icMatrix44( const icMatrix33& rot, const icVector3& pos )
{
    m_el[0] = rot[0];
    m_el[1] = rot[1];
    m_el[2] = rot[2];
    m_el[3] = 0;

    m_el[4] = rot[3];
    m_el[5] = rot[4];
    m_el[6] = rot[5];
    m_el[7] = 0;

    m_el[8] = rot[6];
    m_el[9] = rot[7];
    m_el[10] = rot[8];
    m_el[11] = 0;

    m_el[12] = pos.x;
    m_el[13] = pos.y;
    m_el[14] = pos.z;
    m_el[15] = 1;
}// END FUNCTION icMatrix44( const icMatrix33& rot, const icVector3& pos )


/*! Makes the matrix the identity
**/
FORCE_INLINE void icMatrix44::Identity()
{
    m_el[0] = 1.0f; m_el[1] = 0.0f; m_el[2] = 0.0f; m_el[3] = 0.0f;
    m_el[4] = 0.0f; m_el[5] = 1.0f; m_el[6] = 0.0f; m_el[7] = 0.0f;
    m_el[8] = 0.0f; m_el[9] = 0.0f; m_el[10] = 1.0f; m_el[11] = 0.0f;
    m_el[12] = 0.0f; m_el[13] = 0.0f; m_el[14] = 0.0f; m_el[15] = 1.0f;
}// END FUNCTION Identity()


/*! Rotates this matrix theta radians about the X axis
 *
 *    @param[in]    theta    Angle to rotate about the X axis
**/
FORCE_INLINE void icMatrix44::RotX( const icReal theta )
{
    icMatrix44 m;
    m.MakeRotX(theta);
    *this *= m;
    //icReal c = icCos( theta );
    //icReal s = icSin( theta );
    //icReal t4 = m_el[5];
    //icReal t5 = m_el[6];
    //m_el[5] = m_el[5] * c + m_el[9] * s;
    //m_el[6] = m_el[6] * c + m_el[10] * s;

    //m_el[9] = t4 * -s + m_el[9] * c;
    //m_el[10] = t5 * -s + m_el[10] * c;
}// END FUCNTION RotX( const icReal theta )


/*! Rotates this matrix theta radians about the Y axis
 *
 *    @param[in]    theta    Angle to rotate about the Y axis
**/
FORCE_INLINE void icMatrix44::RotY( const icReal theta )
{
    icMatrix44 m;
    m.MakeRotY(theta);
    *this *= m;
    //icReal c = icCos( theta );
    //icReal s = icSin( theta );
    //icReal t0 = m_el[0];
    //icReal t2 = m_el[2];
    //m_el[0] = m_el[0] * c - m_el[8] * s;
    //m_el[2] = m_el[2] * c - m_el[10] * s;

    //m_el[8] = t0 * s + m_el[8] * c;
    //m_el[10] = t2 * s + m_el[10] * c;
}// END FUNCTION RotY( const icReal theta )


/*! Rotates this matrix theta radians about the Z axis
 *
 *    @param[in]    theta    Angle to rotate about the Z axis
**/
FORCE_INLINE void icMatrix44::RotZ( const icReal theta )
{
    icMatrix44 m;
    m.MakeRotZ(theta);
    *this *= m;
    //icReal c = icCos( theta );
    //icReal s = icSin( theta );
    //icReal t0 = m_el[0];
    //icReal t1 = m_el[1];
    //m_el[0] = m_el[0] * c + m_el[4] * s;
    //m_el[1] = m_el[1] * c + m_el[5] * s;

    //m_el[4] = t0 * -s + m_el[4] * c;
    //m_el[5] = t1 * -s + m_el[5] * c;
}// END FUNCTION RotZ( const icReal theta )


/*! Multiplies this matrix by another
 *
 *  @param          Rhs         Matrix to apply to this one
**/
FORCE_INLINE void icMatrix44::Rotate( const icMatrix44& Rhs )
{
    icMatrix44 m;

    m.m_el[0] = m_el[0]*Rhs.m_el[0] + m_el[4]*Rhs.m_el[1] + 
                m_el[8]*Rhs.m_el[2] + m_el[12]*Rhs.m_el[3];
    m.m_el[1] = m_el[1]*Rhs.m_el[0] + m_el[5]*Rhs.m_el[1] + 
                m_el[9]*Rhs.m_el[2] + m_el[13]*Rhs.m_el[3];
    m.m_el[2] = m_el[2]*Rhs.m_el[0] + m_el[6]*Rhs.m_el[1] + 
                m_el[10]*Rhs.m_el[2] + m_el[14]*Rhs.m_el[3];
    m.m_el[3] = m_el[3]*Rhs.m_el[0] + m_el[7]*Rhs.m_el[1] + 
                m_el[11]*Rhs.m_el[2] + m_el[15]*Rhs.m_el[3];

    m.m_el[4] = m_el[0]*Rhs.m_el[4] + m_el[4]*Rhs.m_el[5] + 
                m_el[8]*Rhs.m_el[6] + m_el[12]*Rhs.m_el[7];
    m.m_el[5] = m_el[1]*Rhs.m_el[4] + m_el[5]*Rhs.m_el[5] + 
                m_el[9]*Rhs.m_el[6] + m_el[13]*Rhs.m_el[7];
    m.m_el[6] = m_el[2]*Rhs.m_el[4] + m_el[6]*Rhs.m_el[5] + 
                m_el[10]*Rhs.m_el[6] + m_el[14]*Rhs.m_el[7];
    m.m_el[7] = m_el[3]*Rhs.m_el[4] + m_el[7]*Rhs.m_el[5] + 
                m_el[11]*Rhs.m_el[6] + m_el[15]*Rhs.m_el[7];

    m.m_el[8] = m_el[0]*Rhs.m_el[8] + m_el[4]*Rhs.m_el[9] + 
                m_el[8]*Rhs.m_el[10] + m_el[12]*Rhs.m_el[11];
    m.m_el[9] = m_el[1]*Rhs.m_el[8] + m_el[5]*Rhs.m_el[9] + 
                m_el[9]*Rhs.m_el[10] + m_el[13]*Rhs.m_el[11];
    m.m_el[10] = m_el[2]*Rhs.m_el[8] + m_el[6]*Rhs.m_el[9] + 
                 m_el[10]*Rhs.m_el[10] + m_el[14]*Rhs.m_el[11];
    m.m_el[11] = m_el[3]*Rhs.m_el[8] + m_el[7]*Rhs.m_el[9] + 
                 m_el[11]*Rhs.m_el[10] + m_el[15]*Rhs.m_el[11];

    m.m_el[12] = m_el[0]*Rhs.m_el[12] + m_el[4]*Rhs.m_el[13] + 
                 m_el[8]*Rhs.m_el[14] + m_el[12]*Rhs.m_el[15];
    m.m_el[13] = m_el[1]*Rhs.m_el[12] + m_el[5]*Rhs.m_el[13] + 
                 m_el[9]*Rhs.m_el[14] + m_el[13]*Rhs.m_el[15];
    m.m_el[14] = m_el[2]*Rhs.m_el[12] + m_el[6]*Rhs.m_el[13] + 
                 m_el[10]*Rhs.m_el[14] + m_el[14]*Rhs.m_el[15];
    m.m_el[15] = m_el[3]*Rhs.m_el[12] + m_el[7]*Rhs.m_el[13] + 
                 m_el[11]*Rhs.m_el[14] + m_el[15]*Rhs.m_el[15];

    *this = m;
}// END FUNCTION Rotate( const icMatrix44& Rhs )


/*! Makes this a roatation matrix, rotated around the x-axis
 *
 *  @param          theta       Angle in radians
**/
FORCE_INLINE const icMatrix44& icMatrix44::MakeRotX( const icReal theta )
{
    icReal s = icSin(theta);
    icReal c = icCos(theta);

    m_el[0] = 1.0f;
    m_el[1] = 0.0f;
    m_el[2] = 0.0f;
    m_el[3] = 0.0f;
    m_el[4] = 0.0f;
    m_el[5] = c;
    m_el[6] = s;
    m_el[7] = 0.0f;
    m_el[8] = 0.0f;
    m_el[9] = -s;
    m_el[10] = c;
    m_el[11] = 0.0f;
    m_el[12] = 0.0f;
    m_el[13] = 0.0f;
    m_el[14] = 0.0f;
    m_el[15] = 1.0f;

    return *this;
}// END FUNCTION MakeRotX( const icReal theta )


/*! Makes this a roatation matrix, rotated around the y-axis
 *
 *  @param          theta       Angle in radians
**/
FORCE_INLINE const icMatrix44& icMatrix44::MakeRotY( const icReal theta )
{
    icReal s = icSin(theta);
    icReal c = icCos(theta);

    m_el[0] = c;
    m_el[1] = 0.0f;
    m_el[2] = -s;
    m_el[3] = 0.0f;
    m_el[4] = 0.0f;
    m_el[5] = 1.0f;
    m_el[6] = 0.0f;
    m_el[7] = 0.0f;
    m_el[8] = s;
    m_el[9] = 0.0f;
    m_el[10] = c;
    m_el[11] = 0.0f;
    m_el[12] = 0.0f;
    m_el[13] = 0.0f;
    m_el[14] = 0.0f;
    m_el[15] = 1.0f;    

    return *this;
}// END FUNCTION MakeRotY( const icReal theta )


/*! Makes this a roatation matrix, rotated around the z-axis
 *
 *  @param          theta       Angle in radians
**/
FORCE_INLINE const icMatrix44& icMatrix44::MakeRotZ( const icReal theta )
{
    icReal s = icSin(theta);
    icReal c = icCos(theta);

    m_el[0] = c;
    m_el[1] = s;
    m_el[2] = 0.0f;
    m_el[3] = 0.0f;
    m_el[4] = -s;
    m_el[5] = c;
    m_el[6] = 0.0f;
    m_el[7] = 0.0f;
    m_el[8] = 0.0f;
    m_el[9] = 0.0f;
    m_el[10] = 1.0f;
    m_el[11] = 0.0f;
    m_el[12] = 0.0f;
    m_el[13] = 0.0f;
    m_el[14] = 0.0f;
    m_el[15] = 1.0f;

    return *this;
}// END FUNCTION MakeRotZ( const icReal theta )


/*! Sets the rotation part of this matrix to the given 3x3
 *
 *  @param          rot                 Rotation matrix
 *  @returns        const icMatrix44&   *(this)
**/
FORCE_INLINE const icMatrix44& icMatrix44::SetRotation(const icMatrix33& rot)
{
    m_el[0] = rot(0,0);
    m_el[1] = rot(0,1);
    m_el[2] = rot(0,2);

    m_el[4] = rot(1,0);
    m_el[5] = rot(1,1);
    m_el[6] = rot(1,2);

    m_el[8] = rot(2,0);
    m_el[9] = rot(2,1);
    m_el[10] = rot(2,2);

    return *this;
}// END FUNCTION SetRotation( const icMatrix33& rot )


/*! Sets the rotation part of this matrix to the rotation of the given quat
 *
 *  @param          rot                 Quaternion
 *  @returns        const icMatrix44&   *(this)
**/
FORCE_INLINE const icMatrix44& icMatrix44::SetRotation( const icQuat& rot )
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
    m_el[4] = xy - wz;
    m_el[8] = xz + wy;

    m_el[1] = xy + wz;
    m_el[5] = 1.0f - (xx + zz);
    m_el[9] = yz - wx;

    m_el[2] = xz - wy;
    m_el[6] = yz + wx;
    m_el[10] = 1.0f - (xx + yy);

    return *this;
}// END FUNCTION SetRotation( const icQuat& rot )


/*! Scales the upper 3x3 portion of the matrix
 *
 *  @param          Rhs         Vector to scale the matrix by
**/
FORCE_INLINE const icMatrix44& icMatrix44::Scale( const icVector3& Rhs )
{
    m_el[0] *= Rhs.x;
    m_el[5] *= Rhs.y;
    m_el[10] *= Rhs.z;

    return *this;
}// END FUNCTION Scale( const icVector3& Rhs )


/*! Scales the upper 3x3 portion of the matrix
 *
 *  @param          x           Amount to scale in X
 *  @param          y           Amount to scale in Y
 *  @param          z           Amount to scale in Z
**/
FORCE_INLINE const icMatrix44& icMatrix44::Scale(icReal x, icReal y, icReal z)
{
    m_el[0] *= x;
    m_el[5] *= y;
    m_el[10] *= z;

    return *this;
}// Scale( icReal x, icReal y, icReal z )


/*! Returns the inverse of this matrix (does not modify)
 *
 *  @returns        icMatrix44             The inverse of this matrix
**/
FORCE_INLINE icMatrix44 icMatrix44::Inverse( void ) const
{
    icMatrix44 m;
    
    // compute upper left 3x3 matrix determinant
    icReal cofactor0 = m_el[5]*m_el[10] - m_el[6]*m_el[9];
    icReal cofactor4 = m_el[2]*m_el[9] - m_el[1]*m_el[10];
    icReal cofactor8 = m_el[1]*m_el[6] - m_el[2]*m_el[5];
    icReal det = m_el[0]*cofactor0 + m_el[4]*cofactor4 + m_el[8]*cofactor8;
    if (icIsZero( det ))
    {
        icWarning("icMatrix44::Invert() -- singular matrix");
        return *this;
    }

    // create adjunct matrix and multiply by 1/det to get upper 3x3
    icReal invDet = 1.0f/det;
    m.m_el[0] = invDet*cofactor0;
    m.m_el[1] = invDet*cofactor4;
    m.m_el[2] = invDet*cofactor8;
    m.m_el[3] = 0;
   
    m.m_el[4] = invDet*(m_el[6]*m_el[8] - m_el[4]*m_el[10]);
    m.m_el[5] = invDet*(m_el[0]*m_el[10] - m_el[2]*m_el[8]);
    m.m_el[6] = invDet*(m_el[2]*m_el[4] - m_el[0]*m_el[6]);
    m.m_el[7] = 0;

    m.m_el[8] = invDet*(m_el[4]*m_el[9] - m_el[5]*m_el[8]);
    m.m_el[9] = invDet*(m_el[1]*m_el[8] - m_el[0]*m_el[9]);
    m.m_el[10] = invDet*(m_el[0]*m_el[5] - m_el[1]*m_el[4]);
    m.m_el[11] = 0;

    // multiply -translation by inverted 3x3 to get its inverse
    
    m.m_el[12] = -m.m_el[0]*m_el[12] - m.m_el[4]*m_el[13] - m.m_el[8]*m_el[14];
    m.m_el[13] = -m.m_el[1]*m_el[12] - m.m_el[5]*m_el[13] - m.m_el[9]*m_el[14];
    m.m_el[14] = -m.m_el[2]*m_el[12] - m.m_el[6]*m_el[13] - m.m_el[10]*m_el[14];
    m.m_el[15] = 1;

    return m;
}// END FUNCTION Inverse( void )


/*! Calculates the affine inverse of this matrix
 *
 *  @returns        const icMatrix44&       The inverse of this matrix
**/
FORCE_INLINE const icMatrix44& icMatrix44::Invert( void )
{
    icMatrix44 m;
    *this = this->Inverse();
    return *this;
}// END FUNCTION Invert( void )


/*! Makes this matrix the transpose of itself
 *
 *  @returns        const icMatrix44&       Transpose of this matrix
**/
FORCE_INLINE const icMatrix44& icMatrix44::Transpose( void )
{
    icReal temp = m_el[1];
    m_el[1] = m_el[4];
    m_el[4] = temp;

    temp = m_el[2];
    m_el[2] = m_el[8];
    m_el[8] = temp;

    temp = m_el[3];
    m_el[2] = m_el[12];
    m_el[12] = temp;

    temp = m_el[6];
    m_el[6] = m_el[9];
    m_el[9] = temp;

    temp = m_el[7];
    m_el[7] = m_el[13];
    m_el[13] = temp;

    temp = m_el[11];
    m_el[11] = m_el[14];
    m_el[14] = temp;

    return *this;
}// END FUNCTION Transpose( void )


/*! Sets the position of the transform
 *
 *  @param          pos         Position
**/
FORCE_INLINE void icMatrix44::SetPos( const icVector3 pos )
{
    m_el[12] = pos.x;
    m_el[13] = pos.y;
    m_el[14] = pos.z;
}// END FUNCTION SetPos( const icVector3 pos )


/*! Sets the position of the transform
 *
 *  @param          x           X-Position
 *  @param          y           Y-Position
 *  @param          z           X-Position
**/
FORCE_INLINE void icMatrix44::SetPos( icReal x, icReal y, icReal z )
{
    m_el[12] = x;
    m_el[13] = y;
    m_el[14] = z;
}// END FUNCTION SetPos( icReal x, icReal y, icReal z )


/*! Translates the transform
 *
 *  @param          translation         Amount to translate
 *  @returns        const icMatrix44&   Translated matrix
**/
FORCE_INLINE
const icMatrix44& icMatrix44::Translate( const icVector3& translation )
{
    m_el[12] += translation.x;
    m_el[13] += translation.y;
    m_el[14] += translation.z;
    return *this;
}// END FUNCTION Translate( const icVector3& translation )


/*! Translates the transform
 *
 *  @param          x                   Amount to translate in X
 *  @param          y                   Amount to translate in Y
 *  @param          z                   Amount to translate in Z
 *  @returns        const icMatrix44&   Translated matrix
**/
FORCE_INLINE
const icMatrix44& icMatrix44::Translate( icReal x, icReal y, icReal z )
{
    icVector3 v = this->Transform(x,y,z);
    m_el[12] += v.x;
    m_el[13] += v.y;
    m_el[14] += v.z;
    return *this;
}// END FUNCTION Translate( icReal x, icReal y, icReal z )


/*! Makes this transform a translation matrix
 *
 *  @param          translation         Amount to translate
 *  @returns        const icMatrix44&   Translated matrix
**/
FORCE_INLINE
const icMatrix44& icMatrix44::MakeTranslation( const icVector3& translation )
{
    m_el[0] = 1;
    m_el[1] = 0;
    m_el[2] = 0;
    m_el[3] = 0;

    m_el[4] = 0;
    m_el[5] = 1;
    m_el[6] = 0;
    m_el[7] = 0;

    m_el[8] = 0;
    m_el[9] = 0;
    m_el[10] = 1;
    m_el[11] = 0;

    m_el[12] = translation.x;
    m_el[13] = translation.y;
    m_el[14] = translation.z;
    m_el[15] = 1;
    return *this;
}// END FUNCTION MakeTranslation( const icVector3& translation )


/*! Makes this transform a translation matrix
 *
 *  @param          x                   Amount to translate in X
 *  @param          y                   Amount to translate in Y
 *  @param          z                   Amount to translate in Z
 *  @returns        const icMatrix44&   Translated matrix
**/
FORCE_INLINE
const icMatrix44& icMatrix44::MakeTranslation( icReal x, icReal y, icReal z )
{
    m_el[0] = 1;
    m_el[1] = 0;
    m_el[2] = 0;
    m_el[3] = 0;

    m_el[4] = 0;
    m_el[5] = 1;
    m_el[6] = 0;
    m_el[7] = 0;

    m_el[8] = 0;
    m_el[9] = 0;
    m_el[10] = 1;
    m_el[11] = 0;

    m_el[12] = x;
    m_el[13] = y;
    m_el[14] = z;
    m_el[15] = 1;
    return *this;
}// END FUNCTION MakeTranslation( icReal x, icReal y, icReal z )


/*! Gets the position of the transform
 *
 *  @param[out]     pos                 Position of the matrix stored here
**/
FORCE_INLINE void icMatrix44::GetPos( icVector3& pos )
{
    pos.x = m_el[12];
    pos.y = m_el[12];
    pos.z = m_el[12];
}// END FUNCTION GetPos( icVector3& pos )


/*! Gets the position of the transform
 *
 *  @returns        icVector3&          Reference to transforms position
**/
FORCE_INLINE icVector3& icMatrix44::GetPos( void )
{
    icVector3* p = (icVector3*)&m_el[12];
    return *p;
}// END FUNCTION GetPos( void )


/*! Gets the position of the transform
 *
 *  @returns        const icVector3&    Reference to transforms position
**/
FORCE_INLINE const icVector3& icMatrix44::GetPos( void ) const
{
    const icVector3* p = (icVector3*)&m_el[12];
    return *p;
}// END FUNCTION GetPos( void ) const


/*! Gets the transformed position of this matrix
 *
 *  @returns        icVector3       Transformed position
**/
FORCE_INLINE icVector3 icMatrix44::GetTransformedPos( void )
{
    icVector3 v;
    v.x = m_el[0]*m_el[12] + m_el[4]*m_el[13] + m_el[8]*m_el[14] + m_el[12];
    v.y = m_el[1]*m_el[12] + m_el[5]*m_el[13] + m_el[9]*m_el[14] + m_el[13];
    v.z = m_el[2]*m_el[12] + m_el[6]*m_el[13] + m_el[10]*m_el[14] + m_el[14];
    return v;
}// END FUNCTION GetTransformedPos( void )


/*! Transforms the given vector into local cooridinate space
 *
 *  @param          x              X-component
 *  @param          y              Y-component
 *  @param          z              Z-component
 *  @returns        icVector3       Transformed vector
**/
FORCE_INLINE icVector3 icMatrix44::Transform( icReal x, icReal y, icReal z) const
{
    icVector3 v;
    v.x = m_el[0]*x + m_el[4]*y + m_el[8]*z;
    v.y = m_el[1]*x + m_el[5]*y + m_el[9]*z;
    v.z = m_el[2]*x + m_el[6]*y + m_el[10]*z;
    return v;
}// END FUNCTION Transform( icReal x, icReal y, icReal z)


/*! Transforms the given vector into local cooridinate space
 *
 *  @param          vect            Vector to transform
 *  @returns        icVector3       Transformed vector
**/
FORCE_INLINE icVector3 icMatrix44::TransformVect( const icVector3& vect ) const
{
    icVector3 v;
    v.x = m_el[0]*vect.x + m_el[4]*vect.y + m_el[8]*vect.z;
    v.y = m_el[1]*vect.x + m_el[5]*vect.y + m_el[9]*vect.z;
    v.z = m_el[2]*vect.x + m_el[6]*vect.y + m_el[10]*vect.z;
    return v;
}// END FUNCTION TransformVect( const icVector3& vect ) const


/*! Creates an orthographics matrix with 0,0 at center of the screen
 *
 *  @param          width           Width of the projection frame
 *  @param          height          Height of the projection frame
 *  @param          nZ              Near clip plane
 *  @param          fZ              Far clip plane
 *  @param[in,out]  outMat          Place to store orthographic matrix
**/
FORCE_INLINE void icCreateOrtho(const icReal width, const icReal height,
                                const icReal nZ, const icReal fZ,
                                icMatrix44& outMat )
{
    outMat.m_el[0] = icReal(2)/width;
    outMat.m_el[1] = 0;
    outMat.m_el[2] = 0;
    outMat.m_el[3] = 0;
    outMat.m_el[4] = 0;
    outMat.m_el[5] = icReal(2)/height;
    outMat.m_el[6] = 0;
    outMat.m_el[7] = 0;
    outMat.m_el[8] = 0;
    outMat.m_el[9] = 0;
    outMat.m_el[10] = icReal(1)/(fZ - nZ);
    outMat.m_el[11] = 0;
    outMat.m_el[12] = 0;
    outMat.m_el[13] = 0;
    outMat.m_el[14] = -nZ/(fZ - nZ);
    outMat.m_el[15] = 1;
}// END FUNCTION icCreateOrtho(const icReal width, const icReal height,
 //                            const icReal nZ, const icReal fZ,
 //                            icMatrix44& outMat )


/*! Creates an orthographics matrix with 0,0 at center of the screen
 *
 *  @param          left            Left most value of the projection frame
 *  @param          right           Right most value of the projection frame
 *  @param          bottom          Bottom most value of the projection frame
 *  @param          top             Top most value of the projection frame
 *  @param          nZ              Near clip plane
 *  @param          fZ              Far clip plane
 *  @param[in,out]  outMat          Place to store orthographic matrix
**/
FORCE_INLINE void icCreateOrthoOffset(const icReal left, const icReal right,
                                      const icReal bottom, const icReal top,
                                      const icReal nZ, const icReal fZ,
                                      icMatrix44& outMat)
                                      
{
    outMat.m_el[0] = icReal(2)/(right - left);
    outMat.m_el[1] = 0;
    outMat.m_el[2] = 0;
    outMat.m_el[3] = 0;
    outMat.m_el[4] = 0;
    outMat.m_el[5] = icReal(2)/(top - bottom);
    outMat.m_el[6] = 0;
    outMat.m_el[7] = 0;
    outMat.m_el[8] = 0;
    outMat.m_el[9] = 0;
    outMat.m_el[10] = icReal(1) / (fZ - nZ);
    outMat.m_el[11] = 0;
    outMat.m_el[12] = -(right+left)/(right-left);
    outMat.m_el[13] = -(top+bottom)/(top-bottom);
    outMat.m_el[14] = -nZ/(fZ - nZ);
    outMat.m_el[15] = 1;
}// END FUNCTION icCreateOrthoOffset(const icReal left, const icReal right,
 //                                  const icReal bottom, const icReal top,
 //                                  const icReal nZ, const icReal fZ,
 //                                  icMatrix44& outMat)



/*! Creates an orthographics matrix with 0,0 at center of the screen
 *
 *  @param          fov             Field of view (radians)
 *  @param          ar              Aspect Ratio
 *  @param          nZ              Near clip plane
 *  @param          fZ              Far clip plane
 *  @param[in,out]  outMat          Place to store orthographic matrix
**/
FORCE_INLINE void icCreatePerspective(const icReal fov, const icReal ar,
                                      const icReal nZ, const icReal fZ,
                                      icMatrix44& outMat)
{
    icReal cot = icReal(1)/icTan(fov/icReal(2));
    //icReal cot = icReal(1)/icTan(fov/icReal(0.5f));
    outMat.m_el[0] = cot/ar;
    outMat.m_el[1] = 0;
    outMat.m_el[2] = 0;
    outMat.m_el[3] = 0;
    outMat.m_el[4] = 0;
    outMat.m_el[5] = cot;
    outMat.m_el[6] = 0;
    outMat.m_el[7] = 0;
    outMat.m_el[8] = 0;
    outMat.m_el[9] = 0;
    outMat.m_el[10] = fZ/(fZ - nZ);
    outMat.m_el[11] = 1;
    outMat.m_el[12] = 0;
    outMat.m_el[13] = 0;
    outMat.m_el[14] = -(nZ * fZ)/(fZ - nZ);
    //outMat.m_el[10] = (nZ + fZ)/(nZ - fZ);
    //outMat.m_el[11] = -1;
    //outMat.m_el[12] = 0;
    //outMat.m_el[13] = 0;
    //outMat.m_el[14] = (2.0f * nZ * fZ)/(nZ - fZ);
    outMat.m_el[15] = 0;
}// END FUNCTION icCreatePerspective(const icReal fov, const icReal ar,
 //                                  const icReal nZ, const icReal fZ,
 //                                  icMatrix44& outMat)


/*! Creates a matrix frame, looking at target from pos
 *
 *  @param          pos         Position to look from
 *  @param          target      Position to look at
 *  @param          up          Up vector
**/
FORCE_INLINE void icCreateLookAt(const icVector3& pos, const icVector3& target,
                                 const icVector3& up, icMatrix44& outMat)
{
    icVector3 at = (target - pos);
    at.Normalize();

    icVector3 right = Cross(up,at);
    right.Normalize();

    icVector3 loc_up = Cross(at,right);
    loc_up.Normalize();

    outMat.m_el[0] = right.x;
    outMat.m_el[1] = right.y;
    outMat.m_el[2] = right.z;

    outMat.m_el[4] = loc_up.x;
    outMat.m_el[5] = loc_up.y;
    outMat.m_el[6] = loc_up.z;

    outMat.m_el[8] = at.x;
    outMat.m_el[9] = at.y;
    outMat.m_el[10] = at.z; 

    outMat.m_el[3] = 0;
    outMat.m_el[7] = 0;
    outMat.m_el[11] = 0;

    outMat.m_el[12] = pos.x;
    outMat.m_el[13] = pos.y;
    outMat.m_el[14] = pos.z;
    outMat.m_el[15] = 1;
}// END FUNCTION icCreateLookAt(const icVector3& pos, const icVector3& target,
 //                             const icVector3& up)


/*! Creates a matrix frame, looking in dir from pos
 *
 *  @param          pos         Position to look from
 *  @param          dir         Direction to look
 *  @param          up          Up vector
**/
FORCE_INLINE void icCreateLookDir(const icVector3& pos, const icVector3& dir,
                                  const icVector3& up, icMatrix44& outMat)
{
    icVector3 right = Cross(up,dir);

    outMat.m_el[0] = right.x;
    outMat.m_el[1] = right.y;
    outMat.m_el[2] = right.z;

    outMat.m_el[4] = up.x;
    outMat.m_el[5] = up.y;
    outMat.m_el[6] = up.z;

    outMat.m_el[8] = dir.x;
    outMat.m_el[9] = dir.y;
    outMat.m_el[10] = dir.z;


    outMat.m_el[3] = 0;
    outMat.m_el[7] = 0;
    outMat.m_el[11] = 0;

    outMat.m_el[12] = pos.x;
    outMat.m_el[13] = pos.y;
    outMat.m_el[14] = pos.z;
    outMat.m_el[15] = 1;
}// END FUNCTION icCreateLookDir(const icVector3& pos, const icVector3& dir,
 //                              const icVector3& up)


/*! Returns requested row
 *
 *    @param[in]    i                    Index to requested row
 *    @returns      const icVector4&     Reference to requested row
**/
FORCE_INLINE icVector4 icMatrix44::GetRow(uchar i) const
{
    assert(i<4);
    icVector4 v(m_el[i],m_el[i+4],m_el[i+8],m_el[i+12]);
    return v;
}// END FUNCTION GetRow(uchar i)


/*! Returns requested column
 *
 *    @param[in]    i                   Index to requested column
 *    @returns      const icVector4&    Reference to requested column
**/
FORCE_INLINE icVector4 icMatrix44::GetCol(uchar i) const
{
    assert(i<4);
    icVector4 v(m_el[4*i],m_el[4*i+1],m_el[4*i+2],m_el[4*i+3]);
    return v;
}// END FUNCTION GetCol(uchar i)


/*! Returns the element at [row,col]
 *
 *    There is no range checking.
 *
 *    @param[in]    row                 Specifies the row
 *    @param[in]    col                 Specifies the col
 *    @returns      const icReal        The element at [row,col]
**/
FORCE_INLINE icReal& icMatrix44::operator()(uchar row, uchar col)
{
    return m_el[row + col*4];
}// END FUNCTION operator()(uchar row, uchar col)


/*! Returns the element at [row,col]
 *
 *    There is no range checking.
 *
 *    @param[in]    row                 Specifies the row
 *    @param[in]    col                 Specifies the col
 *    @returns      const icReal        The element at [row,col]
**/
FORCE_INLINE const icReal
icMatrix44::operator()(uchar row, uchar col) const
{
    return m_el[row + col*4];
}// END FUNCTION operator()(uchar row, uchar col) const


/*! Multiplies a matrix by a vector
 *
 *      The 4x4 matrix is considered a 3x4 for this operation (rot+pos)
 *
 *  @param          Rhs                 The vector on the right of the *
 *  @returns        icVector3           The result of the muliplication
**/
FORCE_INLINE icVector3 icMatrix44::operator*( const icVector3& Rhs ) const
{
    icVector3 v;
    v.x = m_el[0]*Rhs.x + m_el[4]*Rhs.y + m_el[8]*Rhs.z;
    v.y = m_el[1]*Rhs.x + m_el[5]*Rhs.y + m_el[9]*Rhs.z;
    v.z = m_el[2]*Rhs.x + m_el[6]*Rhs.y + m_el[10]*Rhs.z;
    return v;
}// END FUNCTION operator*( const icVector3& Rhs ) const


/*! Multiplies a vector by a matrix
 *
 *      The 4x4 matrix is considered a 3x4 for this operation (rot+pos)
 *
 *  @param          Lhs                 The vector on the left of the *
 *  @param          Rhs                 The matrix on the right of the *
 *  @returns        icVector3           The result of the muliplication
**/
FORCE_INLINE icVector3 operator*( const icVector3& Lhs, const icMatrix44& Rhs )
{
    icVector3 v;
    v.x = Rhs.m_el[0]*Lhs.x + Rhs.m_el[4]*Lhs.y + Rhs.m_el[8]*Lhs.z;
    v.y = Rhs.m_el[1]*Lhs.x + Rhs.m_el[5]*Lhs.y + Rhs.m_el[9]*Lhs.z;
    v.z = Rhs.m_el[2]*Lhs.x + Rhs.m_el[6]*Lhs.y + Rhs.m_el[10]*Lhs.z;
    return v;
}// END FUNCTION operator*( const icVector3& Lhs, const icMatrix44& Rhs )


/*! Multiplies a vector by a matrix
 *
 *  @param          Rhs                 The vector on the right of the *
 *  @returns        icVector4           The result of the muliplication
**/
FORCE_INLINE icVector4 icMatrix44::operator*( const icVector4& Rhs ) const
{
    icVector4 v;
    v.x = m_el[0]*Rhs.x + m_el[4]*Rhs.y + m_el[8]*Rhs.z + m_el[12]*Rhs.w;
    v.y = m_el[1]*Rhs.x + m_el[5]*Rhs.y + m_el[9]*Rhs.z + m_el[13]*Rhs.w;
    v.z = m_el[2]*Rhs.x + m_el[6]*Rhs.y + m_el[10]*Rhs.z + m_el[14]*Rhs.w;
    v.w = m_el[3]*Rhs.x + m_el[7]*Rhs.y + m_el[11]*Rhs.z + m_el[15]*Rhs.w;
    return v;
}// END FUNCTION operator*( const icVector4& Rhs ) const


/*! Multiplies a vector by a matrix
 *
 *  @param          Lhs                 The vector on the left of the *
 *  @param          Rhs                 The matrix on the right of the *
 *  @returns        icVector4           The result of the muliplication
**/
FORCE_INLINE icVector4 operator*(const icVector4& Lhs, const icMatrix44& Rhs)
{
    icVector4 v;

    v.x = Rhs.m_el[0]*Lhs.x + Rhs.m_el[4]*Lhs.y + 
          Rhs.m_el[8]*Lhs.z + Rhs.m_el[12]*Lhs.w;

    v.y = Rhs.m_el[1]*Lhs.x + Rhs.m_el[5]*Lhs.y + 
          Rhs.m_el[9]*Lhs.z + Rhs.m_el[13]*Lhs.w;

    v.z = Rhs.m_el[2]*Lhs.x + Rhs.m_el[6]*Lhs.y + 
          Rhs.m_el[10]*Lhs.z + Rhs.m_el[14]*Lhs.w;

    v.w = Rhs.m_el[3]*Lhs.x + Rhs.m_el[7]*Lhs.y + 
          Rhs.m_el[11]*Lhs.z + Rhs.m_el[15]*Lhs.w;

    return v;
}// END FUNCTION operator*( const icVector4& Lhs, const icMatrix44& Rhs )


/*! Multiplies two matricies together
 *
 *  @param          Lhs                 The matrix on the left of the *
 *  @param          Rhs                 The matrix on the right of the *
 *  @returns        icMatrix44          The result of the muliplication
**/
FORCE_INLINE icMatrix44 icMatrix44::operator*( const icMatrix44& Rhs ) const
{
    icMatrix44 m;

    m.m_el[0] = m_el[0]*Rhs.m_el[0] + m_el[4]*Rhs.m_el[1] + 
                m_el[8]*Rhs.m_el[2] + m_el[12]*Rhs.m_el[3];
    m.m_el[1] = m_el[1]*Rhs.m_el[0] + m_el[5]*Rhs.m_el[1] + 
                m_el[9]*Rhs.m_el[2] + m_el[13]*Rhs.m_el[3];
    m.m_el[2] = m_el[2]*Rhs.m_el[0] + m_el[6]*Rhs.m_el[1] + 
                m_el[10]*Rhs.m_el[2] + m_el[14]*Rhs.m_el[3];
    m.m_el[3] = m_el[3]*Rhs.m_el[0] + m_el[7]*Rhs.m_el[1] + 
                m_el[11]*Rhs.m_el[2] + m_el[15]*Rhs.m_el[3];

    m.m_el[4] = m_el[0]*Rhs.m_el[4] + m_el[4]*Rhs.m_el[5] + 
                m_el[8]*Rhs.m_el[6] + m_el[12]*Rhs.m_el[7];
    m.m_el[5] = m_el[1]*Rhs.m_el[4] + m_el[5]*Rhs.m_el[5] + 
                m_el[9]*Rhs.m_el[6] + m_el[13]*Rhs.m_el[7];
    m.m_el[6] = m_el[2]*Rhs.m_el[4] + m_el[6]*Rhs.m_el[5] + 
                m_el[10]*Rhs.m_el[6] + m_el[14]*Rhs.m_el[7];
    m.m_el[7] = m_el[3]*Rhs.m_el[4] + m_el[7]*Rhs.m_el[5] + 
                m_el[11]*Rhs.m_el[6] + m_el[15]*Rhs.m_el[7];

    m.m_el[8] = m_el[0]*Rhs.m_el[8] + m_el[4]*Rhs.m_el[9] + 
                m_el[8]*Rhs.m_el[10] + m_el[12]*Rhs.m_el[11];
    m.m_el[9] = m_el[1]*Rhs.m_el[8] + m_el[5]*Rhs.m_el[9] + 
                m_el[9]*Rhs.m_el[10] + m_el[13]*Rhs.m_el[11];
    m.m_el[10] = m_el[2]*Rhs.m_el[8] + m_el[6]*Rhs.m_el[9] + 
                 m_el[10]*Rhs.m_el[10] + m_el[14]*Rhs.m_el[11];
    m.m_el[11] = m_el[3]*Rhs.m_el[8] + m_el[7]*Rhs.m_el[9] + 
                 m_el[11]*Rhs.m_el[10] + m_el[15]*Rhs.m_el[11];

    m.m_el[12] = m_el[0]*Rhs.m_el[12] + m_el[4]*Rhs.m_el[13] + 
                 m_el[8]*Rhs.m_el[14] + m_el[12]*Rhs.m_el[15];
    m.m_el[13] = m_el[1]*Rhs.m_el[12] + m_el[5]*Rhs.m_el[13] + 
                 m_el[9]*Rhs.m_el[14] + m_el[13]*Rhs.m_el[15];
    m.m_el[14] = m_el[2]*Rhs.m_el[12] + m_el[6]*Rhs.m_el[13] + 
                 m_el[10]*Rhs.m_el[14] + m_el[14]*Rhs.m_el[15];
    m.m_el[15] = m_el[3]*Rhs.m_el[12] + m_el[7]*Rhs.m_el[13] + 
                 m_el[11]*Rhs.m_el[14] + m_el[15]*Rhs.m_el[15];

    return m;
}// END FUNCTION operator*( const icMatrix44& Rhs ) const


/*! Multiplies this matrix by another
 *
 *  @param          Rhs                 The matrix on the right of the *
 *  @returns        const icMatrix44&   The result of the mult, *(this)
**/
FORCE_INLINE const icMatrix44& icMatrix44::operator*=( const icMatrix44& Rhs)
{
    icMatrix44 m;

    m.m_el[0] = m_el[0]*Rhs.m_el[0] + m_el[4]*Rhs.m_el[1] + 
                m_el[8]*Rhs.m_el[2] + m_el[12]*Rhs.m_el[3];
    m.m_el[1] = m_el[1]*Rhs.m_el[0] + m_el[5]*Rhs.m_el[1] + 
                m_el[9]*Rhs.m_el[2] + m_el[13]*Rhs.m_el[3];
    m.m_el[2] = m_el[2]*Rhs.m_el[0] + m_el[6]*Rhs.m_el[1] + 
                m_el[10]*Rhs.m_el[2] + m_el[14]*Rhs.m_el[3];
    m.m_el[3] = m_el[3]*Rhs.m_el[0] + m_el[7]*Rhs.m_el[1] + 
                m_el[11]*Rhs.m_el[2] + m_el[15]*Rhs.m_el[3];

    m.m_el[4] = m_el[0]*Rhs.m_el[4] + m_el[4]*Rhs.m_el[5] + 
                m_el[8]*Rhs.m_el[6] + m_el[12]*Rhs.m_el[7];
    m.m_el[5] = m_el[1]*Rhs.m_el[4] + m_el[5]*Rhs.m_el[5] + 
                m_el[9]*Rhs.m_el[6] + m_el[13]*Rhs.m_el[7];
    m.m_el[6] = m_el[2]*Rhs.m_el[4] + m_el[6]*Rhs.m_el[5] + 
                m_el[10]*Rhs.m_el[6] + m_el[14]*Rhs.m_el[7];
    m.m_el[7] = m_el[3]*Rhs.m_el[4] + m_el[7]*Rhs.m_el[5] + 
                m_el[11]*Rhs.m_el[6] + m_el[15]*Rhs.m_el[7];

    m.m_el[8] = m_el[0]*Rhs.m_el[8] + m_el[4]*Rhs.m_el[9] + 
                m_el[8]*Rhs.m_el[10] + m_el[12]*Rhs.m_el[11];
    m.m_el[9] = m_el[1]*Rhs.m_el[8] + m_el[5]*Rhs.m_el[9] + 
                m_el[9]*Rhs.m_el[10] + m_el[13]*Rhs.m_el[11];
    m.m_el[10] = m_el[2]*Rhs.m_el[8] + m_el[6]*Rhs.m_el[9] + 
                 m_el[10]*Rhs.m_el[10] + m_el[14]*Rhs.m_el[11];
    m.m_el[11] = m_el[3]*Rhs.m_el[8] + m_el[7]*Rhs.m_el[9] + 
                 m_el[11]*Rhs.m_el[10] + m_el[15]*Rhs.m_el[11];

    m.m_el[12] = m_el[0]*Rhs.m_el[12] + m_el[4]*Rhs.m_el[13] + 
                 m_el[8]*Rhs.m_el[14] + m_el[12]*Rhs.m_el[15];
    m.m_el[13] = m_el[1]*Rhs.m_el[12] + m_el[5]*Rhs.m_el[13] + 
                 m_el[9]*Rhs.m_el[14] + m_el[13]*Rhs.m_el[15];
    m.m_el[14] = m_el[2]*Rhs.m_el[12] + m_el[6]*Rhs.m_el[13] + 
                 m_el[10]*Rhs.m_el[14] + m_el[14]*Rhs.m_el[15];
    m.m_el[15] = m_el[3]*Rhs.m_el[12] + m_el[7]*Rhs.m_el[13] + 
                 m_el[11]*Rhs.m_el[14] + m_el[15]*Rhs.m_el[15];

    *this = m;

    return *(this);
}// END FUNCTION operator*=( const icMatrix44& Rhs)


/*! Overloaded cast
 *
 *    @returns      const icReal*       Pointer to the data
**/
FORCE_INLINE icMatrix44::operator icReal*( void )
{
    return m_el;
}// END FUNCTION operator icReal*( void )


/*! Overloaded cast
 *
 *    @returns      const icReal*       Pointer to the data
**/
FORCE_INLINE icMatrix44::operator const icReal*(void) const
{
    return m_el;
}// END FUNCTION operator(const icReal*)()


/*! Gets the view matrix 'at' vector
 *
 *  @param          vec             Address to hold the 'at' vector
**/
FORCE_INLINE void icMatrix44::SetAt(const icVector3* const vec)
{
    m_el[8] = vec->x;
    m_el[9] = vec->y;
    m_el[10] = vec->z;
}// END FUNCTION GetAt(icVector3* vec) const


/*! Gets the view matrix 'right' vector
 *
 *  @param          vec             Address to hold the 'right' vector
**/
FORCE_INLINE void icMatrix44::SetRight(const icVector3* const vec)
{
    m_el[0] = vec->x;
    m_el[1] = vec->y;
    m_el[2] = vec->z;
}// END FUNCTION GetRight(icVector3* vec) const


/*! Gets the view matrix 'up' vector
 *
 *  @param          vec             Address to hold the 'up' vector
**/
FORCE_INLINE void icMatrix44::SetUp(const icVector3* const vec)
{
    m_el[4] = vec->x;
    m_el[5] = vec->y;
    m_el[6] = vec->z;
}// END FUNCTION GetUp(icVector3* vec) const


/*! Gets the view matrix 'at' vector
 *
 *  @param          vec             Address to hold the 'at' vector
**/
FORCE_INLINE void icMatrix44::GetAt(icVector3* vec) const
{
    vec->x = m_el[8];
    vec->y = m_el[9];
    vec->z = m_el[10];
}// END FUNCTION GetAt(icVector3* vec) const


/*! Gets the view matrix 'right' vector
 *
 *  @param          vec             Address to hold the 'right' vector
**/
FORCE_INLINE void icMatrix44::GetRight(icVector3* vec) const
{
    vec->x = m_el[0];
    vec->y = m_el[1];
    vec->z = m_el[2];
}// END FUNCTION GetRight(icVector3* vec) const


/*! Gets the view matrix 'up' vector
 *
 *  @param          vec             Address to hold the 'up' vector
**/
FORCE_INLINE void icMatrix44::GetUp(icVector3* vec) const
{
    vec->x = m_el[4];
    vec->y = m_el[5];
    vec->z = m_el[6];
}// END FUNCTION GetUp(icVector3* vec) const