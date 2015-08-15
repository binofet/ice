#include "Memory/icAbstractMemoryPool.h"
#include "Memory/AllocationDebug.h"

using namespace Ice::Core::Debug;

void icAbstractMemoryPool::OnAllocated(size_t requested, size_t actual, void* address)
{
	//AllocationDebug::AddAlloc(requested, actual, this, address);
}

void icAbstractMemoryPool::OnFreed(void* address)
{
	//AllocationDebug::AddFree(this, address);
}
