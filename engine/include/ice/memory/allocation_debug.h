#pragma once

#include "ice/globals.h"
#include "ice/memory/memory_pool.h"
#include <string>
#ifndef WIN32
#include <pthread.h>
#endif

namespace ice::memory::debug {

#if defined(WIN32) || defined(XBOX)
typedef DWORD thread_id;
#elif defined(POSIX)
typedef unsigned int thread_id;
#elif !defined(WIN32)
/// Native thread identifier used by non-owning allocation diagnostics.
typedef pthread_t thread_id;
#endif

/// Captured stack-frame addresses plus a lookup hash; frame storage is owned by the allocation
/// tracker.
class stack_trace {
public:
	/// Borrow captured stack-frame storage and retain its lookup hash/count.
	stack_trace(unsigned int hash, size_t size, void **frames) {
		this->hash   = hash;
		this->size   = size;
		this->frames = frames;
	}

	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	bool operator<(const stack_trace &rhs) { return hash < rhs.hash; }

	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	bool operator>(const stack_trace &rhs) { return hash > rhs.hash; }

	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	bool operator==(const stack_trace &rhs) {
		if (hash == rhs.hash) {
			if (size == rhs.size) {
				// TODO: order has performance impact, investigate.
				for (int i = 0; i < (int)size; ++i) {
					if (frames[i] != rhs.frames[i])
						return false;
				}
				return true;
			}
			return false;
		}
		return false;
	}

	/// private:
	unsigned int hash;
	/// Number of valid entries/bytes as specified by the owning type.
	size_t size;
	/// Borrowed captured stack record/frame array owned by the tracker.
	void **frames;
};

/// Non-owning allocation diagnostics, including requested/actual byte sizes, address, pool, and
/// captured stack.
class allocation_record {
public:
	/// Capture non-owning allocation metadata; the tracked application block remains owned by its
	/// pool.
	allocation_record(size_t requested, size_t actual, void *address,
	                  ice::memory::memory_pool_i *pool, stack_trace *trace) {
		this->requested = requested;
		this->actual    = actual;
		this->address   = address;
		this->pool      = pool;
		this->trace     = trace;
#ifndef WIN32
		thread = pthread_self();
#elif (defined(WIN32))
		thread = GetCurrentThreadId();
#endif
	}

	/// private:
	size_t requested;
	/// Actual allocated byte count.
	size_t actual;
	/// Borrowed tracked allocation address.
	void *address;
	/// Borrowed allocator identity responsible for the block.
	ice::memory::memory_pool_i *pool;
	/// Borrowed captured stack record/frame array owned by the tracker.
	stack_trace *trace;
	/// Native identity of the allocating/freeing thread.
	thread_id thread;
};

/// Non-owning free diagnostics associated with a tracked allocation address and captured stack.
class free_record {
public:
	/// Capture a non-owning free diagnostic for the supplied address and stack record.
	free_record(void *address, ice::memory::memory_pool_i *pool, stack_trace *trace) {
		this->address = address;
		this->pool    = pool;
		this->trace   = trace;
#ifndef WIN32
		thread = pthread_self();
#else
		thread = GetCurrentThreadId();
#endif
	}

	/// private:
	void *address;
	/// Borrowed allocator identity responsible for the block.
	ice::memory::memory_pool_i *pool;
	/// Borrowed captured stack record/frame array owned by the tracker.
	stack_trace *trace;
	/// Native identity of the allocating/freeing thread.
	thread_id thread;
};

/// Optional allocation tracker. Tracking records are internal diagnostics and do not own the
/// application allocations they describe.
class allocation_debug {
public:
	/// Write the current decoded/captured data to the supplied output file.
	static void save_to_file(const char *file_name);

	/// Record a non-owning allocation diagnostic with requested/actual byte sizes.
	static void add_alloc(size_t requested, size_t actual, ice::memory::memory_pool_i *pool,
	                      void *address);

	/// Record a non-owning free diagnostic for the supplied address/pool.
	static void add_free(ice::memory::memory_pool_i *pool, void *address);

private:
	static void pre_add();
	static int  find_stack_trace(const stack_trace &other);
#ifdef WIN32
	static BOOL CALLBACK enumerate_loaded_modules(PCTSTR module_name, DWORD64 module_base,
	                                              ULONG module_size, PVOID user_context);

	static CRITICAL_SECTION critical_;
#endif

	static ice::memory::memory_pool_i *mem_pool_;
	static size_t                      alloc_count_;
	static size_t                      free_count_;
	static allocation_record          *allocs_;
	static free_record                *frees_;

	static size_t       trace_count_;
	static stack_trace *stack_traces_;

	static size_t trace_cap_;
	static size_t alloc_cap_;
	static size_t free_cap_;
};

} // namespace ice::memory::debug
