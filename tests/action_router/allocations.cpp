#include "allocations.h"
#include <cstdlib>
#include <new>
namespace {
thread_local std::ptrdiff_t remaining = -1;
thread_local bool           triggered = false;
void                        allocation() {
    if (remaining == 0) {
        remaining = -1;
        triggered = true;
        throw std::bad_alloc();
    }
    if (remaining > 0)
        --remaining;
}
} // namespace
namespace ice::test::route_allocations {
void arm(std::ptrdiff_t ordinal) {
	remaining = ordinal;
	triggered = false;
}
void disarm() { remaining = -1; }
bool fired() { return triggered; }
} // namespace ice::test::route_allocations
void *operator new(std::size_t size) {
	allocation();
	if (auto *p = std::malloc(size ? size : 1))
		return p;
	throw std::bad_alloc();
}
void *operator new[](std::size_t size) { return ::operator new(size); }
void  operator delete(void *p) noexcept { std::free(p); }
void  operator delete[](void *p) noexcept { std::free(p); }
void  operator delete(void *p, std::size_t) noexcept { std::free(p); }
void  operator delete[](void *p, std::size_t) noexcept { std::free(p); }
