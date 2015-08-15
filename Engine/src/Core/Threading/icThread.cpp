
#include "Core/Threading/icThread.h"
#include "Core/Threading/icMutex.h"

#define IC_THREADING_MAX_THREADS 12
static icThread* threads[IC_THREADING_MAX_THREADS] = {0};
static u32 numThreads = 0;
static icMutex mutex;

/*! Default ThreadRun function
*
*      Static function to kick off icThread::StartThread
**/
#ifdef WIN32
DWORD WINAPI icThread::ThreadRun(LPVOID param)
#else
void* icThread::ThreadRun(void* param)
#endif
{
	ICRESULT res = IC_FAIL_GEN;
	if (param)
	{
		icThread* pThread = (icThread*)param;

		res = pThread->StartThread();

#ifdef WIN32
		CloseHandle((HANDLE)pThread->m_pThread);
		pThread->m_pThread = NULL;
		pThread->m_threadID = 0;
		icPrintf("icThread (%i) exited with ICRESULT (%i)",
			pThread->m_threadID, res);
	}
	return res;        
#elif defined(IC_HAS_POSIX)
		pthread_exit(NULL);
		pThread->m_threadID = (pthread_t)-1;
		icPrintf("icThread (%i) exited with ICRESULT (%i)",
			pThread->m_threadID, res);
	}
	return (void*)res;        
#endif
}// END FUNCTION ThreadRun(void*)


/*! Default c'tor
**/
icThread::icThread(void)
{    
#ifdef WIN32
	m_threadID=0;
	m_pThread=NULL;
#elif defined(IC_HAS_POSIX)
	m_threadID = (pthread_t)-1;
#endif
}// END FUNCTION icThread(void)


/*! Default d'tor
**/
icThread::~icThread(void)
{
	for (u32 i = 0; i < numThreads; ++i)
	{
		//mutex.lock();
		//{
		//	if (InterlockedCompareExchange((icIntPtr)threads[i] 
		//}
		//mutex.unlock();
	}
}// END FUNCTION ~icThread(void)


/*! Pauses the thread
*
*    @returns    ICRESULT    Result of pausing the thread.
**/
ICRESULT icThread::Pause(void)
{
#ifdef WIN32
	if (m_pThread)
	{
		if (SuspendThread((HANDLE)m_pThread)!=1)
			return IC_OK;
	}
#endif

	return IC_FAIL_GEN;
}// END FUNCTION Pause(void)


/*! Resumes a paused thread
*
*    @returns    ICRESULT    Result of resuming the thread.
**/
ICRESULT icThread::Resume(void)
{
#ifdef WIN32
	if (m_pThread)
	{
		if (ResumeThread((HANDLE)m_pThread)!=-1)
			return IC_OK;
	}
#endif

	return IC_FAIL_GEN;
}// END FUNCTION Resume(void)


/*! Sleeps the current thread
*
*    @param    millisec    Time in milliseconds to sleep.
**/
void icThread::Sleep(uint millisec)
{
#ifdef WIN32
	::Sleep(millisec);
#elif (defined __APPLE__)
    pthread_yield_np();
#else
	timespec tv;
	tv.tv_sec = millisec/1000;
	tv.tv_nsec = (millisec-(tv.tv_sec*1000))*1000000L;
	while(nanosleep(&tv,&tv)==-1)
		continue;
#endif
}// END FUNCTION Sleep(void)


/*! TEMPORARY
*    TODO: delete this hack
**/
volatile bool icThread::IsThreadActive(void)
{
#ifdef WIN32
	return m_pThread != NULL;
#elif defined(IC_HAS_POSIX)
	return (int)(size_t)m_threadID != -1;
#endif
}


/*! Provides an interface to aquire the operating system threads
*
*    @returns    ICRESULT    Status of getting a thread from the OS
**/
ICRESULT icThread::AquireFromSystem(void)
{
	ICRESULT res=IC_OK;
#ifdef WIN32
	//SECURITY_ATTRIBUTES threadAttributes;
	DWORD createFlags=0;

	m_pThread = CreateThread(NULL,            //SECURITY_ATTRIBUTES
		0,                //STACK SIZE
		&ThreadRun,    //FUNCTION ADDRESS
		this,            //LPVOID
		createFlags,    //CREATION FLAGS,
		&m_threadID    //GET THREAD ID
		);

	if (!m_pThread)
		res = IC_FAIL_GEN;
#elif defined(IC_HAS_POSIX)
	pthread_attr_t    attr;
	if (pthread_attr_init(&attr))
		return IC_FAIL_GEN;
	if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED))
		return IC_FAIL_GEN;

	int threadError = pthread_create(&m_threadID,
		&attr,
		&ThreadRun,
		this);

	if (pthread_attr_destroy(&attr))
	{
		//release the thread and report error
		return IC_FAIL_GEN;
	}

	if (threadError != 0)
	{
		return IC_FAIL_GEN;
	}

#endif

	return res;
}// END FUNCTION AquireFromSystem(void)

icThread* icThread::GetCurrent(void)
{
#ifdef WIN32
	DWORD threadID = GetCurrentThreadId();
#elif (defined __APPLE__)
    pthread_t threadID = pthread_self();
#endif
	icThread* thread = 0;
	for (u32 i = 0; i < numThreads; ++i)
	{
#ifdef WIN32
		if (threads[i]->m_threadID == threadID)
		{
			thread = threads[i];
			break;
		}
#else
        pthread_self();
#endif
	}

	if (thread == 0)
	{
#ifdef WIN32
		thread = new icThread(threadID, GetCurrentThread());
#elif (defined __APPLE__)
        thread = new icThread(threadID);
#endif
		threads[numThreads] = thread;
#ifdef WIN32
		InterlockedIncrement((LPLONG)&numThreads);
#endif
	}

	return thread;
}


/*! Creates a thread
*
*
*
**/
ICRESULT icThreadCreate(ICRESULT (*pFunc)(void*),
	void* param,
	icThread** pt)
{
	icVoidThread* thread = new icVoidThread(pFunc,param);
	if (thread)
	{
		thread->AquireFromSystem();
		*pt = thread;
		return IC_OK;
	}
	return IC_FAIL_GEN;
}// END FUNCTION icThreadCreate(ICRESULT (*pFunc)(void*), void* param,
//                                icThread** pt)


/*! Destroys a thread
*
*
*
**/
ICRESULT icThreadDestroy(icThread* pThread, icThreadFlags flags)
{
	if (pThread)
	{
		while (pThread->IsThreadActive())
		{
			icThread::Sleep(0);
		}
		delete pThread;
		return IC_OK;
	}
	return IC_FAIL_GEN;
}// END FUNCTION icThreadDestroy(icThread* pThread, uint FLAGS)