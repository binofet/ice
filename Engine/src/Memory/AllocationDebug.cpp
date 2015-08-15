#include <new>

#include <string>
#include <map>
#include <vector>

#include "Math/icCommonMath.h"

#include "Memory/AllocationDebug.h"
#include "Memory/dlMemoryPool.h"
#include "Memory/icMinHeap.h"

#ifdef WIN32
#include "Dbghelp.h"
#endif

namespace Ice { namespace Core { namespace Debug {

#define PREADD_INTERVAL 2048

    

	icMemoryPoolI* AllocationDebug::_memPool = 0;
	size_t AllocationDebug::_allocCount = 0;
	size_t AllocationDebug::_freeCount = 0;
	AllocationRecord* AllocationDebug::_allocs = 0;
	FreeRecord* AllocationDebug::_frees = 0;

	size_t AllocationDebug::_traceCount = 0;
	StackTrace* AllocationDebug::_stackTraces = 0;

	size_t AllocationDebug::_traceCap = 0;
	size_t AllocationDebug::_allocCap = 0;
	size_t AllocationDebug::_freeCap = 0;

#ifdef WIN32
    
    CRITICAL_SECTION AllocationDebug::_critical;
    
	struct ModuleInfos
	{
		std::string moduleName;
		DWORD64 moduleBase;
		ULONG moduleSize;
	};

	BOOL AllocationDebug::EnumerateLoadedModulesProc64(
			PCTSTR moduleName,
			DWORD64 moduleBase,
			ULONG moduleSize,
			PVOID UserContext)
	{
		ModuleInfos info = { moduleName, moduleBase, moduleSize };
		std::vector<ModuleInfos>* infos = static_cast<std::vector<ModuleInfos>* >(UserContext);
		infos->push_back(info);
		return TRUE;
	}

	void AllocationDebug::SaveToFile(const char* szFileName)
	{
		char temp[256 + 128];
		SYMBOL_INFO* symbol =
			(SYMBOL_INFO *)_memPool->malloc(
				sizeof( SYMBOL_INFO ) + 256 * sizeof( char ));
		symbol->MaxNameLen   = 255;
		symbol->SizeOfStruct = sizeof( SYMBOL_INFO );

		//std::map<void*, std::string> moduleMap;

		std::vector<ModuleInfos> infos;

		HANDLE process = GetCurrentProcess();

		char searchpath[512];
		BOOL gotpath = SymGetSearchPath(process, searchpath, 512);

		strcpy(searchpath, ".;cache*C:\\symbols");

		BOOL setpath = SymSetSearchPath(process, searchpath);

		BOOL modules = EnumerateLoadedModules64(process, EnumerateLoadedModulesProc64, &infos);

		FILE* fp = fopen(szFileName, "wt");
		if (!fp) return;

		sprintf(temp,
"-----------------------------------------------------------------------\n  Allocations: total - %i, cumulative size - TODO\n-----------------------------------------------------------------------\n",
_allocCount);
		fwrite(temp, strlen(temp), 1, fp);

		for(int i = 0; i < _allocCount; ++i)
		{
			AllocationRecord* allocRecord = &_allocs[i];
			StackTrace* trace = allocRecord->_stackTrace;
			if (!trace)
			{
				sprintf(temp, "\nMISSING TRACE\n");
				fwrite(temp, strlen(temp), 1, fp);
				continue;
			}

			sprintf(
				temp,
				"\nRequest: %i\nActual: %i\nAddress: %#p\nThreadID: %u\nPool: %#p\n",
				allocRecord->_requested,
				allocRecord->_actual,
				allocRecord->_address,
				allocRecord->_threadID,
				allocRecord->_pool);

			fwrite(temp, 1, strlen(temp), fp);
			for (int j = 0; j < trace->_size && j < 256; ++j)
			{
				DWORD displacement = 0;
				IMAGEHLP_LINE line = {};
				void* stackPointer = trace->_stackTrace[j];
				if (SymFromAddr(process, (DWORD64)stackPointer, 0, symbol))
				{
					IMAGEHLP_MODULE64 moduleInfo = {};
					BOOL worked = SymGetModuleInfo64(process, symbol->ModBase, &moduleInfo);

					std::vector<ModuleInfos>::iterator curInfo = infos.begin();
					std::vector<ModuleInfos>::iterator end = infos.end();
					for(;curInfo != end; ++curInfo)
					{
						ModuleInfos& info = (*curInfo);
						if (info.moduleBase == symbol->ModBase ||
							info.moduleBase < symbol->Address && symbol->Address < info.moduleBase + info.moduleSize)
						{
							int len = info.moduleName.length() - 1;
							int modlen = 0;
							for (;;)
							{
								char c = info.moduleName[len - modlen++];
								if (c == '\\' || c == '/')
								{
									break;
								}
							}

							for(int c = 0; c < modlen - 1; ++c)
								moduleInfo.ModuleName[c] = info.moduleName[len - modlen + c + 2];
							
							moduleInfo.ModuleName[modlen] = '\n';
							break;
						}
					}

					if (SymGetLineFromAddr(process, (DWORD)stackPointer, &displacement, &line))
					{
						sprintf(
							temp,
							"\t%s!%s\t0x%p %s:%i\n",
							moduleInfo.ModuleName,
							symbol->Name,
							(int)symbol->Address,
							line.FileName,
							line.LineNumber);
					}
					else
					{
						sprintf(
							temp,
							"\t%s!%s\t%#p\n",
							moduleInfo.ModuleName,
							symbol->Name,
							symbol->Address);
					}
					fwrite(temp, 1, strlen(temp), fp);
				}
				else
				{
					//sprintf(temp, "\t%#p\n", _allocs[i]
				}
			}
		}

		sprintf(temp,
"\n\n\n\n\n-----------------------------------------------------------------------\n  Frees: total - %i\n-----------------------------------------------------------------------\n",
_freeCount);
		fwrite(temp, strlen(temp), 1, fp);

		for(int i = 0; i < _freeCount; ++i)
		{
			
			sprintf(
				temp,
				"\nAddress: %#p\nThreadID: %u\nPool: %#p\n",
				_frees[i]._address,
				_frees[i]._threadID,
				_frees[i]._pool);
			fwrite(temp, 1, strlen(temp), fp);
			StackTrace*  trace= _allocs[i]._stackTrace;
			for (int j = 0; j < trace->_size && j < 256; ++j)
			{
				if (SymFromAddr( process, (DWORD64)( _allocs[i]._stackTrace->_stackTrace[j] ), 0, symbol ))
				{
					sprintf(temp, "\t%s:\t%#p\n", symbol->Name, symbol->Address);
					fwrite(temp, 1, strlen(temp), fp);
				}
			}
		}
		fclose(fp);
	}

	int AllocationDebug::FindStackTrace(const StackTrace& other)
	{
		int min = 0;
		int max = _traceCount - 1;
		while (max >= min)
		{
			int index = (max + min) / 2;

			if (index >= _traceCount)
			{
				int balls = 2;
			}

			if (_stackTraces[index] < other)
			{
				min = index + 1;
			}
			else if (_stackTraces[index] > other)
			{
				max = index - 1;
			}
			else if (_stackTraces[index] == other)
				return index;
		}

		return -1;
	}

	void AllocationDebug::AddAlloc(
			size_t requested,
			size_t actual,
			icMemoryPoolI* pool,
			void* address)
	{
		if (pool == _memPool)
			return;

		if (_memPool == 0)
		{
			InitializeCriticalSection(&_critical);
		}

		EnterCriticalSection(&_critical);

		void* tempStack[256];
		DWORD hash;
		WORD count;
		if ((count = CaptureStackBackTrace(2, 255, tempStack, &hash)) > 0)
		{
			PreAdd();

			StackTrace temp(hash, count, tempStack);
			int index = icMinHeapFind(_stackTraces, _traceCount, temp);
			StackTrace* trace;
			if (index >= 0)
			{
				trace = &_stackTraces[index];
			}
			else
			{
				void** newStack = (void**)_memPool->malloc(sizeof(void*) * count);
				memcpy(newStack, tempStack, sizeof(void*) * count);
				trace = new (&_stackTraces[_traceCount++]) StackTrace(
						hash,
						count,
						newStack);
			}

			AllocationRecord* allocationRecord = 
				new (&_allocs[_allocCount++]) AllocationRecord(
					requested,
					actual,
					address,
					pool,
					trace);

		}
		LeaveCriticalSection(&_critical);
	}

	void AllocationDebug::AddFree(
			icMemoryPoolI* pool,
			void* address)
	{
		if (pool == _memPool || _memPool == (icMemoryPoolI*)0x5747) return;

		EnterCriticalSection(&_critical);

		void* tempStack[256];
		DWORD hash;
		WORD count;
		if ((count = CaptureStackBackTrace(2, 255, tempStack, &hash)) > 0)
		{
			PreAdd();

			StackTrace temp(hash, count, tempStack);
			int index = icMinHeapFind(_stackTraces, _traceCount, temp);
			StackTrace* trace;
			if (index >= 0)
			{
				trace = &_stackTraces[index];
			}
			else
			{
				void** newStack = (void**)_memPool->malloc(sizeof(void*) * count);
				memcpy(newStack, tempStack, sizeof(void*) * count);
				trace = new (&_stackTraces[_traceCount++]) StackTrace(
						hash,
						count,
						newStack);
			}

			FreeRecord* freeRecord = 
				new (&_frees[_freeCount++]) FreeRecord(
					address,
					pool,
					trace);

		}

		LeaveCriticalSection(&_critical);
	}

	void AllocationDebug::PreAdd()
	{
		if (_memPool == 0)
		{
			static char _memory[sizeof(dlMemoryPool)];
			_memPool = new(&_memory) dlMemoryPool(1024 * 1024 * 256);

			SymInitialize( GetCurrentProcess(), NULL, TRUE );
		}

		if (_allocCount >= _allocCap)
		{
			size_t multiplier = icMax<size_t>(_allocCount % PREADD_INTERVAL, 1);
			_allocCap = _allocCount + PREADD_INTERVAL * multiplier * 2;
			_allocs =
				(AllocationRecord*)_memPool->realloc(
					_allocs,
					sizeof(AllocationRecord) * _allocCap);
		}

		if (_freeCount >= _freeCap)
		{
			size_t multiplier = icMax<size_t>(_freeCount % PREADD_INTERVAL, 1);
			_freeCap = _freeCount + PREADD_INTERVAL * multiplier * 2;
			_frees =
				(FreeRecord*)_memPool->realloc(
					_frees,
					sizeof(FreeRecord) * _freeCap);
		}

		if (_traceCount >= _traceCap)
		{
			size_t multiplier = icMax<size_t>(_traceCount % PREADD_INTERVAL, 1);
			_traceCap = _traceCount + PREADD_INTERVAL * multiplier * 2;
			_stackTraces =
				(StackTrace*)_memPool->realloc(
					_stackTraces,
					sizeof(StackTrace) * _traceCap);
		}
	}
    
    
    
#else
    void AllocationDebug::AddAlloc(size_t requested, size_t actual, icMemoryPoolI *pool, void *address)
    {
    }
    
    void AllocationDebug::AddFree(icMemoryPoolI *pool, void *address)
    {
    }
    
    void AllocationDebug::SaveToFile(const char *szFileName)
    {
    }
    
#endif

} } } // Ice.Core.Debug
