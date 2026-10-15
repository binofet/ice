#pragma once

#include "ice/globals.h"
namespace ice {

#ifdef WIN32
} // namespace ice
#include "windows.h"
namespace ice {
#endif

#define IC_WAIT_INFINITE 0xFFFFFFFF

/*! Object provides mutual exclusion
 *
 **/
class mutex {
public:
	/////////////////////////////////////////////////////////////////////////
	// LIFECYCLE
	mutex(void);
	mutex(const char *szName);
	~mutex(void);

	/////////////////////////////////////////////////////////////////////////
	// OPERATIONS
	bool Aquire(uint wait);

	bool Release(void);

	/////////////////////////////////////////////////////////////////////////
	// ACCESS
	const char *GetName(void);

private:
	mutex(const mutex &ref);
	mutex &operator=(const mutex &ref);

	char  m_szName[64];
	void *m_pMutex;
};

class semaphore {
public:
	semaphore(void);
	~semaphore(void);

private:
};
} // namespace ice

#include "ice/mutex.inl"
namespace ice {} // namespace ice
