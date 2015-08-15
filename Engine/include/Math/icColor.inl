
#include "Math/icColor.h"

/*! Default Constructor
**/
FORCE_INLINE icColor::icColor( void )
{
}// END FUNCTION icColor( void )


/*! Overloaded Constructor, initializes color
**/
FORCE_INLINE icColor::icColor( uchar rIn, uchar gIn, uchar bIn, uchar aIn )
{
    r = rIn;
    g = gIn;
    b = bIn;
    a = aIn;
}// END FUNCTION icColor( uchar rIn, uchar gIn, uchar bIn, uchar aIn )


/*! Default Destructor
**/
FORCE_INLINE icColor::~icColor( void )
{
}// END FUNCTION ~icColor( void )


/*! Sets color
**/
FORCE_INLINE void icColor::Set( uchar rIn, uchar gIn, uchar bIn, uchar aIn )
{
    r = rIn;
    g = gIn;
    b = bIn;
    a = aIn;
}// END FUNCTION Set( uchar rIn, uchar gIn, uchar bIn, uchar aIn )


/*! Sets color
**/
FORCE_INLINE void icColor::SetRGBA(uint col)
{
    a = (uchar)(col>>24);
    r = (uchar)(col>>16);
    g = (uchar)(col>>8);
    b = (uchar)(col);
}// END FUNCTION SetRGBA(uint col)


/*! Coverts color to unsigned int
**/
FORCE_INLINE uint icColor::R8G8B8A8( void ) const
{
    return    ((r<<24)&0xFF000000) |
            ((g<<16)&0x00FF0000) |
            ((b<<8 )&0x0000FF00) |
            (a&      0x000000FF);
}// END FUNCTION GetRGBA8( void ) const


/*! Coverts color to unsigned int
**/
FORCE_INLINE uint icColor::A8R8G8B8( void ) const
{
    return    ((a<<24)&0xFF000000) |
            ((r<<16)&0x00FF0000) |
            ((g<<8 )&0x0000FF00) |
            ( b&     0x000000FF);
}// END FUNCTION A8R8G8B8( void ) const;


/*! Overloaded assignment operator
 *
 *    @param[in]    uiColor            Color to copy (unsigned)
 *    @returns    const icColor&    This
 *
**/
FORCE_INLINE const icColor& icColor::operator=( uint uiColor )
{
    r = (uchar)(uiColor>>24);
    g = (uchar)(uiColor>>16);
    b = (uchar)(uiColor>>8);
    a = (uchar)(uiColor);
    return *this;
}// END FUNCTION operator=( uint uiColor )


/*! Overloaded equality operator
**/
FORCE_INLINE bool icColor::operator==( const icColor& Rhs ) const
{
    return ( r == Rhs.r &&
             g == Rhs.g &&
             b == Rhs.b &&
             a == Rhs.a );
}// END FUNCTION operator==( const icColor& Rhs ) const


/*! Overloaded in-equality operator
**/
FORCE_INLINE bool icColor::operator!=( const icColor& Rhs ) const
{
    return ( r != Rhs.r ||
             g != Rhs.g ||
             b != Rhs.b ||
             a != Rhs.a );
}// END FUNCTION operator!=( const icColor& Rhs ) const

     
/*! Returns floating point red value
 *
 *    @returns const float    Red value as a float
 *
**/
FORCE_INLINE const float icColor::R( void ) const
{
    return r/255.0f;
}// END FUNCTION R( void )


/*! Returns floating point green value
 *
 *    @returns const float    Green value as a float
 *
**/
FORCE_INLINE const float icColor::G( void ) const
{
    return g/255.0f;
}// END FUNCTION G( void )


/*! Returns floating point blue value
 *
 *    @returns const float    Blue value as a float
 *
**/
FORCE_INLINE const float icColor::B( void ) const
{
    return b/255.0f;
}// END FUNCTION B( void )


/*! Returns floating point alpha value
 *
 *    @returns const float    Alpha value as a float
 *
**/
FORCE_INLINE const float icColor::A( void ) const
{
    return a/255.0f;
}// END FUNCTION A( void )

