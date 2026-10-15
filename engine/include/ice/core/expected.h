#pragma once

#include <tl/expected.hpp>

namespace ice {
/// Checked success/failure value; the same implementation is used in both supported language modes.
template <typename T, typename E> using expected = tl::expected<T, E>;
/// Explicit failure payload used to construct an expected value.
using tl::unexpected;
} // namespace ice
