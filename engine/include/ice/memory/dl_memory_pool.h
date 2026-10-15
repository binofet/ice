#pragma once

#include "ice/memory/abstract_memory_pool.h"

#include "lea_malloc/leamalloc.h"
namespace ice::memory {

/// Owned dlmalloc mspace with explicit create/destroy lifecycle and pool-local allocation APIs.
class dl_memory_pool : public abstract_memory_pool {
public:
	/// Construct with the borrowed dependencies described by this type; initialize explicitly
	/// before active use.
	dl_memory_pool(size_t max_capacity) { create(max_capacity); }

	/// Create/replace owned resources from the supplied description; inspect status before using
	/// the new resource.
	ice::result create(size_t max_capacity) {
		memory_space_ = create_mspace(max_capacity, 0);
		if (memory_space_ != 0) {
			return IC_OK;
		}
		return IC_FAIL_OUT_OF_MEM;
	}

	/// Release this allocators owned backing storage after all client allocations have been
	/// returned.
	ice::result destroy() {
		if (memory_space_ != 0) {
			destroy_mspace(memory_space_);
			memory_space_ = 0;
			return IC_OK;
		}
		return IC_FAIL_GEN;
	}

	/// Allocate the requested number of bytes from this pool; null reports allocation failure.
	void *malloc(size_t byte_count) {
		size_t actual = 0;
		void  *result = mspace_malloc(memory_space_, byte_count, &actual);
		on_allocated(byte_count, actual, result);
		return result;
	}

	/// Allocate zero-filled element storage from this pool; null reports allocation failure.
	void *calloc(size_t element_count, size_t element_size) {
		return mspace_calloc(memory_space_, element_count, element_size);
	}

	/// Allocate bytes with the requested supported alignment; free through the same pool.
	void *memalign(size_t alignment, size_t byte_count) {
		return mspace_memalign(memory_space_, alignment, byte_count);
	}

	/// Resize a block owned by this pool; inspect the returned pointer before discarding the old
	/// allocation.
	void *realloc(void *ptr, size_t byte_count) {
		return mspace_realloc(memory_space_, ptr, byte_count);
	}

	/// Return an allocation/image to its owning allocator/object; borrowed pointers into it become
	/// invalid.
	void free(void *ptr) {
		mspace_free(memory_space_, ptr);
		on_freed(ptr);
	}

private:
	mspace memory_space_;
};

} // namespace ice::memory
