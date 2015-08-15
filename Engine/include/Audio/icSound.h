#ifndef __IC_SOUND_H__
#define __IC_SOUND_H__

#include "Math/Vector/icVector3.h"
#include "Memory/icString.h"

#include "Audio/icSoundHeader.h"


#define ICSND_MAX_SOURCE        (64)   //! Maximum number of source buffers    
#define ICSND_MAX_PLAYING       (16)    //! Maximum number of simultenous sounds
#define ICSND_MAX_STREAMS       (4)     //! Maximum number of streaming sounds


//! TODO, make this more clear, do we need another one for realtime conversion
//      formats?
/*! Describes a PCM WAV format
**/
enum ICSND_FMT
{
    ICSND_FMT_UKNOWN=0,         //! Invalid pcm wav format
    ICSND_FMT_WAV_MONO8,        //! pcm mono 8 bit sampled audio
    ICSND_FMT_WAV_STEREO8,
    ICSND_FMT_WAV_MONO16,
    ICSND_FMT_WAV_STEREO16,
    ICSND_FMT_NUM_FORMATS
};


/*! Wraps up a sound buffer
**/
struct icSoundBuffer
{
    icSoundBuffer(){};
    icSoundBuffer(const icSoundBuffer& Rhs)
    {
        szName = Rhs.szName;
        data = Rhs.data;
        header = Rhs.header;
        format = Rhs.format;
        freq = Rhs.freq;
        size = Rhs.size;
        buffID = Rhs.buffID;
    };
    const bool operator < (const icSoundBuffer& Rhs) const
    {
        return szName < Rhs.szName;
    };
    const bool operator <= (const icSoundBuffer& Rhs) const
    {
        return szName <= Rhs.szName;
    };
    const bool operator > (const icSoundBuffer& Rhs) const
    {
        return szName > Rhs.szName;
    };
    const bool operator >= (const icSoundBuffer& Rhs) const
    {
        return szName >= Rhs.szName;
    };
    const bool operator == (const icSoundBuffer& Rhs) const
    {
        return szName == Rhs.szName;
    };

    const icSoundBuffer& operator = (const icSoundBuffer& Rhs)
    {
        szName = Rhs.szName;
        data = Rhs.data;
        header = Rhs.header;
        format = Rhs.format;
        freq = Rhs.freq;
        size = Rhs.size;
        buffID = Rhs.buffID;

        return *this;
    };



    icString    szName;         //! Name of this buffer
    void*       data;           //! Pointer to data
    void*       header;         //! Pointer to header
    ICSND_FMT   format;         //! Type of buffer
    int         freq;           //! Frequency of the audio
    int         size;           //! Size of the data
    uint        buffID;         //! ID of the buffer
};


/*! Structure used to store information about a sound object
**/
struct icSoundParam
{
    icVector3   pos;        //! Position of the sound
    icVector3   vel;        //! Velocity of the soun
    float       fVol;       //! Volume of the sound (0.0f - 1.0f)
    float       fPitch;     //! Pitch of the sound (0.0f - 1.0f)
	float       fPan;
    bool        bLoop;      //! Is the sound looping
    bool        bPaused;    //! Is the sound paused
};


/*! Structure used to precisely define what a listener is
 *  and how they are moving/oriented in the world.
**/
struct icSoundListener
{
    icVector3   pos;        //! Position of listener
    icVector3   vel;        //! Velocity of listener
    icVector3   at;         //! Orientation of listener (at direction)
    icVector3   up;         //! Orientation of listener (up direction)
    icReal      vol;        //! Listeners volume level
};


bool Load_Wave_File(const char *fname, icSoundBuffer* header);


/*! Provides an interface to a sound object
**/
class icSoundI
{
public:
    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT Play(void)=0;
    virtual ICRESULT Play3d(void)=0;
    virtual ICRESULT Stop(bool bSavePos=false)=0;
    virtual ICRESULT Pause(void)=0;

    virtual ICRESULT SetParams(const icSoundParam& params)=0;

    FORCE_INLINE const icSoundParam& GetParams(void) {return m_Params;};

protected:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    virtual ~icSoundI(void){};

    icSoundParam    m_Params;   //! Sound parameters
    icSoundBuffer   m_buf;

    bool            m_bPlaying;
    bool            m_bPaused;
    bool            m_b3d;

    friend class icSoundDeviceI;
};

/*! Provides an interface to a streaming sound
 *      must also implement icSoundI
**/
class icSoundStreamI : public icSoundI
{
public:
    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual bool IsStreaming(void)=0;

protected:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    virtual ~icSoundStreamI(void){};
};


/*! Provides an interface to a sound device
**/
class icSoundDeviceI
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icSoundDeviceI(void);
    virtual ~icSoundDeviceI(void){};

    virtual ICRESULT Cleanup(void)=0;

    virtual ICRESULT Initialize(void)=0;

    virtual ICRESULT LoadSource(const char* szFile)=0;
    virtual ICRESULT UnloadSource(const char* szFile)=0;

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT Update(const float fDeltaTime)=0;

    virtual ICRESULT GetSound(const char* szName, icSoundI** ppSound)=0;

    virtual ICRESULT Play(const char* szName,
                          const icSoundParam& soundParams,
                          icSoundI** ppSound)=0;

    virtual ICRESULT Play3D(const char* szName,
                            const icSoundParam& soundParams,
                            icSoundI** ppSound)=0;

    virtual ICRESULT PlayStream(const char* szFile,
                                const icSoundParam& soundParams,
                                icSoundStreamI** ppStream)=0;

    virtual ICRESULT PlayStream3D(const char* szFile,
                                  const icSoundParam& soundParams,
                                  icSoundStreamI** ppStream)=0;

    virtual ICRESULT ReleaseSound(icSoundI* pSound)=0;

    virtual ICRESULT ReleaseStream(icSoundStreamI* pStream)=0;

    //////////////////////////////////////////////////////////////////////////
    // ACCESS/INQUIRY

    virtual void SetListener(const icSoundListener& listener)
    {
        m_Listener = listener;
    };

protected:
    //////////////////////////////////////////////////////////////////////////
    // PROTECTED FUNCTIONS
    ICRESULT FindSource(const char* szFile, icSoundBuffer& buffer);
    ICRESULT CreateSource(const char* szFile, icSoundBuffer& buffer);
    //ICRESULT CreateSource(const char* szName, int bufsize,
    //                            ICSND_FMT format);

    icSoundI* GetFreeSound(void);
    ICRESULT FreeSound(icSoundI* pSound);
    
    icSoundStreamI* GetFreeStream(void);
    ICRESULT FreeStream(icSoundStreamI* pStream);

    //////////////////////////////////////////////////////////////////////////
    // PROTECTED MEMBERS
    icSoundListener m_Listener;                     //! The current listener

    // TODO: this isn't going to work with a heap
    // basically what will happen is icSoundBuffers will get moved
    // after icSoundI is already referencing it.
    // really dangerous, must refactor!
    icSoundBuffer   m_SrcBufs[ICSND_MAX_SOURCE];    //! Sound data buffers

    icSoundI*       m_Sounds[ICSND_MAX_PLAYING];    //! Free sounds
    icSoundI*       m_Playing[ICSND_MAX_PLAYING];   //! Playing sounds

    icSoundStreamI* m_Streams[ICSND_MAX_STREAMS];   //! Streams

    uchar           m_NumPlaying;                   //! Num playing sounds
    uchar           m_NumFreeSounds;                //! Num free sounds
    uchar           m_NumStreaming;                 //! Num playing streams
    uchar           m_NumFreeStreams;               //! Num free streams

    ushort          m_NumSrc;                       //! Num src buffs loaded
};

#endif //__IC_SOUND_H__
