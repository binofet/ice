#pragma once

#include "Memory/icMemoryPool.h"
#include "Memory/icString.h"


namespace Ice { namespace Core { namespace Debug {

#if defined(WIN32) || defined(XBOX)
	typedef DWORD ThreadID;
#elif defined(POSIX)
    typedef unsigned int ThreadID;
#elif defined(__APPLE__)
#include <pthread.h>
	typedef pthread_t ThreadID;
#endif

	class StackTrace
	{
	public:
		StackTrace(unsigned int hash, size_t size, void** stackTrace)
		{
			_hash = hash;
			_size = size;
			_stackTrace = stackTrace;
		}

		bool operator < (const StackTrace& rhs)
		{
			return _hash < rhs._hash;
		}

		bool operator > (const StackTrace& rhs)
		{
			return _hash > rhs._hash;
		}

		bool operator == (const StackTrace& rhs)
		{
			if (_hash == rhs._hash)
			{
				if (_size == rhs._size)
				{
					// TODO: order has performance impact, investigate.
					for (int i=0; i<(int)_size; ++i)
					{
						if (_stackTrace[i] != rhs._stackTrace[i])
							return false;
					}
					return true;
				}
				return false;
			}
			return false;
		}

	//private:
		unsigned int _hash;
		size_t _size;
		void** _stackTrace;
	};

	class AllocationRecord
	{
	public:
		AllocationRecord(
			size_t requested,
			size_t actual,
			void* address,
			icMemoryPoolI* pool,
			StackTrace* trace)
		{
			_requested = requested;
			_actual = actual;
			_address = address;
			_pool = pool;
			_stackTrace = trace;
#ifdef __APPLE__
            _threadID = pthread_self();
#elif (defined (WIN32))
			_threadID = GetCurrentThreadId();
#endif
		}

	//private:
		size_t _requested;
		size_t _actual;
		void* _address;
		icMemoryPoolI* _pool;
		StackTrace* _stackTrace;
		ThreadID _threadID;
	};

	class FreeRecord
	{
	public:
		FreeRecord(
			void* address,
			icMemoryPoolI* pool,
			StackTrace* trace)
		{
			_address = address;
			_pool = pool;
			_stackTrace = trace;
#ifdef __APPLE__
            _threadID = pthread_self();
#else
			_threadID = GetCurrentThreadId();
#endif
		}

	//private:
		void* _address;
		icMemoryPoolI* _pool;
		StackTrace* _stackTrace;
		ThreadID _threadID;
	};

	class AllocationDebug
	{
	public:
		static void SaveToFile(const char* szFileName);

		static void AddAlloc(
			size_t requested,
			size_t actual,
			icMemoryPoolI* pool,
			void* address);

		static void AddFree(
			icMemoryPoolI* pool,
			void* address);

	private:
		static void PreAdd();
		static int FindStackTrace(const StackTrace& other);
#ifdef WIN32
		static BOOL CALLBACK EnumerateLoadedModulesProc64(
			PCTSTR moduleName,
			DWORD64 moduleBase,
			ULONG moduleSize,
			PVOID UserContext);

		static CRITICAL_SECTION _critical;
#endif

		static icMemoryPoolI* _memPool;
		static size_t _allocCount;
		static size_t _freeCount;
		static AllocationRecord* _allocs;
		static FreeRecord* _frees;

		static size_t _traceCount;
		static StackTrace* _stackTraces;

		static size_t _traceCap;
		static size_t _allocCap;
		static size_t _freeCap;
	};


} } } // Ice.Core.Debug
