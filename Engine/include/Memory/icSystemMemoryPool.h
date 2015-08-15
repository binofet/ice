#ifndef __IC_SYSTEM_MEMORY_POOL_H__
#define __IC_SYSTEM_MEMORY_POOL_H__

#include "icAbstractMemoryPool.h"


class icSystemMemoryPool : public icAbstractMemoryPool
{
public:
    icSystemMemoryPool(size_t maxCapacity) {};
    
	ICRESULT Create(size_t maxCapacity)
	{
		return IC_OK;
	}

	ICRESULT Destroy()
	{
		return IC_OK;
	}

	void* malloc(size_t num_bytes)
	{
		return malloc(num_bytes);
	}

	void* calloc(size_t num_elememts, size_t element_size)
	{
		return calloc(num_elememts, element_size);
	}

	void* memalign(size_t alignment, size_t num_bytes)
	{
		return memalign(alignment, num_bytes);
	}

	void* realloc(void* ptr, size_t num_bytes)
	{
		return realloc(ptr, num_bytes);
	}

	void free(void* ptr)
	{
		free(ptr);
	}

private:
	size_t _maxSize;
};


#endif // __IC_SYSTEM_MEMORY_POOL_H__
