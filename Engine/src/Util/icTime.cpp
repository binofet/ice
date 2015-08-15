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
icTimer::icTimer(void)
{
#ifdef WIN32
	QueryPerformanceCounter((LARGE_INTEGER*)&m_last);
#else
	timeval _tstart;
	gettimeofday(&_tstart,NULL);
	m_last = (uint64)_tstart.tv_sec << 32;
	m_last |= _tstart.tv_usec;
#endif
}// END FUNCTION icTimer(void)


/*! Returns the amount of time elapsed in seconds
*
*      First call returns time since this object was created,
*      All calls thereafter are based on the last time this function
*      was called
*
*  @returns            icReal          Elapsed time in seconds
**/
icReal icTimer::Elapsed(void)
{
#ifdef WIN32
	LARGE_INTEGER temp;
	temp.QuadPart = m_last;
	LARGE_INTEGER current;
	QueryPerformanceCounter(&current);
	m_last = current.QuadPart;
	return (icReal)((double)(current.QuadPart - temp.QuadPart)/(double)__ic_freq.QuadPart);
#else
	timeval* pLast = (timeval*)&m_last;
	timeval temp = *pLast;
	timeval _tcurrent;
	gettimeofday(&_tcurrent,NULL);
	*pLast = _tcurrent;
	icReal elapsed = _tcurrent.tv_sec - temp.tv_sec;
	elapsed += (double)(_tcurrent.tv_usec - temp.tv_usec)/1000000.0;
	return elapsed;
#endif
}// END FUNCTION Elapsed(void)


void icStopWatch::Start()
{
}

void icStopWatch::Lap()
{
}

void icStopWatch::End()
{
}


