
//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "Audio/icSound.h"
#include "Memory/icMinHeap.h"

//////////////////////////////////////////////////////////////////////////////
// SYSTEM INCLUDES
#include <stdlib.h>
#include <string.h>

bool Load_Wave_File(const char *fname, icSoundBuffer* header)
{
    FILE *fp;
#ifdef WIN32
    fopen_s(&fp,fname,"rb");
#else
    fp = fopen(fname,"rb");
#endif
    if (!fp)
        return false;

    _ICSND_WAV_HEADER head;

    fread(&head,sizeof(_ICSND_WAV_HEADER),1,fp);

    header->freq = head.sample_rate;
    if (head.num_channels == 1)
    {
        if (head.bits_per_samp == 8)
            header->format = ICSND_FMT_WAV_MONO8;
        else if (head.bits_per_samp == 16)
            header->format = ICSND_FMT_WAV_MONO16;
        else
        {
            icWarningf("ICE Audio does not support the format of: %s", fname);
            return false;
        }
    }
    else if (head.num_channels == 2)
    {
        if (head.bits_per_samp == 8)
            header->format = ICSND_FMT_WAV_STEREO8;
        else if (head.bits_per_samp == 16)
            header->format = ICSND_FMT_WAV_STEREO16;
        else
        {
            icWarningf("ICE Audio does not support the format of: %s", fname);
            return false;
        }
    }
    else
    {
        icWarningf("ICE Audio does not support the format of: %s", fname);
        return false;
    }

    header->data = malloc(head.subchunk2_size);
    header->size = head.subchunk2_size;
    
    fread(header->data,head.subchunk2_size,1,fp);

    fclose(fp);

    return true;
}


/*! c'tor
**/
icSoundDeviceI::icSoundDeviceI(void)
{
    memset(&m_Listener,0,sizeof(m_Listener)); 
    memset(m_SrcBufs,0,sizeof(icSoundBuffer)*ICSND_MAX_SOURCE);
    memset(m_Sounds,0,sizeof(icSoundI*)*ICSND_MAX_PLAYING);
    memset(m_Playing,0,sizeof(icSoundI*)*ICSND_MAX_PLAYING);
    memset(m_Streams,0,sizeof(icSoundStreamI*)*ICSND_MAX_STREAMS);


    m_NumPlaying = 0;                   //! Num playing sounds
    m_NumFreeSounds = 0;                //! Num free sounds
    m_NumStreaming = 0;                 //! Num playing streams
    m_NumFreeStreams = 0;               //! Num free streams
    m_NumSrc = 0;                       //! Num src buffs loaded
}// END FUNCTINO icSoundDeviceI(void)


/*! Creates a source buffer
 *
 *  @param      szFile                File to load
 *  @returns    buffer                Reference to buffer object to fill
 *  @returns    ICRESULT              Pointer to sound buffer (NULL if failed)
**/
ICRESULT icSoundDeviceI::CreateSource(const char* szFile, icSoundBuffer& buffer)
{
    // we don't allow double loading!
    if (ICEOK(FindSource(szFile,buffer)))
        return IC_FAIL_GEN;

    if (m_NumSrc < ICSND_MAX_SOURCE-1)
    {
        icSoundBuffer temp = m_SrcBufs[m_NumSrc];
        temp.szName = szFile;

        // LOAD THE SOUND BITS
        if (Load_Wave_File(szFile, &temp))
        {
            // insert the sound buffer into the list
            icMinHeapInsert(m_SrcBufs,m_NumSrc++,temp);

            int i = icMinHeapFind(m_SrcBufs,m_NumSrc,temp);
            buffer = m_SrcBufs[i];
            return IC_OK;
        }
    }
    return IC_FAIL_GEN;
}// END FUNCTION CreateSource(const char* szFile, icSoundBuffer& buffer)


/*! Finds a source buffer by name
 *
 *  @param      szFile              Name of source to find
 *  @returns    buffer                Reference to buffer object to fill
 *  @returns    ICRESULT              Pointer to sound buffer (NULL if failed)
**/
ICRESULT icSoundDeviceI::FindSource(const char* szFile, icSoundBuffer& buffer)
{
    icSoundBuffer temp;
    temp.szName = szFile;
    int i = icMinHeapFind(m_SrcBufs,m_NumSrc,temp);
    if (i >= 0)
    {
        buffer = m_SrcBufs[i];
        return IC_OK;
    }

    return IC_FAIL_GEN;
}// END FUNCTION FindSource(const char* szFile)


/*! Returns a sound that is free to be played (if any)
 *
 *  @returns    icSound*            Pointer to sound (NULL if none)
**/
icSoundI* icSoundDeviceI::GetFreeSound(void)
{
    if (m_NumPlaying < ICSND_MAX_PLAYING-1)
    {
        icSoundI* sound = icMinHeapDequeue(m_Sounds,m_NumFreeSounds--);
        icMinHeapInsert(m_Playing,m_NumPlaying++,sound);
        return sound;
    }
    return NULL;
}// END FUNCTION GetFreeSound(void)


/*! Frees a sound object that is no longer playing
 *
 *  @param      pSound              Pointer to sound object
 *  @returns    ICRSESULT           Success/failure of freeing sound
**/
ICRESULT icSoundDeviceI::FreeSound(icSoundI* pSound)
{
    int i = icMinHeapFind(m_Playing,m_NumPlaying,pSound);
    if (i >= 0)
    {
        icMinHeapDecreaseKey(m_Playing,i,(icSoundI*)0);
        icMinHeapDequeue(m_Playing,m_NumPlaying--);
        icMinHeapInsert(m_Sounds,m_NumFreeSounds++,pSound);
        return IC_OK;
    }
    return IC_FAIL_GEN;
}// END FUNCTION FreeSound(icSoundI* pSound);


/*! Returns a stream that is free to be played (if any)
 *
 *  @returns    icSoundStreamI*     Pointer to stream object (NULL if none)
**/
icSoundStreamI* icSoundDeviceI::GetFreeStream(void)
{
    return NULL;
}// END FUNCTION GetFreeStream(void)


/*! Frees a stream object that is no longer playing
 *
 *  @param      pStream             Pointer to stream object
 *  @returns    ICRESULT            Success/failure of freeing stream
**/
ICRESULT icSoundDeviceI::FreeStream(icSoundStreamI* pStream)
{
    return IC_FAIL_GEN;
}// END FUNCTION FreeStream(icSoundStreamI* pStream)