#include "allocations.h"
#include <cstdlib>
#include <new>
namespace {
thread_local bool        watching = false;
thread_local std::size_t attempts = 0;
} // namespace
namespace ice::profiling::test {
void watch_allocations(bool enabled) noexcept {
	watching = enabled;
	if (enabled)
		attempts = 0;
}
std::size_t allocation_attempts() noexcept { return attempts; }
} // namespace ice::profiling::test
void *operator new(std::size_t bytes) {
	if (watching)
		++attempts;
	if (auto *memory = std::malloc(bytes ? bytes : 1))
		return memory;
	throw std::bad_alloc();
}
void *operator new[](std::size_t bytes) { return ::operator new(bytes); }
void  operator delete(void *memory) noexcept { std::free(memory); }
void  operator delete[](void *memory) noexcept { ::operator delete(memory); }
void  operator delete(void *memory, std::size_t) noexcept { ::operator delete(memory); }
void  operator delete[](void *memory, std::size_t) noexcept { ::operator delete(memory); }
