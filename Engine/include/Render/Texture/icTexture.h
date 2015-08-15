#ifndef __IC_TEXTURE_H__
#define __IC_TEXTURE_H__

#include "Core/icGlobals.h"

#define ICTEX_FMT_LUM8      1
//#define ICTEX_FMT_RBGA32    2
#define ICTEX_FMT_RBG24     2
#define ICTEX_FMT_RBGA32    3


#define ICTEX_MAX_NAME      256


/*! Defines parameters about a texture
**/
struct icTexDef
{
    uint    width;         //! Width in pixels
    uint    height;        //! Height in pixels
    uint    mipLevels;     //! Number of mip map levels
    uint    format;        //! Texture format
    uchar   usage;         //! How this buffer is to be used
};


struct icTexLock
{
    uint level;
    uint width;
    uint height;
    uint format;
    uint pitch;
    void* pData;
};


/*! Provides an interface for textures
**/
class icTexture
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icTexture( void )
    {
        m_ptrTex = NULL;
        m_bLocked = false;
    };
    virtual ~icTexture( void ) { Cleanup(); };

    virtual ICRESULT Cleanup( void ) { return IC_OK; };

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT Lock( icTexLock* pTexLock )=0;
    virtual ICRESULT Unlock( uint level=0 )=0;

    //////////////////////////////////////////////////////////////////////////
    // ACCESS

    //! Gets the texture object
    //\returns void** Pointer to pointer of texture
    inline void** GetTexture( void ) { return &m_ptrTex; };

    //! Gets a pointer to the texture definition struct
    //\returns icTexDef* Pointer to texture definition
    inline icTexDef* GetTexDef( void ) { return &m_TexDef; };

    inline bool HasAlpha(void) { return m_TexDef.format == ICTEX_FMT_RBGA32; };

    int operator<(const icTexture& Rhs);
    int operator>(const icTexture& Rhs);
    int operator<=(const icTexture& Rhs);
    int operator>=(const icTexture& Rhs);
    int operator==(const icTexture& Rhs);
    int operator!=(const icTexture& Rhs);

    char        m_szName[ICTEX_MAX_NAME]; //! Texture path + name
protected:
    //////////////////////////////////////////////////////////////////////////
    // PROTECTED MEMBERS
    void*       m_ptrTex;        //! Pointer to texture data
    bool        m_bLocked;       //! Tells whether the texture is locked

    icTexDef    m_TexDef;        //! Defines this texture
};


FORCE_INLINE int icTexture::operator<(const icTexture& Rhs)
{
    return strcmp(m_szName,Rhs.m_szName);
}

FORCE_INLINE int icTexture::operator>(const icTexture& Rhs)
{
    return strcmp(m_szName,Rhs.m_szName);
}

FORCE_INLINE int icTexture::operator<=(const icTexture& Rhs)
{
    return strcmp(m_szName,Rhs.m_szName);
}

FORCE_INLINE int icTexture::operator>=(const icTexture& Rhs)
{
    return strcmp(m_szName,Rhs.m_szName);
}

FORCE_INLINE int icTexture::operator==(const icTexture& Rhs)
{
    return strcmp(m_szName,Rhs.m_szName);
}

FORCE_INLINE int icTexture::operator!=(const icTexture& Rhs)
{
    return strcmp(m_szName,Rhs.m_szName);
}

#endif //__IC_TEXTURE_H__

