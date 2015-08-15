#ifndef __IC_PLANE_H__
#define __IC_PLANE_H__


class icPlane
{
public:
    icPlane( void );
    icPlane( const icVector3& normal, const icReal offset );

    const icVector3& GetNormal( void ) const;
    const icReal GetOffset( void ) const;

    bool operator==( const icPlane& plane ) const;
    bool operator!=( const icPlane& plane ) const;
private:
    icVector3    m_v3Norm;
    icReal    m_Offset;
};

#endif //__IC_PLANE_H__