#ifndef __TLSF_MEMORY_POOL_H__
#define __TLSF_MEMORY_POOL_H__

#include "icAbstractMemoryPool.h"

#include "TLSF/tlsf.h"

#ifdef __APPLE__
#include <memory.h>
#endif

class tlsfMemoryPool : public icAbstractMemoryPool
{
public:

	ICRESULT Create(size_t maxCapacity)
	{
		_raw = ::malloc(maxCapacity + 1024 * 3);
		_memory = tlsf_create(_raw, maxCapacity);
		if (_memory != 0)
		{
			return IC_OK;
		}
		return IC_FAIL_OUT_OF_MEM;
	}

	ICRESULT Destroy()
	{
		if (_memory != 0)
		{
			tlsf_destroy(_memory);
			_memory = 0;
		}
		if (_raw != 0)
		{
			free(_raw);
			_raw = 0;
		}

		return IC_OK;
	}

	void* malloc(size_t num_bytes)
	{
		size_t actual = 0;
		void* result = tlsf_malloc(_memory, num_bytes, &actual);
		OnAllocated(num_bytes, actual, result);
		return result;
	}

	void* calloc(size_t num_elememts, size_t element_size)
	{
		return this->malloc(num_elememts * element_size);
	}

	void* memalign(size_t alignment, size_t num_bytes)
	{
		return tlsf_memalign(_memory, alignment, num_bytes);
	}

	void* realloc(void* ptr, size_t num_bytes)
	{
		return tlsf_realloc(_memory, ptr, num_bytes);
	}

	void free(void* ptr)
	{
		tlsf_free(_memory, ptr);
		OnFreed(ptr);
	}

private:
	tlsf_pool _memory;
	void* _raw;
};


#endif // __TLSF_MEMORY_POOL_H__
