
#include "Content/icImage.h"

#include "Core/IO/icFile.h"
#include "Util/icFileName.h"
#include "Memory/icString.h"

icImage::icImage(void)
{
    m_pFile = NULL;
    m_bHeaderCurrent = false;
}

icImage::~icImage(void)
{
    if (m_pFile)
        SAFEDEL(m_pFile);
}

ICRESULT icImage::ReadHeader(const char* szFile)
{
    if (m_pFile)
        SAFEDEL(m_pFile);

    m_pFile = new icFile();

    if (ICEFAIL(m_pFile->Open(szFile, ICFMREAD_EXISTING)))
    {
        SAFEDEL(m_pFile);
        icWarningf("icImage could not open file (%s)", szFile);
        return IC_FAIL_GEN;
    }

    icFileName fileName;
    fileName.ParseFileName(szFile);

    icString ext = fileName.GetFileExtension();
    ext.MakeUpper();

    if (ext == ".TGA")
    {
        size_t sizeread = 0;
        if (ICEFAIL(m_pFile->Read(&m_header, sizeof(m_header), &sizeread)))
        {
            icWarningf("Failure loading image header: %s", szFile);
            return IC_FAIL_GEN;
        }
        m_bHeaderCurrent = true;
        return IC_OK;
    }
    //else if (ext == "BMP")
    //{
    //}
    //else if (ext == "JPG" || ext == "JPEG")
    //{
    //}

    icWarningf("Attempt to load invalid image type: %s", szFile);
    return IC_FAIL_GEN;
}

ICRESULT icImage::GetImageDataSize(uint* pSize)
{
    if (!m_bHeaderCurrent)
    {
        *pSize = 0;
        return IC_FAIL_GEN;
    }

    short width = m_header.width;
    short height = m_header.height;
    char depth = m_header.bits / 8;

    *pSize = (uint)width*height*depth;
    
    return IC_OK;
}

ICRESULT icImage::CopyImageBits(void* destination)
{
    if (!m_bHeaderCurrent)
        return IC_FAIL_GEN;


    // save off important stuff to callers pTex
    short width = m_header.width;
    short height = m_header.height;
    char depth = m_header.bits / 8;

    // get some memory and fill it up
    size_t size = (size_t)width*height*depth;

    size_t sizeread = 0;

    ICRESULT res = m_pFile->Read(destination, size, &sizeread);

    return res;
}

#ifdef ICGL
ICRESULT icImage::GetGLTexInfo(_GL_TEX_INFO* pInfo)
{
    if (!m_bHeaderCurrent)
        return IC_FAIL_GEN;


    // save off important stuff to callers pTex
    pInfo->width = m_header.width;
    pInfo->height = m_header.height;
    pInfo->depth = m_header.bits / 8;

    switch(pInfo->depth)
    {
    case 1:
        {
            pInfo->format = GL_LUMINANCE;
            pInfo->comp = GL_LUMINANCE8;
        }break;
    case 3:
        {
            pInfo->format = GL_BGR_EXT;
            pInfo->comp = GL_RGB8;
        }break;
    case 4:
        {
            pInfo->format = GL_BGRA_EXT;
            pInfo->comp = GL_RGBA8;
        }break;
    default:
        icWarning("trying to load unsupported texture!");
        return IC_FAIL_GEN;
    };

    return IC_OK;
}
#endif