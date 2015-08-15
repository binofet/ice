#ifndef _IC_MEMORY_POOL_H__
#define _IC_MEMORY_POOL_H__

#include "Core/icGlobals.h"

class icMemoryPoolI
{
public:
   
	virtual ~icMemoryPoolI()
	{
		Destroy();
	}

	virtual ICRESULT Create(size_t maxCapacity) = 0;

	virtual ICRESULT Destroy() { return IC_NOT_IMPL; }

	virtual void* malloc(size_t num_bytes) = 0;

	virtual void* calloc(size_t num_elememts, size_t element_size) = 0;

	virtual void* memalign(size_t alignment, size_t num_bytes) = 0;

	virtual void* realloc(void* ptr, size_t num_bytes) = 0;

	virtual void free(void* ptr) = 0;

protected:

	virtual void OnAllocated(size_t requested, size_t actual, void* address) = 0;

	virtual void OnFreed(void* address) = 0;
};

#endif // _IC_MEMORY_POOL_H__
