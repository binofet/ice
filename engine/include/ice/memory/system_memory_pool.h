#pragma once

#include "ice/memory/abstract_memory_pool.h"
#include <cstdlib>
#include <limits>
namespace ice::memory {

/// Allocator backed by the platform/system heap; blocks require the matching pool free operation.
class system_memory_pool : public abstract_memory_pool {
public:
	/// Construct with the borrowed dependencies described by this type; initialize explicitly
	/// before active use.
	system_memory_pool([[maybe_unused]] size_t max_capacity) {};

	/// Create/replace owned resources from the supplied description; inspect status before using
	/// the new resource.
	ice::result create([[maybe_unused]] size_t max_capacity) { return IC_OK; }

	/// Release this allocators owned backing storage after all client allocations have been
	/// returned.
	ice::result destroy() { return IC_OK; }

	/// Allocate the requested number of bytes from this pool; null reports allocation failure.
	void *malloc(size_t byte_count) { return std::malloc(byte_count); }

	/// Allocate zero-filled element storage from this pool; null reports allocation failure.
	void *calloc(size_t element_count, size_t element_size) {
		return std::calloc(element_count, element_size);
	}

	/// Allocate bytes with the requested supported alignment; free through the same pool.
	void *memalign(size_t alignment, size_t byte_count) {
		if (alignment < sizeof(void *) || (alignment & (alignment - 1)) != 0 ||
		    byte_count > std::numeric_limits<size_t>::max() - (alignment - 1))
			return nullptr;
#ifdef _WIN32
		// The general free operation cannot release _aligned_malloc blocks.
		return nullptr;
#else
		return std::aligned_alloc(alignment, (byte_count + alignment - 1) & ~(alignment - 1));
#endif
	}

	/// Resize a block owned by this pool; inspect the returned pointer before discarding the old
	/// allocation.
	void *realloc(void *ptr, size_t byte_count) { return std::realloc(ptr, byte_count); }

	/// Return an allocation/image to its owning allocator/object; borrowed pointers into it become
	/// invalid.
	void free(void *ptr) { std::free(ptr); }
};

} // namespace ice::memory
