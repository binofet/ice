
#include "Memory/icMemory.h"

#include "Memory/icSystemMemoryPool.h"

//dlMemoryPool icMemory::_defaultPool;
#ifdef __APPLE__
tlsfMemoryPool* icMemory::_stringPool;
tlsfMemoryPool* icMemory::_scriptPool;
tlsfMemoryPool* icMemory::_tlsfPool;
#else
dlMemoryPool* icMemory::_stringPool;
dlMemoryPool* icMemory::_scriptPool;
tlsfMemoryPool* icMemory::_tlsfPool;
#endif

ICRESULT icMemory::Init()
{
//	_defaultPool.Create(1024*1024*256);

#ifndef __APPLE__
	_stringPool = new dlMemoryPool(mB(2));

	_tlsfPool = new tlsfMemoryPool();
	_tlsfPool->Create(mB(2));

	_scriptPool = new dlMemoryPool(mB(2));
#else
    _stringPool = new tlsfMemoryPool();
    _stringPool->Create(mB(2));
    
	_tlsfPool = new tlsfMemoryPool();
	_tlsfPool->Create(mB(2));
    
	_scriptPool = new tlsfMemoryPool();
    _scriptPool->Create(mB(2));
#endif
	return IC_OK;
}

ICRESULT icMemory::Shutdown()
{
	//_defaultPool.Destroy();
	delete _stringPool;
	delete _tlsfPool;
	delete _scriptPool;
	return IC_OK;
}

icMemoryPoolI* icMemory::getDefaultPool()
{
#ifdef __APPLE__
    static icSystemMemoryPool defaultPool(mB(128));
#else
	static dlMemoryPool defaultPool(mB(128));
#endif
	return &defaultPool;
}

void* operator new (size_t num_bytes)
{
	return icMemory::getDefaultPool()->malloc(num_bytes);
}

void operator delete(void* p)
{
	icMemory::getDefaultPool()->free(p);
}

void* operator new [](size_t num_bytes)
{
	return icMemory::getDefaultPool()->malloc(num_bytes);
}

void operator delete [](void* p)
{
	icMemory::getDefaultPool()->free(p);
}

