#pragma once

#include "ice/memory/abstract_memory_pool.h"

#include "TLSF/tlsf.h"
namespace ice::memory {

#ifdef __APPLE__
} // namespace ice::memory
#include <memory.h>
namespace ice::memory {
#endif

/// Owned TLSF allocator storage with explicit capacity and lifecycle; do not free pool blocks
/// through another allocator.
class tlsf_memory_pool : public abstract_memory_pool {
public:
	/// Create/replace owned resources from the supplied description; inspect status before using
	/// the new resource.
	ice::result create(size_t max_capacity) {
		raw_    = ::malloc(max_capacity + 1024 * 3);
		memory_ = tlsf_create(raw_, max_capacity);
		if (memory_ != 0) {
			return IC_OK;
		}
		return IC_FAIL_OUT_OF_MEM;
	}

	/// Release this allocators owned backing storage after all client allocations have been
	/// returned.
	ice::result destroy() {
		if (memory_ != 0) {
			tlsf_destroy(memory_);
			memory_ = 0;
		}
		if (raw_ != 0) {
			free(raw_);
			raw_ = 0;
		}

		return IC_OK;
	}

	/// Allocate the requested number of bytes from this pool; null reports allocation failure.
	void *malloc(size_t byte_count) {
		size_t actual = 0;
		void  *result = tlsf_malloc(memory_, byte_count, &actual);
		on_allocated(byte_count, actual, result);
		return result;
	}

	/// Allocate zero-filled element storage from this pool; null reports allocation failure.
	void *calloc(size_t element_count, size_t element_size) {
		return this->malloc(element_count * element_size);
	}

	/// Allocate bytes with the requested supported alignment; free through the same pool.
	void *memalign(size_t alignment, size_t byte_count) {
		return tlsf_memalign(memory_, alignment, byte_count);
	}

	/// Resize a block owned by this pool; inspect the returned pointer before discarding the old
	/// allocation.
	void *realloc(void *ptr, size_t byte_count) { return tlsf_realloc(memory_, ptr, byte_count); }

	/// Return an allocation/image to its owning allocator/object; borrowed pointers into it become
	/// invalid.
	void free(void *ptr) {
		tlsf_free(memory_, ptr);
		on_freed(ptr);
	}

private:
	tlsf_pool memory_;
	void     *raw_;
};

} // namespace ice::memory
