#ifndef __IC_MUTEX_H__
#define __IC_MUTEX_H__


#include "Core/icGlobals.h"

#ifdef WIN32
#include "windows.h"
#endif

#define IC_WAIT_INFINITE 0xFFFFFFFF

/*! Object provides mutual exclusion
 *
**/
class icMutex
{
public:
    /////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icMutex(void);
    icMutex(const char* szName);
    ~icMutex(void);

    /////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    bool Aquire( uint wait );

    bool Release(void);

    /////////////////////////////////////////////////////////////////////////
    // ACCESS
    const char* GetName(void);

private:
    icMutex(const icMutex& ref);
    icMutex& operator=(const icMutex& ref);

    char    m_szName[64];
    void*    m_pMutex;
};


class icSemaphore
{
public:
    icSemaphore(void);
    ~icSemaphore(void);

private:
};

#include "icMutex.inl"

#endif // __IC_MUTEX_H__