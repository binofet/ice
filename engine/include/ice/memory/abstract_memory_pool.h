#pragma once

#include "ice/memory/memory_pool.h"
namespace ice::memory {

/// Allocator base that adds tracking hooks; derived classes supply allocation/free behavior.
class abstract_memory_pool : public memory_pool_i {
protected:
	virtual void on_allocated(size_t requested, size_t actual, void *address);

	virtual void on_freed(void *address);
};

} // namespace ice::memory
