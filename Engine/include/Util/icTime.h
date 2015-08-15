#ifndef __IC_TIME_H__
#define __IC_TIME_H__

#include "Math/icCommonMath.h"

void icSysTimeInit(void);


/* Provides accurate timing functionality
**/
class icTimer
{
public:
    icTimer(void);

    icReal Elapsed(void);

private:
    uint64 m_last;
};


class icStopWatch
{
public:
    icStopWatch(void);

    void Start();
    void Lap();
    void End();
private:
    icTimer m_Timer;
};

#endif //__IC_TIME_H__
