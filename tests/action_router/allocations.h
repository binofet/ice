#pragma once
#include <cstddef>
namespace ice::test::route_allocations {
void arm(std::ptrdiff_t ordinal);
void disarm();
bool fired();
} // namespace ice::test::route_allocations
