#ifndef __IC_CONTENT_LOADER_H__
#define __IC_CONTENT_LOADER_H__


#include "Core/icGlobals.h"
#include "Core/GXDevice/icGXDevice.h"

#include "Audio/icSound.h"


#define _IC_MAX_TEXUTRE 256
#define _IC_MAX_MODEL   256

class icTexture;
class icModel;

/*! Provides an interface for loading game assets
**/
class icContentLoader
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icContentLoader(icGXDevice* pDevice, icSoundDeviceI* pAudio);
    ~icContentLoader(void);

    ICRESULT Initialize(void);
    ICRESULT FreeContent(void);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    template <class T>
    ICRESULT Load(const char* szFileName, T** ppObj);

    // TODO: offer asynchronous loading
    //template <class T>
    //void LoadAsync(const char* szFileName, T** ppObj, );

    template <class T>
    ICRESULT Unload(T** ppObj);

    //////////////////////////////////////////////////////////////////////////
    // ACCESS
    FORCE_INLINE icGXDevice* GetDevice(void) {return m_pDevice;};
    FORCE_INLINE icSoundDeviceI* GetAudio(void) {return m_pAudio;};

private:
    icGXDevice*     m_pDevice;      //! Passed in pointer to grafx device
    icSoundDeviceI* m_pAudio;       //! Passed in pointer to audio device

    icTexture**     m_texHeap;
    icModel**       m_modelHeap;

    int             m_iNumTex;
    int             m_iNumModel;
    int             m_iMaxTex;
    int             m_iMaxModel;
};

#endif //__IC_CONTENT_LOADER_H__