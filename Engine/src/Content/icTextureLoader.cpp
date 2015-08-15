#include "Content/icContentLoader.h"

#include "Render/Texture/icTexture.h"


/*! Texture loader
 *
 *  @param          szFileName      Name of the texture to load
 *  @param[in/out]  ppObj           Storage place for loaded texture
 *  @returns        ICRESULT        Success/failure of texture load
**/
template<>
ICRESULT icContentLoader::Load<icTexture>(const char* szFileName,
                                          icTexture** ppObj)
{
    // TODO: reference counted objects!
    return m_pDevice->CreateTexFromFile(ppObj, szFileName);
    //return IC_OK;
}// END FUNCTION Load<icTexture>(const char* szFileName, icTexture** ppObj)


/*! Texture unloader
 *
 *  @param[in/out]  ppObj           Loaded texture
 *  @returns        ICRESULT        Success/failure of texture unload
**/
template<>
ICRESULT icContentLoader::Unload<icTexture>(icTexture** ppObj)
{
    if (m_pDevice)
    {
        // TODO: reference counting!
        //if (refcount == 0)
            m_pDevice->ReleaseTex(*ppObj);

        *ppObj = NULL;
        return IC_OK;
    }
    return IC_FAIL_NO_DEVICE;
}// END FUNCTION Unload<icTexture>(icTexture** ppObj)
