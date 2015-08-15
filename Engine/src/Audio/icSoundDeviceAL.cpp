
//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "Audio/icSoundAL.h"

#include "Memory/icMinHeap.h"
#include "Core/icDebug.h"

//////////////////////////////////////////////////////////////////////////////
// SYSTEM INCLUDES
#include <string.h>

/*! c'tor
**/
icSoundDeviceAL::icSoundDeviceAL(void)
{
    m_Context = NULL;
    m_Device = NULL;
    m_bInitialized = false;
}// END FUNCTINO icSoundDeviceAL(void)


/*! d'tor
**/
icSoundDeviceAL::~icSoundDeviceAL(void)
{
    Cleanup();
}// END FUNCTION ~icSoundDeviceAL(void)


/*! Frees all memory allocated by the sound device
 *
 *  @returns        ICRESULT        Success/failure of the cleanup
**/
ICRESULT icSoundDeviceAL::Cleanup(void)
{
    if (m_bInitialized)
    {
        icSoundI* temp = icMinHeapDequeue(m_Playing, m_NumPlaying--);
        while (temp)
        {
            temp->Stop();
            icMinHeapInsert(m_Sounds,m_NumFreeSounds++,temp);
        }

        ALuint temp_buff[ICSND_MAX_SOURCE];
        ALuint temp_source[ICSND_MAX_PLAYING];

        for (int i=0; i<ICSND_MAX_SOURCE; ++i)
        {
            temp_buff[i] = m_SrcBufs[i].buffID;
            m_SrcBufs[i].buffID = 0;
        }

        for (int i=0; i<ICSND_MAX_PLAYING; ++i)
        {
            temp_source[i] = ((icSoundAL*)m_Sounds[i])->m_ALsource;
            ((icSoundAL*)m_Sounds[i])->m_ALsource = 0;
            ((icSoundAL*)m_Sounds[i])->m_bInitialized = false;

            delete ((icSoundAL*)m_Sounds[i]);
        }

        //Free buffers and source objects
        alDeleteBuffers(ICSND_MAX_SOURCE, temp_buff);
        alDeleteSources(ICSND_MAX_PLAYING, temp_source);

        //Disable context
        alcMakeContextCurrent(NULL);

        //Release context
        alcDestroyContext(m_Context);

        //Close device
        alcCloseDevice(m_Device);
    }
    return IC_OK;
}// END FUNCTION Cleanup(void)


/*! Initializes the sound device
 *
 *  @returns        ICRESULT        Success/failure of initializing the device
**/
ICRESULT icSoundDeviceAL::Initialize(void)
{
    ICRESULT res = Cleanup();
    if(ICEFAIL(res))
    {
        ALenum err = alGetError();
        icWarningf("icSoundDeviceAL::Initialize failed with error: %i", err);
        return res;
    }

    m_Device = alcOpenDevice(NULL);
    if (NULL == m_Device)
    {
        ALenum err = alGetError();
        icWarningf("icSoundDeviceAL::Initialize failed with error: %i", err);
        return IC_FAIL_GEN;
    }

    m_Context = alcCreateContext(m_Device,NULL);


    if (!alcMakeContextCurrent(m_Context))
    {
        ALenum err = alGetError();
        icWarningf("icSoundDeviceAL::Initialize failed with error: %i", err);
        return IC_FAIL_GEN;
    }

    
    ALenum err = alGetError();

    if (err)
    {
        ALenum err = alGetError();
        icWarningf("icSoundDeviceAL::Initialize failed with error: %i", err);
        return IC_FAIL_GEN;
    }


    ALuint temp_sounds[ICSND_MAX_PLAYING];
    ALuint temp_buffers[ICSND_MAX_SOURCE];
    memset(temp_sounds,0,sizeof(ALuint)*ICSND_MAX_PLAYING);
    memset(temp_buffers,0,sizeof(ALuint)*ICSND_MAX_SOURCE);

    // create the sources
    alGenSources(ICSND_MAX_PLAYING, temp_sounds);

    err = alGetError();
    if (err)
    {
        ALenum err = alGetError();
        icWarningf("icSoundDeviceAL::Initialize failed with error: %i", err);
        return IC_FAIL_GEN;
    }

    // create the buffers
    alGenBuffers(ICSND_MAX_SOURCE, temp_buffers);

    err = alGetError();
    if (err)
    {
        ALenum err = alGetError();
        icWarningf("icSoundDeviceAL::Initialize failed with error: %i", err);
        return IC_FAIL_GEN;
    }

    for (int i=0; i<ICSND_MAX_SOURCE; ++i)
        m_SrcBufs[i].buffID = temp_buffers[i];

    for (int i=0; i<ICSND_MAX_PLAYING; ++i)
    {
        m_Sounds[i] = new icSoundAL();
        m_NumFreeSounds++;
        icSoundAL* temp = (icSoundAL*)m_Sounds[i];
        temp->m_ALsource = temp_sounds[i];
        temp->m_bInitialized = true;
    }

    icMinHeapBuild(m_Sounds,m_NumFreeSounds);

    // TODO 3d stuff
    //alDopplerFactor(ALfloat factor);
    //alDopplerVelocity(ALfloat velocity);

    return IC_OK;
}// END FUNCTION Initialize(void)


/*! Call this to unload a sound object from memory
 *
 *  @param          szFile          The file to unload
 *  @returns        ICRESULT        Success/failure of unloading the resource
**/
ICRESULT icSoundDeviceAL::LoadSource(const char* szFile)
{
    icSoundBuffer pBuf;

    if (ICEFAIL(CreateSource(szFile, pBuf)))
        return IC_FAIL_GEN;

    ALenum err = AL_NO_ERROR;

    err = alGetError();

    ALenum al_format;

    switch (pBuf.format)
    {
    case ICSND_FMT_WAV_MONO16: al_format = AL_FORMAT_MONO16; break;
    case ICSND_FMT_WAV_STEREO16: al_format = AL_FORMAT_STEREO16; break;
    case ICSND_FMT_WAV_MONO8: al_format = AL_FORMAT_MONO8; break;
    case ICSND_FMT_WAV_STEREO8: al_format = AL_FORMAT_STEREO8; break;
    default: return IC_FAIL_GEN;
    }


    ALsizei al_freq = (ALsizei)pBuf.freq;
    ALsizei al_size = (ALsizei)pBuf.size;


    //put the data into an openAL buffer
    alBufferData(pBuf.buffID, al_format, pBuf.data, al_size, al_freq);

    err = alGetError();
    if (err != AL_NO_ERROR)
        return IC_FAIL_GEN;

    return IC_OK;
}// END FUNCTION LoadSource(const char* szFile)


/*! Call this to unload a sound object from memory
 *
 *  @param          szFile          The file to unload
 *  @returns        ICRESULT        Success/failure of unloading the resource
**/
ICRESULT icSoundDeviceAL::UnloadSource(const char* szFile)
{

    //release the data
    //alutUnloadWAV(alFormatBuffer, alBuffer, alBufferLen, alFreqBuffer);
    return IC_FAIL_GEN;
}// END FUNCTION UnloadSource(const char* szFile)


/*! This should be called by the main thread every frame
 *
 *  @param              fDeltaTime      Elapsed time since last frame
 *  @returns            ICRESULT        Success/failure of update
**/
ICRESULT icSoundDeviceAL::Update(const float fDeltaTime)
{
    alListenerfv(AL_POSITION,    m_Listener.pos);
    alListenerfv(AL_VELOCITY,    m_Listener.vel);
    alListenerfv(AL_ORIENTATION, m_Listener.at);


    // loop through and get this shit
    //    alGetSourcei( uiSource, AL_SOURCE_STATE, &iState);
    //} while (iState == AL_PLAYING);

    return IC_OK;
}// END FUNCTION Update(const float fDeltaTime)


/*! Gets a handle to a sound for playing
 *
 *  @param          szFile          Audio file to play
 *  @param[out]     ppStream        Handle to the sound object
 *  @returns        ICRESULT        Success/failure of playing audio
**/
ICRESULT icSoundDeviceAL::GetSound(const char* szName, icSoundI** ppSound)
{
    icSoundI* sound = GetFreeSound();
    if (!sound)
        return IC_FAIL_GEN;

    icSoundBuffer pBuf;
    
    if (ICEFAIL(FindSource(szName,pBuf)))
	{
		if (ICEFAIL(LoadSource(szName)))
		{
			return IC_FAIL_GEN;
		}

		if (ICEFAIL(FindSource(szName,pBuf)))
		{
			return IC_FAIL_GEN;
		}
	}


    icSoundAL* alSound = (icSoundAL*)sound;

    alSound->m_buf = pBuf;

    // assign the buffer to this source
    alSourcei(alSound->m_ALsource, AL_BUFFER, pBuf.buffID);

    ALenum err = alGetError();
    if (err != AL_NO_ERROR)
        return IC_FAIL_GEN;

	*ppSound = sound;

    return IC_OK;
}// END FUNCTION GetSound(const char* szName, icSoundI** ppSound)


/*! Used to play an ambient sound
 *
 *  Note, if you call this on a file that has not been Loaded
 *  (see icSoundDeviceAL::LoadSource), it will keep this audio
 *  loaded in memory until you call UnloadSource or the device
 *  is destroyed.
 *
 *  @param          szFile          Audio file to play
 *  @param          soundParam      How to play that audio
 *  @param[out]     ppStream        Handle to the sound object
 *  @returns        ICRESULT        Success/failure of playing audio
**/
ICRESULT icSoundDeviceAL::Play(const char* szName,
                               const icSoundParam& soundParams,
                               icSoundI** ppSound)
{
    ALenum err = AL_NO_ERROR;

    err = alGetError();

    icSoundI* sound = GetFreeSound();
    if (!sound)
        return IC_FAIL_GEN;

    icSoundBuffer pBuf;
    
    if (ICEFAIL(FindSource(szName,pBuf)))
        LoadSource(szName);

    

    if (ICEFAIL(FindSource(szName,pBuf)))
        return IC_FAIL_GEN;


    icSoundAL* alSound = (icSoundAL*)sound;

    alSound->m_buf = pBuf;
    alSound->m_Params = soundParams;

    // assign the buffer to this source
    alSourcei(alSound->m_ALsource, AL_BUFFER, pBuf.buffID);

    err = alGetError();
    if (err != AL_NO_ERROR)
        return IC_FAIL_GEN;

    alSound->Play();

    *ppSound = sound;

    return IC_OK;
}// END FUNCTION Play(const char* szName,
 //                   const icSoundParam& soundParams,
 //                   icSoundI** ppSound)


/*! Used to play a 3d sound
 *
 *  Note, if you call this on a file that has not been Loaded
 *  (see icSoundDeviceAL::LoadSource), it will keep this audio
 *  loaded in memory until you call UnloadSource or the device
 *  is destroyed.
 *
 *  @param          szFile          Audio file to play
 *  @param          soundParam      How to play that audio
 *  @param[out]     ppStream        Handle to the sound object
 *  @returns        ICRESULT        Success/failure of playing audio
**/
ICRESULT icSoundDeviceAL::Play3D(const char* szName,
                                 const icSoundParam& soundParams,
                                 icSoundI** ppSound)
{
    icSoundI* sound = GetFreeSound();
    if (!sound)
        return IC_FAIL_GEN;

    icSoundBuffer pBuf;

    if (ICEFAIL(FindSource(szName,pBuf)))
        LoadSource(szName);

    if (ICEFAIL(FindSource(szName,pBuf)))
        return IC_FAIL_GEN;


    icSoundAL* alSound = (icSoundAL*)sound;

    alSound->m_buf = pBuf;

    alSound->m_Params = soundParams;

    // assign the buffer to this source
    alSourcei(alSound->m_ALsource, AL_BUFFER, pBuf.buffID);

    alSound->Play3d();

    *ppSound = sound;

    return IC_OK;
}// END FUNCTION Play3D(const char* szName,
 //                     const icSoundParam& soundParams,
 //                     icSoundI** ppSound)


/*! Used to play ambient streaming audio
 *
 *  @param          szFile          Audio file to stream
 *  @param          soundParam      How to play that stream
 *  @param[out]     ppStream        Handle to the stream
 *  @returns        ICRESULT        Success/failure of starting stream
**/
ICRESULT icSoundDeviceAL::PlayStream(const char* szFile,
                                     const icSoundParam& soundParams,
                                     icSoundStreamI** ppStream)
{
    return IC_FAIL_GEN;
}// END FUNCTION PlayStream(const char* szFile,
 //                         const icSoundParam& soundParams,
 //                         icSoundStreamI** ppStream)


/*! Used to play streaming audio that has spatial properties
 *
 *  @param          szFile          Audio file to stream
 *  @param          soundParam      How to play that stream
 *  @param[out]     ppStream        Handle to the stream
 *  @returns        ICRESULT        Success/failure of starting stream
**/
ICRESULT icSoundDeviceAL::PlayStream3D(const char* szFile,
                                       const icSoundParam& soundParams,
                                       icSoundStreamI** ppStream)
{
    return IC_FAIL_GEN;
}// END FUNCTION PlayStream3D(const char* szFile,
 //                           const icSoundParam& soundParams,
 //                           icSoundStreamI** ppStream)



/*! Releases a sound object for reuse
 *
 *  @param          pSound          Pointer to a sound object
 *  @returns        ICRESULT        Success/failure of releasing sound
**/
ICRESULT icSoundDeviceAL::ReleaseSound(icSoundI* pSound)
{
    icSoundAL* alsound = (icSoundAL*)pSound;
    alsound->Stop();
    // TODO: zero this!
    //alsound->m_buf
    return FreeSound(pSound);
}// END FUNCTION ReleaseSound(icSoundI* pSound)


/*! Releases a audio stream for reuse
 *
 *  @param          pStream         Pointer to stream object
 *  @returns        ICRESULT        Success/failure of releasing stream
**/
ICRESULT icSoundDeviceAL::ReleaseStream(icSoundStreamI* pStream)
{
    icSoundStreamAL* alstream = (icSoundStreamAL*)pStream;
    alstream->Stop();
    return FreeStream(pStream);
}// END FUNCTION ReleaseStream(icSoundStreamI* pStream)