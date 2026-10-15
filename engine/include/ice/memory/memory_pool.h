#pragma once

#include "ice/globals.h"
namespace ice::memory {

/// Explicit allocator lifecycle interface. Returned blocks belong to the allocating pool and must
/// be freed through that pool.
class memory_pool_i {
public:
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	virtual ~memory_pool_i() { destroy(); }

	/// Create/replace owned resources from the supplied description; inspect status before using
	/// the new resource.
	virtual ice::result create(size_t max_capacity) = 0;

	/// Release this allocators owned backing storage after all client allocations have been
	/// returned.
	virtual ice::result destroy() { return IC_NOT_IMPL; }

	/// Allocate the requested number of bytes from this pool; null reports allocation failure.
	virtual void *malloc(size_t byte_count) = 0;

	/// Allocate zero-filled element storage from this pool; null reports allocation failure.
	virtual void *calloc(size_t element_count, size_t element_size) = 0;

	/// Allocate bytes with the requested supported alignment; free through the same pool.
	virtual void *memalign(size_t alignment, size_t byte_count) = 0;

	/// Resize a block owned by this pool; inspect the returned pointer before discarding the old
	/// allocation.
	virtual void *realloc(void *ptr, size_t byte_count) = 0;

	/// Return an allocation/image to its owning allocator/object; borrowed pointers into it become
	/// invalid.
	virtual void free(void *ptr) = 0;

protected:
	virtual void on_allocated(size_t requested, size_t actual, void *address) = 0;

	virtual void on_freed(void *address) = 0;
};

} // namespace ice::memory
