#include "command_allocations.h"
#include <cstdlib>
#include <new>

namespace {
thread_local bool        rejecting = false;
thread_local std::size_t observed  = 0;
} // namespace
namespace command_allocations {
void arm() {
	observed  = 0;
	rejecting = true;
}
void        disarm() { rejecting = false; }
std::size_t attempts() { return observed; }
} // namespace command_allocations

// Keep allocation replacements in a separate TU from standard-library callers.
void *operator new(std::size_t bytes) {
	if (rejecting) {
		++observed;
		throw std::bad_alloc();
	}
	if (auto *memory = std::malloc(bytes ? bytes : 1))
		return memory;
	throw std::bad_alloc();
}
void *operator new[](std::size_t bytes) { return ::operator new(bytes); }
void  operator delete(void *memory) noexcept { std::free(memory); }
void  operator delete[](void *memory) noexcept { ::operator delete(memory); }
void  operator delete(void *memory, [[maybe_unused]] std::size_t bytes) noexcept {
    ::operator delete(memory);
}
void operator delete[](void *memory, [[maybe_unused]] std::size_t bytes) noexcept {
	::operator delete(memory);
}
