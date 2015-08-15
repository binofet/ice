#ifndef __DOUG_LEA_MEMORY_POOL_H__
#define __DOUG_LEA_MEMORY_POOL_H__

#include "icAbstractMemoryPool.h"

#include "lea_malloc/leamalloc.h"

class dlMemoryPool : public icAbstractMemoryPool
{
public:
	dlMemoryPool(size_t maxCapacity)
	{
		Create(maxCapacity);
	}

	ICRESULT Create(size_t maxCapacity)
	{
		_memorySpace = create_mspace(maxCapacity, 0);
		if (_memorySpace != 0)
		{
			return IC_OK;
		}
		return IC_FAIL_OUT_OF_MEM;
	}

	ICRESULT Destroy()
	{
		if (_memorySpace != 0)
		{
			destroy_mspace(_memorySpace);
			_memorySpace = 0;
			return IC_OK;
		}
		return IC_FAIL_GEN;
	}

	void* malloc(size_t num_bytes)
	{
		size_t actual = 0;
		void* result = mspace_malloc(_memorySpace, num_bytes, &actual);
		OnAllocated(num_bytes, actual, result);
		return result;
	}

	void* calloc(size_t num_elememts, size_t element_size)
	{
		return mspace_calloc(_memorySpace, num_elememts, element_size);
	}

	void* memalign(size_t alignment, size_t num_bytes)
	{
		return mspace_memalign(_memorySpace, alignment, num_bytes);
	}

	void* realloc(void* ptr, size_t num_bytes)
	{
		return mspace_realloc(_memorySpace, ptr, num_bytes);
	}


	void free(void* ptr)
	{
		mspace_free(_memorySpace, ptr);
		OnFreed(ptr);
	}

private:
	mspace _memorySpace;
};

#endif // __DOUG_LEA_MEMORY_POOL_H__
