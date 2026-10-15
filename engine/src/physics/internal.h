#pragma once
#include <cstdint>
namespace ice::physics::detail {
// Zero means the process-local, non-reusing world lifetime space is exhausted.
std::uint64_t next_world_identity();
} // namespace ice::physics::detail
