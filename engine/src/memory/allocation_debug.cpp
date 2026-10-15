#include <new>

#include <map>
#include <string>
#include <vector>

#include "ice/math/common_math.h"

#include "ice/memory/allocation_debug.h"
#include "ice/memory/dl_memory_pool.h"
#include "ice/memory/min_heap.h"

#ifdef WIN32
#include <dbghelp.h>
#endif

namespace ice::memory::debug {

#define PREADD_INTERVAL 2048

ice::memory::memory_pool_i *allocation_debug::mem_pool_    = 0;
size_t                      allocation_debug::alloc_count_ = 0;
size_t                      allocation_debug::free_count_  = 0;
allocation_record          *allocation_debug::allocs_      = 0;
free_record                *allocation_debug::frees_       = 0;

size_t       allocation_debug::trace_count_  = 0;
stack_trace *allocation_debug::stack_traces_ = 0;

size_t allocation_debug::trace_cap_ = 0;
size_t allocation_debug::alloc_cap_ = 0;
size_t allocation_debug::free_cap_  = 0;

#ifdef WIN32

CRITICAL_SECTION allocation_debug::critical_;

struct module_info {
	std::string module_name;
	DWORD64     module_base;
	ULONG       module_size;
};

BOOL allocation_debug::enumerate_loaded_modules(PCTSTR module_name, DWORD64 module_base,
                                                ULONG module_size, PVOID user_context) {
	module_info               info  = {module_name, module_base, module_size};
	std::vector<module_info> *infos = static_cast<std::vector<module_info> *>(user_context);
	infos->push_back(info);
	return TRUE;
}

void allocation_debug::save_to_file(const char *file_name) {
	char         temp[256 + 128];
	SYMBOL_INFO *symbol =
	    (SYMBOL_INFO *)mem_pool_->malloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char));
	symbol->MaxNameLen   = 255;
	symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

	// std::map<void*, std::string> moduleMap;

	std::vector<module_info> infos;

	HANDLE process = GetCurrentProcess();

	char searchpath[512];
	BOOL gotpath = SymGetSearchPath(process, searchpath, 512);

	strcpy(searchpath, ".;cache*C:\\symbols");

	BOOL setpath = SymSetSearchPath(process, searchpath);

	BOOL modules = EnumerateLoadedModules64(process, enumerate_loaded_modules, &infos);

	FILE *fp = fopen(file_name, "wt");
	if (!fp)
		return;

	sprintf(temp,
	        "-----------------------------------------------------------------------\n  "
	        "Allocations: total - %i, cumulative size - "
	        "TODO\n-----------------------------------------------------------------------\n",
	        alloc_count_);
	fwrite(temp, strlen(temp), 1, fp);

	for (int i = 0; i < alloc_count_; ++i) {
		allocation_record *record = &allocs_[i];
		stack_trace       *trace  = record->trace;
		if (!trace) {
			sprintf(temp, "\nMISSING TRACE\n");
			fwrite(temp, strlen(temp), 1, fp);
			continue;
		}

		sprintf(temp, "\nRequest: %i\nActual: %i\nAddress: %#p\nthread_id: %u\nPool: %#p\n",
		        record->requested, record->actual, record->address, record->thread, record->pool);

		fwrite(temp, 1, strlen(temp), fp);
		for (int j = 0; j < trace->size && j < 256; ++j) {
			DWORD           displacement = 0;
			IMAGEHLP_LINE64 line         = {};
			line.SizeOfStruct            = sizeof(line);
			void *stackPointer           = trace->frames[j];
			if (SymFromAddr(process, (DWORD64)stackPointer, 0, symbol)) {
				IMAGEHLP_MODULE64 moduleInfo = {};
				BOOL worked = SymGetModuleInfo64(process, symbol->ModBase, &moduleInfo);

				std::vector<module_info>::iterator curInfo = infos.begin();
				std::vector<module_info>::iterator end     = infos.end();
				for (; curInfo != end; ++curInfo) {
					module_info &info = (*curInfo);
					if (info.module_base == symbol->ModBase ||
					    info.module_base < symbol->Address &&
					        symbol->Address < info.module_base + info.module_size) {
						int len    = info.module_name.length() - 1;
						int modlen = 0;
						for (;;) {
							char c = info.module_name[len - modlen++];
							if (c == '\\' || c == '/') {
								break;
							}
						}

						for (int c = 0; c < modlen - 1; ++c)
							moduleInfo.ModuleName[c] = info.module_name[len - modlen + c + 2];

						moduleInfo.ModuleName[modlen] = '\n';
						break;
					}
				}

				if (SymGetLineFromAddr64(process, (DWORD64)(uintptr_t)stackPointer, &displacement,
				                         &line)) {
					sprintf(temp, "\t%s!%s\t0x%p %s:%i\n", moduleInfo.ModuleName, symbol->Name,
					        (void *)(uintptr_t)symbol->Address, line.FileName, line.LineNumber);
				} else {
					sprintf(temp, "\t%s!%s\t%#p\n", moduleInfo.ModuleName, symbol->Name,
					        symbol->Address);
				}
				fwrite(temp, 1, strlen(temp), fp);
			} else {
				// sprintf(temp, "\t%#p\n", allocs_[i]
			}
		}
	}

	sprintf(temp,
	        "\n\n\n\n\n-----------------------------------------------------------------------\n  "
	        "Frees: total - "
	        "%i\n-----------------------------------------------------------------------\n",
	        free_count_);
	fwrite(temp, strlen(temp), 1, fp);

	for (int i = 0; i < free_count_; ++i) {

		sprintf(temp, "\nAddress: %#p\nthread_id: %u\nPool: %#p\n", frees_[i].address,
		        frees_[i].thread, frees_[i].pool);
		fwrite(temp, 1, strlen(temp), fp);
		stack_trace *trace = allocs_[i].trace;
		for (int j = 0; j < trace->size && j < 256; ++j) {
			if (SymFromAddr(process, (DWORD64)(allocs_[i].trace->frames[j]), 0, symbol)) {
				sprintf(temp, "\t%s:\t%#p\n", symbol->Name, symbol->Address);
				fwrite(temp, 1, strlen(temp), fp);
			}
		}
	}
	fclose(fp);
}

int allocation_debug::find_stack_trace(const stack_trace &other) {
	int min = 0;
	int max = trace_count_ - 1;
	while (max >= min) {
		int index = (max + min) / 2;

		if (index >= trace_count_) {
			int balls = 2;
		}

		if (stack_traces_[index] < other) {
			min = index + 1;
		} else if (stack_traces_[index] > other) {
			max = index - 1;
		} else if (stack_traces_[index] == other)
			return index;
	}

	return -1;
}

void allocation_debug::add_alloc(size_t requested, size_t actual, ice::memory::memory_pool_i *pool,
                                 void *address) {
	if (pool == mem_pool_)
		return;

	if (mem_pool_ == 0) {
		InitializeCriticalSection(&critical_);
	}

	EnterCriticalSection(&critical_);

	void *temporary_frames[256];
	DWORD hash;
	WORD  count;
	if ((count = CaptureStackBackTrace(2, 255, temporary_frames, &hash)) > 0) {
		pre_add();

		stack_trace  temp(hash, count, temporary_frames);
		int          index = ice::memory::min_heap_find(stack_traces_, trace_count_, temp);
		stack_trace *trace;
		if (index >= 0) {
			trace = &stack_traces_[index];
		} else {
			void **new_frames = (void **)mem_pool_->malloc(sizeof(void *) * count);
			memcpy(new_frames, temporary_frames, sizeof(void *) * count);
			trace = new (&stack_traces_[trace_count_++]) stack_trace(hash, count, new_frames);
		}

		allocation_record *record = new (&allocs_[alloc_count_++])
		    allocation_record(requested, actual, address, pool, trace);
	}
	LeaveCriticalSection(&critical_);
}

void allocation_debug::add_free(ice::memory::memory_pool_i *pool, void *address) {
	if (pool == mem_pool_ || mem_pool_ == (ice::memory::memory_pool_i *)0x5747)
		return;

	EnterCriticalSection(&critical_);

	void *temporary_frames[256];
	DWORD hash;
	WORD  count;
	if ((count = CaptureStackBackTrace(2, 255, temporary_frames, &hash)) > 0) {
		pre_add();

		stack_trace  temp(hash, count, temporary_frames);
		int          index = ice::memory::min_heap_find(stack_traces_, trace_count_, temp);
		stack_trace *trace;
		if (index >= 0) {
			trace = &stack_traces_[index];
		} else {
			void **new_frames = (void **)mem_pool_->malloc(sizeof(void *) * count);
			memcpy(new_frames, temporary_frames, sizeof(void *) * count);
			trace = new (&stack_traces_[trace_count_++]) stack_trace(hash, count, new_frames);
		}

		free_record *record = new (&frees_[free_count_++]) free_record(address, pool, trace);
	}

	LeaveCriticalSection(&critical_);
}

void allocation_debug::pre_add() {
	if (mem_pool_ == 0) {
		static char memory_[sizeof(ice::memory::dl_memory_pool)];
		mem_pool_ = new (&memory_) ice::memory::dl_memory_pool(1024 * 1024 * 256);

		SymInitialize(GetCurrentProcess(), NULL, TRUE);
	}

	if (alloc_count_ >= alloc_cap_) {
		size_t multiplier = ice::math::max<size_t>(alloc_count_ % PREADD_INTERVAL, 1);
		alloc_cap_        = alloc_count_ + PREADD_INTERVAL * multiplier * 2;
		allocs_           = (allocation_record *)mem_pool_->realloc(allocs_,
		                                                            sizeof(allocation_record) * alloc_cap_);
	}

	if (free_count_ >= free_cap_) {
		size_t multiplier = ice::math::max<size_t>(free_count_ % PREADD_INTERVAL, 1);
		free_cap_         = free_count_ + PREADD_INTERVAL * multiplier * 2;
		frees_ = (free_record *)mem_pool_->realloc(frees_, sizeof(free_record) * free_cap_);
	}

	if (trace_count_ >= trace_cap_) {
		size_t multiplier = ice::math::max<size_t>(trace_count_ % PREADD_INTERVAL, 1);
		trace_cap_        = trace_count_ + PREADD_INTERVAL * multiplier * 2;
		stack_traces_ =
		    (stack_trace *)mem_pool_->realloc(stack_traces_, sizeof(stack_trace) * trace_cap_);
	}
}

#else
void allocation_debug::add_alloc(size_t requested [[maybe_unused]], size_t actual [[maybe_unused]],
                                 ice::memory::memory_pool_i *pool [[maybe_unused]],
                                 void                       *address [[maybe_unused]]) {}

void allocation_debug::add_free(ice::memory::memory_pool_i *pool [[maybe_unused]],
                                void                       *address [[maybe_unused]]) {}

void allocation_debug::save_to_file(const char *file_name [[maybe_unused]]) {}

#endif

} // namespace ice::memory::debug
