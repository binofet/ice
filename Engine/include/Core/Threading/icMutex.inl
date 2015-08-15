
#include "icMutex.h"

/*! Creates a mutex with a default name
**/
FORCE_INLINE icMutex::icMutex(void)
{
    static unsigned int numThreads = 0;

#ifdef WIN32
#ifdef _MSC_VER
    sprintf_s(m_szName, 64, "icMutex_unnamed%u", numThreads++);
#else
    sprintf(m_szName, "icMutex_unnamed%u", numThreads++);
#endif
    //SECURITY_ATTRIBUTES mutexAttrib;
    m_pMutex = CreateMutexA(/*&mutexAttrib*/NULL, FALSE, m_szName);
#endif
}// END FUNCTION icMutex(void)


/*! Creates a mutex with a user specified name
**/
FORCE_INLINE icMutex::icMutex(const char* szName)
{
#if defined(_MSC_VER) && (_MSC_VER > 1400)
    strcpy_s(m_szName, 64, szName);
#else
    strcpy(m_szName, szName);
#endif
#ifdef WIN32
    //SECURITY_ATTRIBUTES mutexAttrib;
    m_pMutex = CreateMutexA(/*&mutexAttrib*/NULL, FALSE, m_szName);
#else

#endif
}// END FUNCTION icMutex(const char* szName)


/*! Destroys the mutex
**/
FORCE_INLINE icMutex::~icMutex(void)
{
    if (m_pMutex)
    {
#ifdef WIN32
        if (!CloseHandle(m_pMutex))
            icWarning("ReleaseMutex failed");
#endif
    }
}// END FUNCTION ~icMutex(void)


/*! Acquires a lock on this mutex
 *
 *    @param[in]    wait    Time in ms to wait for lock.
 *    @returns    bool    true on successful lock.
**/
FORCE_INLINE bool icMutex::Aquire(uint wait)
{
#ifdef WIN32
    DWORD res = WaitForSingleObject(m_pMutex, wait);
    if (res == WAIT_ABANDONED)
        return false;

    return true;
#endif
    return false;
}// END FUNCTION Acquire(uint wait)


/*! Releases the lock on this mutex
 *
 *    @returns    bool    true on success.
**/
FORCE_INLINE bool icMutex::Release(void)
{
#ifdef WIN32
    if (!ReleaseMutex(m_pMutex))
    {
        icWarning("Mutex failed to release");
        return false;
    }
#endif
    return true;
}// END FUNCTION Release(void)
