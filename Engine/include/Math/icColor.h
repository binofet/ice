#ifndef __IC_COLOR_H__
#define __IC_COLOR_H__

#include "Math/icCommonMath.h"

class icColor
{
public:
    /////////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icColor( void );
    icColor( uchar rIn, uchar gIn, uchar bIn, uchar aIn=255 );
    ~icColor( void );

    /////////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    void Set( uchar rIn, uchar gIn, uchar bIn, uchar aIn=255 );

    void SetRGBA(uint col);

    uint R8G8B8A8( void ) const;
    uint A8R8G8B8( void ) const;

    const float R( void ) const;
    const float G( void ) const;
    const float B( void ) const;
    const float A( void ) const;

    /////////////////////////////////////////////////////////////////////////////
    // OPERATORS
    const icColor& operator=( uint uiColor );
    bool operator==( const icColor& Rhs ) const;
    bool operator!=( const icColor& Rhs ) const;

    /////////////////////////////////////////////////////////////////////////////
    // PUBLIC MEMBERS
    uchar    r;
    uchar    g;
    uchar    b;
    uchar    a;

    /////////////////////////////////////////////////////////////////////////////
    // STATIC MEMBERS
    static icColor BLACK;
    static icColor BLUE;
    static icColor GREEN;
    static icColor RED;
    static icColor MAGENTA;
    static icColor YELLOW;
    static icColor BROWN;
    static icColor LGREY;
    static icColor DGREY;
    static icColor BBLUE;
    static icColor BGREEN;
    static icColor BRED;
    static icColor BMAGENTA;
    static icColor BYELLOW;
    static icColor WHITE;
};

#include "Math/icColor.inl"

#endif //__IC_COLOR_H__

