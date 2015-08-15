#ifndef __IC_ABSTRACT_MEMORY_POOL_H__
#define __IC_ABSTRACT_MEMORY_POOL_H__

#include "Memory/icMemoryPool.h"

class icAbstractMemoryPool : public icMemoryPoolI
{
protected:

	virtual void OnAllocated(size_t requested, size_t actual, void* address);

	virtual void OnFreed(void* address);
};

#endif // __IC_ABSTRACT_MEMORY_POOL_H__
