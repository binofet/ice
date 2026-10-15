#include "ice/memory/abstract_memory_pool.h"
#include "ice/memory/allocation_debug.h"
namespace ice::memory {

using namespace ice::memory::debug;

void abstract_memory_pool::on_allocated(size_t requested [[maybe_unused]],
                                        size_t actual [[maybe_unused]],
                                        void  *address [[maybe_unused]]) {
	// AllocationDebug::AddAlloc(requested, actual, this, address);
}

void abstract_memory_pool::on_freed(void *address [[maybe_unused]]) {
	// AllocationDebug::AddFree(this, address);
}

} // namespace ice::memory
