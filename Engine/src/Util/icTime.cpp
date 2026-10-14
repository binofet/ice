#include <chrono>
#include "Util/icTime.h"

#ifdef WIN32
#ifdef WIN32
#include <windows.h>
#else
#include <Xtl.h>
#endif
LARGE_INTEGER __ic_freq;
void icSysTimeInit(void)
{
	QueryPerformanceFrequency(&__ic_freq);
}
#else
#include <sys/time.h>
void icSysTimeInit(void)
{
}
#endif

/*! c'tor
**/
static uint64 icNowNanoseconds()
{
    return (uint64)std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
icTimer::icTimer() : m_last(icNowNanoseconds()) {}
icReal icTimer::Elapsed()
{
    uint64 now = icNowNanoseconds();
    icReal elapsed = (icReal)((now - m_last) / 1000000000.0);
    m_last = now;
    return elapsed;
}

void icStopWatch::Start()
{
}

void icStopWatch::Lap()
{
}

void icStopWatch::End()
{
}


