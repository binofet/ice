#ifndef __IC_SOUND_HEADER_H__
#define __IC_SOUND_HEADER_H__



struct _ICSND_WAV_HEADER
{
    int     chunk_id;
    uint    chunk_size;
    uint    format;
    uint    subchunk1_id;
    uint    subchunk1_size;
    ushort  audio_format;
    ushort  num_channels;
    uint    sample_rate;
    uint    byte_rate;
    ushort  block_align;
    ushort  bits_per_samp;
    uint    subchunk2_id;
    uint    subchunk2_size;
};

#endif //__IC_SOUND_HEADER_H__