#ifndef __IC_IMAGE_H__
#define __IC_IMAGE_H__


#include "Core/icGlobals.h"

#ifdef _MSC_VER
#pragma pack(push)  /* push current alignment to stack */
#pragma pack(1)     /* set alignment to 1 byte boundary */
#endif


/*! Targa file header
*/
struct _TGAHeader
{
    uchar  identsize;       // size of ID field that follows 18 byte header
    uchar  colourmaptype;   // type of colour map 0=none, 1=has palette
    uchar  imagetype;       // type of image

    short colourmapstart;   // first colour map entry in palette
    short colourmaplength;  // number of colours in palette
    uchar  colourmapbits;   // number of bits per palette entry 15,16,24,32

    short xstart;           // image x origin
    short ystart;           // image y origin
    short width;            // image width in pixels
    short height;           // image height in pixels
    uchar  bits;            // image bits per pixel 8,16,24,32
    uchar  descriptor;      // image descriptor bits (vh flip bits)
    // pixel data follows header  
#ifdef _MSC_VER
};
#else
}__attribute__((packed));
#endif

#ifdef ICGL

#include "Core/GXDevice/icGLext.h"

/*! struct to get opengl texture info
**/
struct _GL_TEX_INFO
{
    int width;
    int height;
    int depth;
    GLint format;
    GLint comp;
};
#endif

#ifdef _MSC_VER
#pragma pack(pop)   /* restore original alignment from stack */
#endif

class icFile;


/*! Provides an interface to get device ready bits from textures
**/
class icImage
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icImage(void);
    ~icImage(void);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    ICRESULT ReadHeader(const char* szFile);

    ICRESULT GetImageDataSize(uint* pSize);

    ICRESULT CopyImageBits(void* destination);

#ifdef ICGL
    ICRESULT GetGLTexInfo(_GL_TEX_INFO* pInfo);
#endif

private:
    icFile*     m_pFile;

    bool        m_bHeaderCurrent;
    _TGAHeader  m_header;// temporary, this should be a universal object
};

#endif //__IC_IMAGE_H__