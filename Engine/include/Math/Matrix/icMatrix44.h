#ifndef __IC_MATRIX44_H__
#define __IC_MATRIX44_H__


#include "Math/icCommonMath.h"

class icVector3;
class icVector4;
class icQuat;
class icMatrix33;

class icMatrix44
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icMatrix44( void );
    icMatrix44( const icReal* data );
    icMatrix44( const icQuat& rot, const icVector3& pos );
    icMatrix44( const icMatrix33& rot, const icVector3& pos );

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    void Identity( void );

    // rotations
    void RotX( const icReal theta );
    void RotY( const icReal theta );
    void RotZ( const icReal theta );
    void Rotate( const icMatrix44& Rhs );

    const icMatrix44& MakeRotX( const icReal theta );
    const icMatrix44& MakeRotY( const icReal theta );
    const icMatrix44& MakeRotZ( const icReal theta );

    const icMatrix44& SetRotation( const icMatrix33& rot );
    const icMatrix44& SetRotation( const icQuat& rot );

    const icMatrix44& Scale( const icVector3& Rhs );
    const icMatrix44& Scale( icReal x, icReal y, icReal z );

    icMatrix44 Inverse( void ) const;
    const icMatrix44& Invert( void );
    const icMatrix44& Transpose( void );

    void SetPos( const icVector3 pos );
    void SetPos( icReal x, icReal y, icReal z );

    void SetAt(const icVector3* const vec);
    void SetRight(const icVector3* const vec);
    void SetUp(const icVector3* const vec);

    const icMatrix44& Translate( const icVector3& translation );
    const icMatrix44& Translate( icReal x, icReal y, icReal z );

    const icMatrix44& MakeTranslation( const icVector3& translation );
    const icMatrix44& MakeTranslation( icReal x, icReal y, icReal z );



    icVector3 Transform( icReal x, icReal y, icReal z) const;
    icVector3 TransformVect( const icVector3& vect ) const;

    // projections
    friend void icCreateOrtho(const icReal width, const icReal height,
                              const icReal nZ, const icReal fZ,
                              icMatrix44& outMat);

    friend void icCreateOrthoOffset(const icReal left, const icReal right,
                                    const icReal bottom, const icReal top,
                                    const icReal nZ, const icReal fZ,
                                    icMatrix44& outMat);

    friend void icCreatePerspective(const icReal fov, const icReal ar,
                                    const icReal nZ, const icReal fZ,
                                    icMatrix44& outMat);

    friend void icCreateLookAt(const icVector3& pos, const icVector3& target,
                               const icVector3& up, icMatrix44& outMat);

    friend void icCreateLookDir(const icVector3& pos, const icVector3& dir,
                                const icVector3& up, icMatrix44& outMat);

    //////////////////////////////////////////////////////////////////////////
    // ACCESS
    icVector4 GetRow( uchar i ) const;
    icVector4 GetCol( uchar i ) const;

    void GetAt(icVector3* vec) const;
    void GetRight(icVector3* vec) const;
    void GetUp(icVector3* vec) const;

    void GetPos( icVector3& pos );
    icVector3& GetPos( void );
    const icVector3& GetPos( void ) const;

    icVector3 GetTransformedPos( void );

    //////////////////////////////////////////////////////////////////////////
    // OPERATORS
    icReal& operator()( uchar row, uchar col );
    const icReal operator()( uchar row, uchar col ) const;

    // 4x4 * 3x1
    icVector3 operator*( const icVector3& Rhs ) const;
    friend icVector3 operator*( const icVector3& Lhs,
                                const icMatrix44& Rhs );

    // 4x4 * 4x1
    icVector4 operator*( const icVector4& Rhs ) const;
    friend icVector4 operator*( const icVector4& Lhs,
                                const icMatrix44& Rhs );

    // 4x4 * 4x4
    icMatrix44 operator*( const icMatrix44& Rhs ) const;
    const icMatrix44& operator*=( const icMatrix44& Rhs);

    operator icReal*( void );
    operator const icReal*( void ) const;    

    static const icMatrix44 IDENTITY;
private:
    //////////////////////////////////////////////////////////////////////////
    // PRIVATE MEMBERS
    icReal    m_el[16];
};

#include "Math/Matrix/icMatrix44.inl"

#endif //__IC_MATRIX44_H__

