#include "icRand.h"

extern int __ic_rand_seed;

#ifdef WIN32
#include "windows.h"
#endif

FORCE_INLINE int icRandAutoSeed(void)
{
#ifdef WIN32
    LARGE_INTEGER tim, freq;
    double seconds;

    QueryPerformanceCounter(&tim);
    QueryPerformanceFrequency(&freq);
    seconds = (double)tim.QuadPart / (double) freq.QuadPart;
    __ic_rand_seed = (int)((seconds - floor(seconds))* 10000000.0);
#else
    __ic_rand_seed = 0xb16b00b5;
#endif
    return __ic_rand_seed;
}

FORCE_INLINE void icRandSeed(int seed)
{
    __ic_rand_seed = seed;
}

FORCE_INLINE int icRandI(void)
{
    __ic_rand_seed = (__ic_rand_seed * 69069 + 1);
    return __ic_rand_seed;
}

FORCE_INLINE int icRandI(int min, int max)
{
    assert(max > min);
    if (min >= 0)
        return (int)(icRandUI() % ((uint)max - (uint)min)) + min;
    else
        return icRandI() % (max - min) + min;
}

FORCE_INLINE uint icRandUI(void)
{
    __ic_rand_seed = (__ic_rand_seed * 69069 + 1);
    return (uint)__ic_rand_seed;
}

FORCE_INLINE uint icRandUI(uint min, uint max)
{
    return icRandUI() % (max - min) + min;
}

FORCE_INLINE float icRandF()
{
    return (float)(icRandI() & 0xFFFF) / (float)0x10000;
}

FORCE_INLINE float icRandF(float min, float max)
{
    return ((max - min) * icRandF()) + min;
}

FORCE_INLINE double icRandD()
{
    return (double)(icRandI() & 0xFFFF) / (double)0x10000;
}

FORCE_INLINE double icRandD(double min, double max)
{
    return ((max - min) * icRandD()) + min;
}

FORCE_INLINE icReal icRandR()
{
    return (icReal)(icRandI() & 0xFFFF) / (icReal)0x10000;
}

FORCE_INLINE icReal icRandR(icReal min, icReal max)
{
    return ((max - min) * icRandR()) + min;
}