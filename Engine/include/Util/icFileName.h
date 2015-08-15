#ifndef __IC_FILE_NAME_H__
#define __IC_FILE_NAME_H__

#include "Memory/icString.h"

class icFileName
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icFileName( void );
    ~icFileName( void );

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    ICRESULT ParseFileName( const char* szFileName, uint length=0 );
    ICRESULT ParseFileName( const icString& sFileName );

    //////////////////////////////////////////////////////////////////////////
    // ACCESS
    inline const char* GetDrive( void ) { return m_Drive; };
    inline const char* GetPath( void ) { return m_Path; };
    inline const char* GetFileName( void ) { return m_FileName; };
    inline const char* GetFileExtension( void ) { return m_Extension; };

private:
    //////////////////////////////////////////////////////////////////////////
    // PRIVATE METHODS
    void SplitPath( const char* szFileName );

    //////////////////////////////////////////////////////////////////////////
    // PRIVATE MEMBERS
    char        m_Mem[512];        //! Memory pool
    const char*    m_Drive;        //! Pointer to the drive letter
    const char*    m_Path;            //! Pointer to the path
    const char*    m_FileName;        //! Pointer to the filename
    const char* m_Extension;    //! Pointer to the extention
};

#endif //__IC_FILE_NAME_H__

