#ifndef __IC_THREAD_H__
#define __IC_THREAD_H__

#include "Core/icGlobals.h"

#ifdef WIN32
#include <windows.h>
#elif defined(IC_HAS_POSIX)
#include <pthread.h>
#endif


enum icThreadFlags
{
	eTF_Kill = 0,
	eTF_WaitFinish,
};


/*! Provides the thread interface
*
*  Objects inheriting from this must implement StartThread(void)
**/
class icThread;
ICRESULT icThreadDestory(icThread* pThread, icThreadFlags flags = eTF_Kill);

class icThread
{
public:
	ICRESULT Pause(void);
	ICRESULT Resume(void);
	static void Sleep(uint millisec);

	volatile bool IsThreadActive(void);

	friend ICRESULT icThreadDestroy(icThread* pThread, icThreadFlags flags);

	static icThread* GetCurrent(void);

protected:
	icThread(void);
	virtual ~icThread(void);

	virtual FORCE_INLINE ICRESULT StartThread(void) {return IC_FAIL_GEN;};

	ICRESULT AquireFromSystem(void);

#ifdef WIN32
	icThread(DWORD id, HANDLE pseudoThread)
	{
		m_threadID = id;
		m_pThread = pseudoThread;
	}

	static DWORD WINAPI ThreadRun(LPVOID param);
	DWORD    m_threadID;
	volatile void*    m_pThread;
#else
    icThread(pthread_t id)
    {
        m_threadID = id;
    }
    
	static void* ThreadRun(void* param);
	pthread_t        m_threadID;
#endif
};


/*! VoidThread provides a wrapper for threads that
*  call a c-style function with a void param.
**/
class icVoidThread : public icThread
{
public:
	~icVoidThread(void){};

	friend ICRESULT icThreadCreate(ICRESULT (*pFunc)(void*),
		void* param,
		icThread** pt);

protected:
	FORCE_INLINE ICRESULT StartThread(void) {return m_pFunc(m_ptrData);};

private:
	icVoidThread(ICRESULT (*pFunc)(void*), void* param)
	{
		m_pFunc=pFunc;
		m_ptrData=param;
	};
	icVoidThread(void);
	icVoidThread(const icVoidThread& copy);

	ICRESULT (*     m_pFunc)(void*);
	void*           m_ptrData;
};

/*! TypeThread provides a delegate object to start
*  a thread on an instantiated objects member
*  function.
**/
template<class T>
class icTypeThread : public icThread
{
public:
	virtual ~icTypeThread(void){};

	template<class E>
	friend ICRESULT icThreadCreate(E* pObj, ICRESULT (E::*pFunc)(void*),
		void* param,
		icThread** pt);

protected:
	FORCE_INLINE ICRESULT StartThread(void){return (m_pObj->*m_pFunc)(m_pData);};

private:
	icTypeThread(T* pObj, ICRESULT (T::*pFunc)(void*), void* pData)
	{
		m_pObj = pObj;
		m_pFunc = pFunc;
		m_pData = pData;
	}

	icTypeThread(void);
	icTypeThread(const icTypeThread& copy);

	T*              m_pObj;
	ICRESULT (T::*  m_pFunc)(void*);
	void*           m_pData;
};

template<class T>
ICRESULT icThreadCreate(T* pObj, ICRESULT (T::*pFunc)(void*),
	void* param,
	icThread** pt)
{
	icTypeThread<T>* thread = new icTypeThread<T>(pObj,pFunc,param);
	if (thread)
	{
		thread->AquireFromSystem();
		*pt = thread;
		return IC_OK;
	}
	return IC_FAIL_GEN;
}
#endif // __IC_THREAD_H__