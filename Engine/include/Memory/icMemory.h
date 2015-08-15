#ifndef __IC_MEMORY_H__
#define __IC_MEMORY_H__

#include "Core/icGlobals.h"

#include "dlMemoryPool.h"
#include "tlsfMemoryPool.h"

#define kB(x) (1024*x)
#define mB(x) (1024*1024*x)

class icMemory
{
public:
	static ICRESULT Init();
	static ICRESULT Shutdown();

	static icMemoryPoolI* getDefaultPool();

	static icMemoryPoolI* getScriptPool() { return _scriptPool; }
	static icMemoryPoolI* getStringPool() { return _stringPool; }
	static icMemoryPoolI* getTlsfPool() { return _tlsfPool; }

private:
	static tlsfMemoryPool* _tlsfPool;
#ifdef __APPLE__
    static tlsfMemoryPool* _stringPool;
	static tlsfMemoryPool* _scriptPool;
#else
	static dlMemoryPool* _stringPool;
	static dlMemoryPool* _scriptPool;
#endif
};


#endif //__IC_MEMORY_H__

