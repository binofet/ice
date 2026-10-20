#pragma once
#include <cstddef>
namespace ice::profiling::test {
void        watch_allocations(bool enabled) noexcept;
std::size_t allocation_attempts() noexcept;
} // namespace ice::profiling::test
