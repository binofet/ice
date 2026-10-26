#pragma once
#include <cstddef>

namespace command_allocations {
void        arm();
void        disarm();
std::size_t attempts();
} // namespace command_allocations
