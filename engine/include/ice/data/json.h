#pragma once

#include "ice/data/document.h"
#include <string_view>

namespace ice::data {

/// Decode one complete UTF-8 JSON document into owned portable values. Reject duplicate keys,
/// NUL, invalid Unicode, nonfinite/out-of-range numbers and trailing input. The budgets are one
/// MiB, 48 nested levels, 65536 nodes and 4096 items/bytes per container/string.
/// Nonnegative integers become uint64; negative integers become int64; fractions/exponents
/// become double. JSON does not retain a positive integer's signedness or number spelling.
ice::expected<value, diagnostic> decode_json(std::string_view input);

/// Encode a bounded portable tree as deterministic compact JSON, with sorted object keys.
/// Reject resolved runtime references; authored reference tags remain ordinary objects.
/// Values must satisfy the portable-tree and one-MiB encoded-size budgets of decode_json.
ice::expected<std::string, diagnostic> encode_json(const value &input);

} // namespace ice::data
