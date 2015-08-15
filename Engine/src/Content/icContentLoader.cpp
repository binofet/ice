
#include "Content/icContentLoader.h"

#include "Render/VertBuffer/icVertBuffer.h"
#include "Render/IndexBuffer/icIndexBuf.h"
#include "Render/Texture/icTexture.h"
#include "Render/icModel.h"

#include "Core/IO/icFile.h"

/*! c'tor
**/
icContentLoader::icContentLoader(icGXDevice* pDevice, icSoundDeviceI* pAudio)
{
    m_pAudio = pAudio;
    m_pDevice = pDevice;

    m_texHeap = NULL;
    m_modelHeap = NULL;

    m_iNumTex = 0;
    m_iNumModel = 0;
    m_iMaxTex = _IC_MAX_TEXUTRE;
    m_iMaxModel = _IC_MAX_MODEL;
}// END FUNCTION icContentLoader(icGXDevice* pDevice, icSoundDeviceI* pAudio)


/*! d'tor
**/
icContentLoader::~icContentLoader(void)
{
    FreeContent();
}// END FUNCTION ~icContentLoader(void)


/*! Initializes the content loader
 *
 *  @returns        ICRESULT        Success/failure of initialization
**/
ICRESULT icContentLoader::Initialize(void)
{
    // make sure we have a device
    if (!m_pDevice)
        return IC_FAIL_NO_DEVICE;

    // make sure we are starting from a clean state
    if (ICEFAIL(FreeContent()))
    {
        icWarning("Content loader could not initialze");
        return IC_FAIL_GEN;
    }

    // create the space for models and textures
    m_texHeap = (icTexture**)malloc(sizeof(icTexture*)*_IC_MAX_TEXUTRE);
    m_modelHeap = (icModel**)malloc(sizeof(icModel*)*_IC_MAX_MODEL);

    if (m_texHeap && m_modelHeap)
    {
        // zero the mem
        memset(m_texHeap, 0, sizeof(icTexture*)*_IC_MAX_TEXUTRE);
        memset(m_modelHeap, 0, sizeof(icModel*)*_IC_MAX_MODEL);
        return IC_OK;
    }

    return IC_FAIL_OUT_OF_MEM;
}// END FUNCTION Initialize(void)


/*! Frees all loaded content
 *
 *  @returns        ICRESULT        Success/failure of freeing content
**/
ICRESULT icContentLoader::FreeContent(void)
{
    if (m_pDevice)
    {
        // free allocated textures
        if (m_texHeap)
        {
            for (int i=0; i<m_iNumTex; ++i)
            {
                m_pDevice->ReleaseTex(m_texHeap[i]);
                m_texHeap[i] = NULL;
            }
        }

        // free allocated models (vert buffs and index buffs)
        if (m_modelHeap)
        {
            for (int i=0; i<m_iNumModel; ++i)
            {
                m_pDevice->ReleaseVB(m_modelHeap[i]->m_pVB);
                m_pDevice->ReleaseIB(m_modelHeap[i]->m_pIB);
                m_modelHeap[i] = NULL;
            }
        }

        return IC_OK;
    }
    return IC_FAIL_NO_DEVICE;
}// END FUNCTION FreeContent(void)


/*! Template load function
 *
 *      This is a guaranteed failure, the specialization doesn't exist for T
 *      if the compiler uses this function.  Should maybe make this a compiler
 *      error.
**/
template <class T>
ICRESULT icContentLoader::Load(const char* szFileName, T** ppObj)
{
    *ppObj = NULL;
    return IC_NOT_IMPL;
}// END FUNCTION Load(const char* szFileName, T** ppObj)


/*! Audio loader
 *
 *  @param          szFileName      Name of the sound file 
 *  @param[in/out]  ppObj           Storage place for handle to sound object
 *  @returns        ICRESULT        Success/failure of sound load
**/
template<>
ICRESULT icContentLoader::Load<icSoundI>(const char* szFileName,
                                         icSoundI** ppObj)
{
    m_pAudio->LoadSource(szFileName);
    m_pAudio->GetSound(szFileName, ppObj);

    return IC_OK;
}// END FUNCTION Load<icSoundI>(const char* szFileName, icSoundI** ppObj)


/*! Template unload function
 *
 *      This is a guaranteed failure, the specialization doesn't exist for T
 *      if the compiler uses this function.  Should maybe make this a compiler
 *      error.
**/
template <class T>
ICRESULT icContentLoader::Unload(T** ppObj)
{
    return IC_FAIL_GEN;
}// END FUNCTION Unload(T* object)
