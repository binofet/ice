
#include "Audio/icSoundAL.h"




/*! c'tor
**/
icSoundAL::icSoundAL(void)
{
    // TODO: zero this
    //m_buf = NULL;

    m_bPlaying = false;
    m_b3d = false;
    m_bPaused = false;
    m_bInitialized = false;
}// END FUNCTION icSoundAL(void)


/*! d'tor
**/
icSoundAL::~icSoundAL(void)
{
}// END FUNCTION ~icSoundAL(void)


/*! Sets the playback parameters for this sound
 *
 *  @param      params      The new parameters for this sound
 *  @returns    ICRESULT    Success/failure of changing params
**/
ICRESULT icSoundAL::SetParams(const icSoundParam& params)
{
    m_Params = params;
    m_Params.fPitch = icClamp(m_Params.fPitch,0.0f,1.0f);
    m_Params.fVol = icClamp(m_Params.fVol,0.0f,1.0f);
    if (m_bInitialized)
    {
        alSourcef(m_ALsource, AL_PITCH,    m_Params.fPitch);
        alSourcef(m_ALsource, AL_GAIN,     m_Params.fVol);
        alSourcei(m_ALsource, AL_LOOPING,  m_Params.bLoop);

        if (m_b3d)
        {
            alSourcefv(m_ALsource, AL_POSITION, m_Params.pos);
            alSourcefv(m_ALsource, AL_VELOCITY, m_Params.vel);
        }
    }
    return IC_OK;
}// END FUNCTION SetParams(const icSoundParam& params)


/*! Plays the sound
 *
 *  @returns    ICRESULT     Success/failure of playing sound
**/
ICRESULT icSoundAL::Play(void)
{
    if (!m_buf.data || !m_bInitialized)
        return IC_FAIL_GEN;

    // are we looping?
    if (m_Params.bLoop)
        alSourcei(m_ALsource, AL_LOOPING, AL_TRUE);
    else
        alSourcei(m_ALsource, AL_LOOPING, AL_FALSE);


    // play the sound
    alSourcePlay(m_ALsource);

    return IC_OK;
}// END FUNCTION Play(void)


/*! Plays the sound in 3d
 *
 *  @returns    ICRESULT     Success/failure of playing sound
**/
ICRESULT icSoundAL::Play3d(void)
{
    if (!m_buf.data || !m_bInitialized)
        return IC_FAIL_GEN;

    // are we looping?
    if (m_Params.bLoop)
        alSourcei(m_ALsource, AL_LOOPING, AL_TRUE);

    //set source position
    alSourcefv(m_ALsource,AL_POSITION, m_Params.pos);

    //set source velocity
    alSourcefv(m_ALsource,AL_VELOCITY, m_Params.vel);

    // play the sound
    alSourcePlay(m_ALsource);

    return IC_OK;
}// END FUNCTION Play(void)


/*! Stops sound from playing
 *
 *  @param      bSavePos      Set to true to be able to play from cur pos
 *  @returns    ICRESULT      Success/failure of stopping sound
**/
ICRESULT icSoundAL::Stop(bool bSavePos)
{
    if (m_bPlaying)
    {
        if (bSavePos)
            alSourcePause(m_ALsource);
        else
            alSourceStop(m_ALsource);
    }
    return IC_WARN_GEN;
}// END FUNCTION Stop(void)


/*! Pauses or unpauses a sound
 *
 *  @returns    ICRESULT      Success/failure of pausing/unpausing
**/
ICRESULT icSoundAL::Pause(void)
{
    if (!m_bPaused)
    {
        m_bPaused = true;
        alSourcePause(m_ALsource);
    }
    else
    {
        m_bPaused = false;
        alSourcePlay(m_ALsource);
    }
    return IC_FAIL_GEN;
}// END FUNCTION Pause(void)