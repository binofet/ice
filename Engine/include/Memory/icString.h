#ifndef __IC_STRING_H__
#define __IC_STRING_H__

#include "Core/icGlobals.h"


#define IC_STRING_DYNAMIC 1 // 0 for static IC_STRING_MAX strings, 1-dyanmic
#define IC_STRING_MAX_DYNAMIC (1024*1024) // if using dynamic strings, maxsize
#define IC_STRING_MAX (512) // used for malloc chunks also

class icString
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icString( void );
    icString( const char* szInitial );
    icString( const icString& refCopy );
    ~icString( void );

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS

    const int Compare( const icString& val ) const;

    const int FindFirst( const char cPattern ) const;
    const int FindFirst( const icString& sPattern ) const;
    const int FindFirst( const char* szPattern ) const;

    const int FindLast( const char cPattern ) const;
    const int FindLast( const icString& sPattern) const;
    const int FindLast( const char* szPattern ) const;

    const bool Insert( const char val, uint index );
    const bool Insert( const char* val, uint index );
    const bool Insert( const icString& val, uint index);

    const icString& MakeUpper( void );
    const icString& MakeLower( void );

    const icString& Replace( char cPattern, char cReplace);
    const icString& Replace( const char* szPattern, const char* szReplace);
    const icString& Replace( const icString& sPattern,
                             const icString& sReplace);

    icString Remove( int index );
    icString Remove( int index, int length );

    //////////////////////////////////////////////////////////////////////////
    // OPERATORS
    char operator[]( uint index ) const;
    char& operator[]( uint index );
    icString operator+( const icString& Rhs );
    icString operator+( const char* Rhs );
    const icString& operator+=( const icString& Rhs );
    const icString& operator+=( const char* Rhs );
    const icString& operator=( const icString& Rhs );
    const icString& operator=( const char* Rhs );

    const bool operator==( const icString& Rhs ) const;
    const bool operator!=( const icString& Rhs ) const;
    const bool operator>=( const icString& Rhs ) const;
    const bool operator<=( const icString& Rhs ) const;
    const bool operator>( const icString& Rhs ) const;
    const bool operator<( const icString& Rhs ) const;

    //////////////////////////////////////////////////////////////////////////
    // ACCESS
    const char* ToChar( void ) const;

    //////////////////////////////////////////////////////////////////////////
    // INQUIRY
    const uint Length( void ) const;
    const uint BufferSize( void ) const;

private:
    //////////////////////////////////////////////////////////////////////////
    // PRIVATE FUNCTIONS
    const bool Expand( uint newSize );

    //////////////////////////////////////////////////////////////////////////
    // PRIVATE MEMBERS
#if IC_STRING_DYNAMIC
    char*       m_Mem;                   //! Dynamic strings
#else
    char        m_Mem[IC_STRING_MAX];    //! Fixed max size strings
#endif
    uint        m_uCurPos;               //! Current pos in buffer
    uint        m_uMemSize;              //! Size of buffer (not str length)
};

#include "Memory/icString.inl"

#endif //__ICE_STRING_H__



