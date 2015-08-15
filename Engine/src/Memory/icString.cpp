
#include "Memory/icString.h"



/*! Finds the first occurance of sPattern
 *
 *    @param[in]    sPattern    Search value
 *    @returns    const int    Index of found val, -1 if not found
 *
**/
const int icString::FindFirst( const icString& sPattern ) const
{
    int found=-1;
    uint myPos=0;
    uint length = sPattern.Length();
    while( found<0 && myPos<m_uCurPos )
    {
        uint sPos=0;
        bool matches = true;
        while ( matches && sPos<length )
            matches = (m_Mem[myPos+sPos++] == sPattern[sPos]);

        if (sPos == length)
            found = myPos;
        ++myPos;
    }
    return found;
}// END FUNCTION FindFirst( const icString& sPattern ) const


/*! Finds the first occurance of szPattern
 *
 *    @param[in]    szPattern    Search value
 *    @returns    const int    Index of found val, -1 if not found
 *
**/
const int icString::FindFirst( const char* szPattern ) const
{
    assert(szPattern);

    int found=-1;
    uint myPos=0;
    uint length = 0;
    while (szPattern[length++] != '\0')
        ;

    if (length > 1)
    {
        length--;
        while( found<0 && myPos+length<m_uCurPos )
        {
            uint sPos=0;
            bool matches = true;
            while ( matches && sPos<length )
                matches = (m_Mem[myPos+sPos++] == szPattern[sPos]);

            if (sPos == length)
                found = myPos;
            ++myPos;
        }
    }
    return found;
}// END FUNCTION FindFirst( const char* szPattern ) const


/*! Finds the last occurance of sPattern
 *
 *    @param[in]    sPattern    Search value
 *    @returns    const int    Index of found val, -1 if not found
 *
**/
const int icString::FindLast( const icString& sPattern ) const
{
    int found=-1;
    uint myPos=m_uCurPos-1;
    uint length = sPattern.Length();
    while(found < 0 && myPos > length )
    {
        uint sPos=length;
        bool matches = true;
        while ( matches && --sPos>0 )
            matches = (m_Mem[myPos-(length-1)+sPos] == sPattern[sPos]);

        if (sPos == 0)
            found = myPos;
        --myPos;
    }
    return found;
}// END FUNCTION FindLast( const icString& sPattern ) const



/*! Finds the last occurance of szPattern
 *
 *    @param[in]    szPattern    Search value
 *    @returns    const int    Index of found val, -1 if not found
 *
**/
const int icString::FindLast( const char* szPattern ) const
{
    int found=-1;
    uint length = 0;
    while (szPattern[length] != '\0')
        length++;

    uint myPos=m_uCurPos-1;
    if (length > 1)
    {
        while( found < 0 && myPos > length )
        {
            uint sPos=length;
            bool matches = true;
            while ( matches && --sPos>0 )
                matches = (m_Mem[myPos-(length-1)+sPos] == szPattern[sPos]);

            if (sPos == 0)
                found = myPos-length;
            --myPos;
        }
    }
    return found;
}// END FUNCTION FindLast( const char* szPattern ) const


/*! Inserts val into the string at index (room permitting)
 *
 *    @param[in]    val            The value to insert
 *    @param[in]    index        The position in string to insert val
 *    @returns    const bool    True on success
 *
**/
const bool icString::Insert( const char val, uint index )
{
    assert( index < m_uCurPos );
#if IC_STRING_DYNAMIC
    if (m_uCurPos+1>m_uMemSize)
    {
        if ( !Expand( m_uCurPos+1 ) )
            return false;
    }
#else
    if (m_uCurPos+1>IC_STRING_MAX)
        return false;
#endif
    uint cur = m_uCurPos;
    while ( cur >= index )
        m_Mem[cur+1] = m_Mem[cur--];

    m_uCurPos++;
    m_Mem[index] = val;
    return true;
}// END FUNCTION Insert( const char val, uint index )


/*! Inserts val into the string at index (room permitting)
 *
 *    @param[in]    val            The value to insert
 *    @param[in]    index        The position in string to insert val
 *    @returns    const bool    True on success
 *
**/
const bool icString::Insert( const char* val, uint index )
{
    assert( val );
    assert( index < m_uCurPos );

    uint length = 0;
    while ( val[length] != '\0' )
        length++;

#if IC_STRING_DYNAMIC
    if (m_uCurPos+length>m_uMemSize)
    {
        m_uMemSize = m_uCurPos+length;
        while (m_uMemSize%IC_STRING_MAX)
            m_uMemSize++;
        assert(m_uMemSize < IC_STRING_MAX_DYNAMIC);
        if ( !Expand( m_uMemSize ) )
            return false;
    }
#else
    if (m_uCurPos+length>IC_STRING_MAX)
        return false;
#endif
    uint cur = m_uCurPos;
    while ( cur+length >= index )
        m_Mem[cur+length] = m_Mem[cur--];

    uint i=0;
    cur = index;
    while ( i < length )
        m_Mem[cur++] = val[i++];

    m_uCurPos += length;
    return true;
}// END FUNCTION Insert( const char* val, uint index )


/*! Inserts val into the string at index (room permitting)
 *
 *    @param[in]    val            The value to insert
 *    @param[in]    index        The position in string to insert val
 *    @returns    const bool    True on success
 *
**/
const bool icString::Insert( const icString& val, uint index)
{
    assert( index < m_uCurPos );

    uint length = val.Length();

#if IC_STRING_DYNAMIC
    if (m_uCurPos+length>m_uMemSize)
    {
        m_uMemSize = m_uCurPos+length;
        while (m_uMemSize%IC_STRING_MAX)
            m_uMemSize++;
        assert(m_uMemSize < IC_STRING_MAX_DYNAMIC);
        if ( !Expand( m_uMemSize ) )
            return false;
    }
#else
    if (m_uCurPos+length>IC_STRING_MAX)
        return false;
#endif
    uint cur = m_uCurPos;
    while ( cur+length >= index )
        m_Mem[cur+length] = m_Mem[cur--];

    uint i=0;
    cur = index;
    while ( i < length )
        m_Mem[cur++] = val[i++];

    m_uCurPos += length;
    return true;
}// END FUNCTION Insert( const icString& val, uint index )


/*! Replaces all instances of szPattern with szReplace
 *
 *    @param[in]    szPattern            Search pattern
 *    @param[in]    szReplace            Replaces search pattern
 *    @returns    const icString&        Modified refernce to this
 *
**/
const icString& icString::Replace( const char* szPattern,
                                   const char* szReplace )
{
    assert( szPattern );
    assert( szReplace );

    uint patLength=0;
    uint repLength=0;
    while (szPattern[patLength] != '\0')
        patLength++;

    if (patLength > 0)
    {
        while (szReplace[repLength] != '\0')
            repLength++;

        int pPos=FindFirst(szPattern);
        while (pPos > - 1)
        {
            uint cur = m_uCurPos;
            while ( cur+repLength-patLength >= (uint)pPos )
                m_Mem[cur+repLength-patLength] = m_Mem[cur--];

            uint i=0;
            cur = pPos;
            while ( i < repLength )
                m_Mem[cur++] = szReplace[i++];

            m_uCurPos += repLength-patLength;

            pPos=FindFirst(szPattern);
        }
    }
    return *this;
}// END FUNCTION Replace( const char* szFind, const char* szReplace )


/*! Replaces all instances of sPattern with sReplace
 *
 *    @param[in]    sPattern            Search pattern
 *    @param[in]    sReplace            Replaces search pattern
 *    @returns    const icString&        Modified refernce to this
 *
**/
const icString& icString::Replace( const icString& sPattern,
                                   const icString& sReplace )
{
    uint patLength=sPattern.Length();
    uint repLength=sReplace.Length();
    if (patLength > 0)
    {
        int pPos=FindFirst(sPattern);
        while (pPos > - 1)
        {
            uint cur = m_uCurPos;
            while ( cur+repLength-patLength >= (uint)pPos )
                m_Mem[cur+repLength-patLength] = m_Mem[cur--];

            uint i=0;
            cur = pPos;
            while ( i < repLength )
                m_Mem[cur++] = sReplace[i++];

            m_uCurPos += repLength-patLength;

            pPos=FindFirst(sPattern);
        }
    }
    return *this;
}// END FUNCTION Replace( const icString& sFind, const icString& sReplace )


/*! Removes a subsection of the string
 *
 *  @param      index       Where to start removing characters
 *  @returns    icString    String with those characters removed
**/
icString icString::Remove( int index )
{
    icString ret(*this);

    if (index < 0)
        return ret;    

    if ((uint)index < ret.m_uCurPos)
    {
        while (ret.m_uCurPos != (uint)index)
            ret.m_Mem[--ret.m_uCurPos] = '\0';

        ret.m_Mem[ret.m_uCurPos] = '\0';
    }

    return ret;
}// END FUNCTION Remove( int index )


/*! Removes a subsection of the string
 *
 *  @param      index       Where to start removing characters
 *  @param      length      The number of characters to remove
 *  @returns    icString    String with those characters removed
**/
icString icString::Remove( int index, int length )
{
    icString ret(*this);

    if ((uint)index < 0 || length < 0 || (uint)length > ret.Length())
        return ret;

    ret.m_uCurPos = index;
    while (ret.m_uCurPos<m_uCurPos-(index + length))
        ret.m_Mem[ret.m_uCurPos] = m_Mem[ret.m_uCurPos++ + length+1];

    ret.m_Mem[m_uCurPos] = '\0';

    return ret;
}// END FUNCTION Remove( int index, int length )