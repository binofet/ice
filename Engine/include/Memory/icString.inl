

#include "Memory/icString.h"

#if IC_STRING_DYNAMIC
//! TODO: start a memory manager, dynamic strings should reside in a mem pool
#include <stdlib.h>
#endif

/*! Default constructor
 *
 *    Initializes the string class
**/
FORCE_INLINE icString::icString( void )
{
    m_uCurPos=0;
    m_uMemSize=IC_STRING_MAX;
#if IC_STRING_DYNAMIC
    m_Mem=(char*)malloc(IC_STRING_MAX);
    assert(m_Mem);
#endif
    m_Mem[0]='\0';
}// END FUNCTION icString( void )


/*! Overloaded constructor for initializing string with content
 *
 *    @param[in]    szInitial    Buffer to initial this string with
**/
FORCE_INLINE icString::icString( const char* szInitial )

{
    if (!szInitial)
    {
        m_uCurPos=0;
        m_uMemSize=IC_STRING_MAX;
#if IC_STRING_DYNAMIC
        m_Mem=(char*)malloc(IC_STRING_MAX);
        assert(m_Mem);
#endif
        m_Mem[0]='\0';
        return;
    }

    // get the length of copy string
    uint length=0;
    while (szInitial[length] != '\0')
        length++;

    if (length > 0)
    {
#if IC_STRING_DYNAMIC
        // we only malloc in large chunks for dynamic strings
        m_uMemSize = length;
        while (m_uMemSize%IC_STRING_MAX)
            m_uMemSize++;
        assert(m_uMemSize < IC_STRING_MAX_DYNAMIC);
        m_Mem = (char*)malloc(m_uMemSize);
        assert(m_Mem);
#else
        // when not using dynamic strings, we will copy until full
        if ( length > IC_STRING_MAX )
        {
            length = IC_STRING_MAX;
            icWarning("Truncated string, too big for buffer!");
        }
        m_uMemSize = IC_STRING_MAX;
#endif
        
        //! copy data into buffer
        m_uCurPos = 0;
        while (m_uCurPos < length)
            m_Mem[m_uCurPos] = szInitial[m_uCurPos++];

        m_Mem[m_uCurPos] = '\0';
    }
    else // an empty string was passed in, make this an empty string as well
    {
        m_uCurPos=0;
        m_uMemSize=IC_STRING_MAX;
#if IC_STRING_DYNAMIC
        m_Mem=(char*)malloc(IC_STRING_MAX);
        assert(m_Mem);
#endif
        m_Mem[0]='\0';
    }
}// END FUNCTION icString( const char* szInitial )


/*! Copy Constructor
 *
 *    @param[in]    refCopy    Reference to the string to copy
**/
FORCE_INLINE icString::icString( const icString& refCopy )
{
    uint length=refCopy.Length();

    if (length > 0)
    {
#if IC_STRING_DYNAMIC
        // we only malloc in large chunks for dynamic strings
        m_uMemSize = length;
        while (m_uMemSize%IC_STRING_MAX)
            m_uMemSize++;
        assert(m_uMemSize < IC_STRING_MAX_DYNAMIC);
        m_Mem = (char*)malloc(m_uMemSize);
#else
        // when not using dynamic strings, we will copy until full
        if ( length > IC_STRING_MAX )
            length = IC_STRING_MAX;
        m_uMemSize = IC_STRING_MAX;
#endif

        // copy string into this one
        m_uCurPos=0;
        while (m_uCurPos < length)
            m_Mem[m_uCurPos] = refCopy.m_Mem[m_uCurPos++];

        m_Mem[m_uCurPos] = '\0';
    }
    else
    {
        m_uCurPos=0;
        m_uMemSize=IC_STRING_MAX;
#if IC_STRING_DYNAMIC
        m_Mem=(char*)malloc(IC_STRING_MAX);
        assert(m_Mem);
#endif
        m_Mem[0]='\0';
    }
}// END FUNCTION icString( const icString& refCopy )


/*! Default destructor
 *
 *    Frees any allocated memory if using dynamic strings
**/
FORCE_INLINE icString::~icString( void )
{
#if IC_STRING_DYNAMIC
    free(m_Mem);
    m_Mem = NULL;
#endif
}// END FUNCTION ~icString( void )


/*!  Compares this string to another
 *
 *    @param[in]    val        const reference to compare string
 *    @returns      int        -1,0,1 (Less, Equal, Greater)
**/
FORCE_INLINE const int icString::Compare( const icString& val ) const
{
    int comp=0;
    uint i=0;
    // TODO, what is the behavior if one of these strings is empty?
    while (!comp && i<m_uCurPos && i<val.Length())
        comp = m_Mem[i] - val.m_Mem[(int)i++];
    return comp;
}// END FUNCTION Compare( cost icString& val ) const


/*! Finds the first occurance of cPattern
 *
 *    @param[in]    cPattern    Search value
 *    @returns      const int   Index of found val, -1 if not found
**/
FORCE_INLINE const int icString::FindFirst( const char cPattern ) const
{
    int found = -1;
    uint i=0;
    while ( found < 0  && i < m_uCurPos)
        found = (cPattern == m_Mem[i++]) ? (int)i-1 : -1;

    return found;
}// END FUNCTION FindFirst( const char val ) const


/*! Finds the last occurance of cPattern
 *
 *    @param[in]    cPattern    Search value
 *    @returns      const int   Index of found val, -1 if not found
**/
FORCE_INLINE const int icString::FindLast( const char cPattern ) const
{
    int found = -1;
    uint i=m_uCurPos;
    while ( found < 0  && i > 0 )
        found = (cPattern == m_Mem[--i]) ? (int)i : -1;

    return found;
}// END FUNCTION FindLast( const char val ) const


/*! Makes all characters in string uppercase
 *
 *    @returns    const icString&    Modified refernce to this
**/
FORCE_INLINE const icString& icString::MakeUpper( void )
{
    uint i=m_uCurPos;
    while ( i-- > 0 )
    {
        if (96 < m_Mem[i] && m_Mem[i] < 123 )
            m_Mem[i] -= 32;
    }
    return *this;
}// END FUNCTION MakeUpper( void )


/*! Makes all characters in string lowercase
 *
 *    @returns    const icString&        Modified refernce to this
**/
FORCE_INLINE const icString& icString::MakeLower( void )
{
    uint i=m_uCurPos;
    while ( i-- > 0 )
    {
        if (64 < m_Mem[i] && m_Mem[i] < 91 )
            m_Mem[i] += 32;
    }
    return *this;
}// END FUNCTION MakeLower( void )


/*! Replaces all instances of cPattern with cReplace
 *
 *    @param[in]    cPattern            Search pattern
 *    @param[in]    cReplace            Replaces search pattern
 *    @returns      const icString&     Modified refernce to this
**/
FORCE_INLINE const icString& icString::Replace( char cPattern, char cReplace )
{
    uint i=m_uCurPos;
    while ( i-- > 0 )
    {
        if (m_Mem[i] == cPattern )
            m_Mem[i] = cReplace;
    }
    return *this;
}// END FUNCTION Replace( char cFind, char cReplace )


/*! Gets a character at index in string
 *
 *    @param[in]    index    Index to desired character
 *    @returns      char&    Indexed character
**/
FORCE_INLINE char icString::operator[]( uint index ) const
{
    assert( index <= m_uCurPos );
    return m_Mem[index];
}// END FUNCTION operator[]( uint index ) const


/*! Gets a character at index in string
 *
 *    @param[in]    index    Index to desired character
 *    @returns      char&    Reference to indexed character
**/
FORCE_INLINE char& icString::operator[]( uint index )
{
    assert( index <= m_uCurPos );
    return m_Mem[index];
}// END FUNCTION operator[]( uint index )


/*! Concatenates two strings together, making a new string
 *! the original strings are not changed
 *
 *    @param[in]    Rhs             const reference to string on right
 *    @returns      icString        the new concatenated string
**/
FORCE_INLINE icString icString::operator+( const icString& Rhs )
{
    uint length = Rhs.Length();
    uint catLength = m_uCurPos + length;

    icString catString(*this);

#if IC_STRING_DYNAMIC
    if (catLength > catString.m_uMemSize)
    {
        // make sure we get some more room
        catString.m_uMemSize = catLength;
        while (catString.m_uMemSize%IC_STRING_MAX)
            catString.m_uMemSize++;
        assert(catString.Expand( catString.m_uMemSize ));
    }
#else
    if ( catLength > IC_STRING_MAX )
        catLength = IC_STRING_MAX;
#endif

    uint i=0;
    while (catString.m_uCurPos < catLength)
        catString[catString.m_uCurPos++] = Rhs.m_Mem[i++];

    catString.m_Mem[catString.m_uCurPos] = '\0';

    return catString;
}// END FUNCTION operator+( const icString& Rhs )


/*! Concatenates two strings together, making a new string
 *! the original strings are not changed
 *
 *    @param[in]    Rhs             Pointer to char buffer
 *    @returns      icString        the new concatenated string
**/
FORCE_INLINE icString icString::operator+( const char* Rhs )
{
    icString catString(*this);

    uint length = 0;
    while ( Rhs[length] != '\0')
        length++;

    if (length > 0)
    {
        uint catLength = m_uCurPos + length;
#if IC_STRING_DYNAMIC
        if (catLength > catString.m_uMemSize)
        {
            // make sure we get some more room
            catString.m_uMemSize = catLength;
            while (catString.m_uMemSize%IC_STRING_MAX)
                catString.m_uMemSize++;
            assert(catString.Expand( catString.m_uMemSize ));
        }
#else
        if ( catLength > IC_STRING_MAX )
            catLength = IC_STRING_MAX;
#endif
        uint i=0;
        while (catString.m_uCurPos < catLength)
            catString[catString.m_uCurPos++] = Rhs[i++];

        catString.m_Mem[catString.m_uCurPos] = '\0';
    }

    return catString;
}// END FUNCTION operator+( const char* Rhs )


/*! Concatenates two strings together, the string on the
 *! left is modified.
 *
 *    @param[in]    Rhs                    String on the right
 *    @returns      const icString&        Modified reference to this
**/
FORCE_INLINE const icString& icString::operator+=( const icString& Rhs )
{
    uint catLength = m_uCurPos + Rhs.Length();
#if IC_STRING_DYNAMIC
    if (catLength > m_uMemSize)
    {
        // make sure we get some more room
        m_uMemSize = catLength;
        while (m_uMemSize%IC_STRING_MAX)
            m_uMemSize++;
        assert(Expand( m_uMemSize ));
    }
#else
    if ( catLength > IC_STRING_MAX )
        catLength = IC_STRING_MAX;
#endif

    uint i=0;
    while (m_uCurPos < catLength)
        m_Mem[m_uCurPos++] = Rhs.m_Mem[i++];

    m_Mem[m_uCurPos] = '\0';

    return *this;
}// END FUNCTION operator+=( const icString& Rhs )


/*! Concatenates two strings together, the string on the
 *! left is modified.
 *
 *    @param[in]    Rhs                    String on the right
 *    @returns      const icString&        Modified reference to this
**/
FORCE_INLINE const icString& icString::operator+=( const char* Rhs )
{
    assert( Rhs );

    uint length = 0;
    while ( Rhs[length] != '\0')
        length++;

    if ( length > 0 )
    {
        uint catLength = m_uCurPos + length;
#if IC_STRING_DYNAMIC
        if (catLength > m_uMemSize )
        {
            uint newSize = catLength + (catLength%IC_STRING_MAX);
            assert(Expand(newSize));
            m_uMemSize = newSize;
        }
#else
        if ( catLength > IC_STRING_MAX )
            catLength = IC_STRING_MAX;
#endif

        uint i=0;
        while (m_uCurPos < catLength)
            m_Mem[m_uCurPos++] = Rhs[i++];

        m_Mem[m_uCurPos] = '\0';
    }
    return *this;
}// END FUNCTION operator+=( const char* Rhs )


/*! Assigns this string the value of the string on the right
 *
 *    @param[in]    Rhs                    String on the right
 *    @returns      const icString&        Modifed reference to this
**/
FORCE_INLINE const icString& icString::operator=( const icString& Rhs )
{
    if (this == &Rhs)
        return *this;

    uint length=Rhs.Length();
#if IC_STRING_DYNAMIC
    m_uMemSize = length;
    while (m_uMemSize%IC_STRING_MAX)
        m_uMemSize++;
    assert(m_uMemSize < IC_STRING_MAX_DYNAMIC);
    m_Mem = (char*)malloc(m_uMemSize);
#else
    m_uMemSize = IC_STRING_MAX;
#endif

    m_uCurPos=0;
    while (m_uCurPos < length)
        m_Mem[m_uCurPos] = Rhs.m_Mem[m_uCurPos++];

    m_Mem[m_uCurPos] = '\0';

    return *this;
}// END FUNCTION operator=( const icString& Rhs )


/*! Assigns this string the value of the string on the right
 *
 *    @param[in]    Rhs                     Pointer to char array
 *    @returns      const icString&         Modifed reference to this
**/
FORCE_INLINE const icString& icString::operator=( const char* Rhs )
{
    assert(Rhs);
    uint length=0;
    while (Rhs[length] != '\0')
        length++;

    if ( length > 0 )
    {
#if IC_STRING_DYNAMIC
        m_uMemSize = length;
        while (m_uMemSize%IC_STRING_MAX)
            m_uMemSize++;
        assert(m_uMemSize < IC_STRING_MAX_DYNAMIC);
        m_Mem = (char*)malloc(m_uMemSize);
#else
        if ( length > IC_STRING_MAX )
            length = IC_STRING_MAX;
        m_uMemSize = IC_STRING_MAX;
#endif

        m_uCurPos=0;
        while (m_uCurPos < length)
            m_Mem[m_uCurPos] = Rhs[m_uCurPos++];

        m_Mem[m_uCurPos] = '\0';
    }
    return *this;
}// END FUNCTION operator=( const char* Rhs )


/*! Compares two strings for equality
 *
 *    @param[in]    Rhs             String to compare this to
 *    @returns      const bool      True if data is equal
**/
FORCE_INLINE const bool icString::operator==( const icString& Rhs ) const
{
    int comp=-1;
    if (m_uCurPos == Rhs.Length())
    {
        comp=0;
        uint i=0;
        while (!comp && i < m_uCurPos)
            comp = m_Mem[i] - Rhs.m_Mem[(int)i++];

    }
    return (comp == 0);
}// END FUNCTION operator==( const icString& Rhs ) const


/*! Compares two strings for inequality
 *
 *    @param[in]    Rhs             String to compare this to
 *    @returns      const bool      True if data is not equal
**/
FORCE_INLINE const bool icString::operator!=( const icString& Rhs ) const
{
    int comp=-1;
    if (m_uCurPos == Rhs.Length())
    {
        comp=0;
        uint i=0;
        while (!comp && i<m_uCurPos && i<Rhs.Length())
            comp = m_Mem[i] - Rhs.m_Mem[(int)i++];
    }
    return (comp != 0);
}// END FUNCTION operator!=( const icString& Rhs ) const


/*! Compares two strings for greater-than-equal
 *
 *    @param[in]    Rhs            String to compare this to
 *    @returns      const bool      True if left is >= to right
**/
FORCE_INLINE const bool icString::operator>=( const icString& Rhs ) const
{
    return (Compare(Rhs) >= 0);
}// END FUNCTION operator>=( const icString& Rhs ) const


/*! Compares two strings for less-than-equal
 *
 *    @param[in]    Rhs             String to compare this to
 *    @returns      const bool      True if left is <= to right
**/
FORCE_INLINE const bool icString::operator<=( const icString& Rhs ) const
{
    return (Compare(Rhs) <= 0);
}// END FUNCTION operator<=( const icString& Rhs ) const


/*! Compares two strings for greater-than
 *
 *    @param[in]    Rhs             String to compare this to
 *    @returns      const bool      True if left is > to right
**/
FORCE_INLINE const bool icString::operator>( const icString& Rhs ) const
{
    return (Compare(Rhs) > 0);
}// END FUNCTION operator>( const icString& Rhs ) const


/*! Compares two strings for less-than
 *
 *    @param[in]    Rhs             String to compare this to
 *    @returns      const bool      True if left is < to right
**/
FORCE_INLINE const bool icString::operator<( const icString& Rhs ) const
{
    return (Compare(Rhs) < 0);
}// END FUNCTION operator<( const icString& Rhs ) const


/*! Returns pointer to the string data
 *
 *    @returns    char*    Pointer to internal memory
 *
**/
FORCE_INLINE const char* icString::ToChar( void ) const
{
    return m_Mem;
}// END FUNCTION ToChar( void ) const


/*! Returns length of string
 *
 *    @returns    uint    Length of the string
**/
FORCE_INLINE const uint icString::Length( void ) const
{
    return m_uCurPos;
}// END FUNCTION Length ( void ) const


/*! Returns the size of the string buffer
 *
 *    @returns    uint    Size of container buffer
**/
FORCE_INLINE const uint icString::BufferSize( void ) const
{
    return m_uMemSize;
}// END FUNCTION BufferSize( void ) const


/*! Expands the size of the buffer if using dynamic strings
 *
 *    @returns    const bool    True on successful realloc
**/
FORCE_INLINE const bool icString::Expand( uint newSize )
{
    // TODO, make this thread safe
#if IC_STRING_DYNAMIC
    char* temp = (char*)realloc(m_Mem, newSize);
    if(temp)
        m_Mem = temp;
    return (NULL != temp);
#else
    newSize;
    return false;
#endif
}// END FUNCTION Expand( uint neSize )

