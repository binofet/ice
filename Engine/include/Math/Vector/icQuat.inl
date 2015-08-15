

#include "Math/Vector/icQuat.h"

#include "Math/Vector/icVector3.h"
#include "Math/Vector/icVector4.h"

#include "Math/Matrix/icMatrix33.h"
#include "Math/Matrix/icMatrix44.h"


/*! Default Constructor
**/
FORCE_INLINE icQuat::icQuat( void )
{
}// END FUNCTION icQuat( void )


/*! Quat from 3x3 matrix c'tor
**/
FORCE_INLINE icQuat::icQuat( const icMatrix33& mat )
{
    icReal trace = mat.Trace();
    if ( trace > 0.0f )
    {
        icReal s = icSqrt( trace + icReal(1) );
        (*this).w = s*icReal(0.5);
        icReal recip = icReal(0.5)/s;
        (*this).x = mat[5] - mat[7]*recip;
        (*this).y = mat[6] - mat[2]*recip;
        (*this).z = mat[1] - mat[3]*recip;
    }
    else
    {
        unsigned int i = 0;
        if ( mat[4] > mat[0] )
            i = 1;
        if ( mat[8] > mat[i+3*i] )
            i = 2;
        unsigned int j = (i+1)%3;
        unsigned int k = (j+1)%3;
        icReal s =  mat[i+3*i] - mat[j+3*j] - mat[k+3*k] + icReal(1.0);
        s = icSqrt(s);
        (*this)[i] = icReal(0.5)*s;
        icReal recip = icReal(0.5)/s;
        (*this).w = mat[k+3*j] - mat[j+3*k]*recip;
        (*this)[j] = mat[j+3*i] + mat[i+3*j]*recip;
        (*this)[k] = mat[k+3*i] + mat[i+3*k]*recip;
    }
}// END FUNCTION icQuat( const icMatrix33& mat )


/*! Overloaded Constructor, initializes values
**/
FORCE_INLINE icQuat::icQuat( const icReal xIn, const icReal yIn, 
                             const icReal zIn, const icReal wIn )
{
    x = xIn;
    y = yIn;
    z = zIn;
    w = wIn;
}// END FUNCTION icQuat ( const icReal xIn, const icReal yIn, 
 //                       const icReal zIn, const icReal wIn )


/*! Make a 4x4 transform matrix with this quat rotation and given position
 *
 *  @param          pos             Position
 *  @returns        icMatrix44      Transform
**/
FORCE_INLINE icMatrix44 icQuat::MakeTransform( const icVector3& pos ) const
{
    icMatrix44 m;

    m[12] = pos.x;
    m[13] = pos.y;
    m[14] = pos.z;
    m[15] = 1;

    return m;
}// END FUNCTION MakeTransform( const icVector3& pos )


/*! Make a 3x3 rotation matrix from this quat
 *
 *  @returns        icMatrix33      Rotation matrix
**/
FORCE_INLINE icMatrix33 icQuat::MakeRotation( void ) const
{
    icMatrix33 m;
    return m;
}// END FUNCTION MakeRotation( void )



/*! Makes this quat the inverse of itself
**/
FORCE_INLINE const icQuat& icQuat::Invert( void )
{
    icReal norm = w*w + x*x + y*y + z*z;
    // if we're the zero quaternion, just return identity
    if ( icIsZero( norm ) )
    {
        this->Set(icReal(0),icReal(0),icReal(0),icReal(1));
        return *this;
    }

    icReal nR = 1.0f / norm;
    this->Set( -nR*x, -nR*y, -nR*z, nR*w );
    return *this;
}// END FUNCTION Invert( void )


/*! Find the complex conjugate
 *
 *  @returns        const icQuat&       This
**/
FORCE_INLINE const icQuat& icQuat::Conjugate( void )
{
    x = -x;
    y = -y;
    z = -z;
    return *this;
}// END FUNCTION Conjugate( void)


/*! Multiplies this quat by another
 *
 *  @param          Rhs             Quat to multiply this by
 *  @returns        icQuat          New quat, result of multiplication
**/
FORCE_INLINE icQuat icQuat::operator*( const icQuat& Rhs ) const
{
    icQuat q( w*Rhs.x + x*Rhs.w + y*Rhs.z - z*Rhs.y,
              w*Rhs.y + y*Rhs.w + z*Rhs.x - x*Rhs.z,
              w*Rhs.z + z*Rhs.w + x*Rhs.y - y*Rhs.x,
              w*Rhs.w - x*Rhs.x - y*Rhs.y - z*Rhs.z);
    return q;
}// END FUNCTION operator*( const icQuat& Rhs ) const


/*! Multiplies this quat by another
 *
 *  @param          Rhs             Quat to multiply this by
 *  @returns        const icQuat&   This quat, result of multiplication
**/
FORCE_INLINE const icQuat& icQuat::operator*=( const icQuat& Rhs )
{
    this->Set(w*Rhs.x + x*Rhs.w + y*Rhs.z - z*Rhs.y,
              w*Rhs.y + y*Rhs.w + z*Rhs.x - x*Rhs.z,
              w*Rhs.z + z*Rhs.w + x*Rhs.y - y*Rhs.x,
              w*Rhs.w - x*Rhs.x - y*Rhs.y - z*Rhs.z);

    return *this;
}// END FUNCTION operator*=( const icQuat& Rhs )


/*! Adds two quats together
 *
 *  @param          Rhs         Quaternion on the right of the +
 *  @returns        icQuat      new quaternion, (this + Rhs)
**/
FORCE_INLINE icQuat icQuat::operator+( const icQuat& Rhs ) const
{
    icQuat q(x + Rhs.x, y + Rhs.y, z + Rhs.z, w + Rhs.w);
    return q;
}// END FUNCTION :operator+( const icQuat& Rhs ) const


/*! Adds another quaternion to this one
 *
 *  @param      Rhs             Quaternion on the right of the +
 *  @returns    const icQuat&   (this = this + Rhs)
**/
FORCE_INLINE const icQuat& icQuat::operator+=( const icQuat& Rhs )
{
    x += Rhs.x; y += Rhs.y; z += Rhs.z; w += Rhs.w;
    return *this;
}// END FUNCTION operator+=( const icQuat& Rhs )


/*! Subtracts two quats from one another
 *
 *  @param          Rhs         Quaternion on the right of the -
 *  @returns        icQuat      new quaternion, (this - Rhs)
**/
FORCE_INLINE icQuat icQuat::operator-( const icQuat& Rhs ) const
{
    icQuat q(x - Rhs.x, y - Rhs.y, z - Rhs.z, w - Rhs.w);
    return q;
}// END FUNCTION operator-( const icQuat& Rhs ) const


/*! Subtracts another quaternion from this one
 *
 *  @param      Rhs             Quaternion on the right of the -
 *  @returns    const icQuat&   (this = this - Rhs)
**/
FORCE_INLINE const icQuat& icQuat::operator-=( const icQuat& Rhs )
{
    x -= Rhs.x; y -= Rhs.y; z -= Rhs.z; w -= Rhs.w;
    return *this;
}// END FUNCTION operator-=( const icQuat& Rhs )


/*! Quaternion scaling
 *
 *  @param          Rhs         Scalar
 *  @returns        icQuat      new quat, (this * Scalar)
**/
FORCE_INLINE icQuat icQuat::operator*( const icReal& Rhs ) const
{
    icQuat q(x*Rhs,y*Rhs,z*Rhs,w*Rhs);
    return q;
}// END FUNCTION operator*( const icReal& Rhs ) const


/*! Quaternion scaling
 *
 *  @param          Rhs         Scalar
 *  @returns        icQuat      new quat, (this * Scalar)
**/
FORCE_INLINE icQuat operator*( const icReal& Lhs, const icQuat& Rhs )
{
    icQuat q(Rhs.x*Lhs,Rhs.y*Lhs,Rhs.z*Lhs,Rhs.w*Lhs);
    return q;
}// END FUNCTION operator*( const icReal& Lhs, const icQuat& Rhs )


/*! Lerp this quat to the given end quat
 *
 *  @param          end             Destination quat
 *  @param          t               time value
**/
FORCE_INLINE const icQuat& icQuat::Lerp( const icQuat& end, icReal t )
{
    // get cos of "angle" between quaternions
    icReal cosTheta = this->Dot( end );

    // initialize result
    icQuat result = t*end;

    // if "angle" between quaternions is less than 90 degrees
    if ( cosTheta >= ICEPSILON )
    {
        // use standard interpolation
        result += (1.0f-t)*(*this);
    }
    else
    {
        // otherwise, take the shorter path
        result += (t-1.0f)*(*this);
    }
    *this = result;
    return *this;
}// END FUNCTION Lerp( const icQuat& end, icReal t )


/*! Spherical Lerp this quat to the given end quat
 *
 *  @param          end             Destination quat
 *  @param          t               time value
**/
FORCE_INLINE const icQuat& icQuat::Slerp( const icQuat& end, icReal t )
{
    // get cosine of "angle" between quaternions
    icReal cosTheta = this->Dot( end );
    icReal startInterp, endInterp;

    // if "angle" between quaternions is less than 90 degrees
    if ( cosTheta >= ICEPSILON )
    {
        // if angle is greater than zero
        if ( (1.0f - cosTheta) > ICEPSILON )
        {
            // use standard slerp
            icReal theta = acosf( cosTheta );
            icReal recipSinTheta = 1.0f/icSin( theta );

            startInterp = icSin( (1.0f - t)*theta )*recipSinTheta;
            endInterp = icSin( t*theta )*recipSinTheta;
        }
        // angle is close to zero
        else
        {
            // use linear interpolation
            startInterp = 1.0f - t;
            endInterp = t;
        }
    }
    // otherwise, take the shorter route
    else
    {
        // if angle is less than 180 degrees
        if ( (1.0f + cosTheta) > ICEPSILON )
        {
            // use slerp w/negation of start quaternion
            icReal theta = acosf( -cosTheta );
            icReal recipSinTheta = 1.0f/icSin( theta );

            startInterp = icSin( (t-1.0f)*theta )*recipSinTheta;
            endInterp = icSin( t*theta )*recipSinTheta;
        }
        // angle is close to 180 degrees
        else
        {
            // use lerp w/negation of start quaternion
            startInterp = t - 1.0f;
            endInterp = t;
        }
    }
    
    *this = startInterp*(*this) + endInterp*end;
    return *this;
}// END FUNCTION Slerp( const icQuat& end, icReal t )


/*! Spherical Lerp this quat to the given end quat (approximation) 
 *
 *   Approximate spherical linear interpolation of two quaternions
 *   Based on "Hacking Quaternions", Jonathan Blow, Game Developer, March 2002.
 *   See Game Developer, February 2004 for an alternate method.
 *
 *  @param          end             Destination quat
 *  @param          t               time value
 *  @returns        const icQuat&   this
**/
FORCE_INLINE const icQuat& icQuat::ApproxSlerp(const icQuat& end, icReal t)
{
    icReal cosTheta = this->Dot(end);

    // correct time by using cosine of angle between quaternions
    icReal factor = 1.0f - 0.7878088f*cosTheta;
    icReal k = 0.5069269f;
    factor *= factor;
    k *= factor;

    icReal b = 2*k;
    icReal c = -3*k;
    icReal d = 1 + k;

    t = t*(b*t + c) + d;

    // initialize result
    icQuat result = end * t;

    // if "angle" between quaternions is less than 90 degrees
    if ( cosTheta >= ICEPSILON )
    {
        // use standard interpolation
        result += (icReal(1)-t) * (*this);
    }
    else
    {
        // otherwise, take the shorter path
        result += (t-icReal(1)) * (*this);
    }
    *this = result;
    return *this;
}// END FUNCTION ApproxSlerp(const icQuat& end, icReal t)