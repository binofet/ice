#ifndef __IC_SOUND_AL_H__
#define __IC_SOUND_AL_H__

#include "Audio/icSound.h"

#include "openAL/al.h"
#include "openAL/alc.h"

/*! Implements the sound interface under openAL
**/
class icSoundAL : public icSoundI
{
public:
    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT Play(void);
    virtual ICRESULT Play3d(void);
    virtual ICRESULT Stop(bool bSavePos=false);
    virtual ICRESULT Pause(void);

    virtual ICRESULT SetParams(const icSoundParam& params);

private:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icSoundAL(void);
    virtual ~icSoundAL(void);

    
    ALuint      m_ALsource;
    bool        m_bInitialized;

    friend class icSoundDeviceAL;
};

/*! Implements the sound stream interface under openAL
**/
class icSoundStreamAL : public icSoundStreamI
{
public:
    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual bool IsStreaming(void)=0;

private:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    virtual ~icSoundStreamAL(void);

    friend class icSoundDeviceAL;
};


/*! Provides an interface to a sound device
**/
class icSoundDeviceAL : public icSoundDeviceI
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icSoundDeviceAL(void);
    virtual ~icSoundDeviceAL(void);

    virtual ICRESULT Cleanup(void);
    
    virtual ICRESULT Initialize(void);

    virtual ICRESULT LoadSource(const char* szFile);
    virtual ICRESULT UnloadSource(const char* szFile);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT Update(const float fDeltaTime);

    virtual ICRESULT GetSound(const char* szName, icSoundI** ppSound);

    virtual ICRESULT Play(const char* szName,
                          const icSoundParam& soundParams,
                          icSoundI** ppSound);

    virtual ICRESULT Play3D(const char* szName,
                            const icSoundParam& soundParams,
                            icSoundI** ppSound);

    virtual ICRESULT PlayStream(const char* szFile,
                                const icSoundParam& soundParams,
                                icSoundStreamI** ppStream);

    virtual ICRESULT PlayStream3D(const char* szFile,
                                  const icSoundParam& soundParams,
                                  icSoundStreamI** ppStream);

    virtual ICRESULT ReleaseSound(icSoundI* pSound);

    virtual ICRESULT ReleaseStream(icSoundStreamI* pStream);

private:
    bool            m_bInitialized;

    ALCcontext*     m_Context;
    ALCdevice*      m_Device;
};

#endif //__IC_SOUND_AL_H__