// Generated mapping consumer; do not edit.
#pragma once
#include "sequence_adapters.h"
#if __cplusplus > 202002L
#include <playground/custom_vector.h>
namespace metagen_examples {
using mapped_sequence        = playground::custom_vector<std::int32_t>;
using mapped_nested_sequence = playground::custom_vector<playground::custom_vector<std::int32_t>>;
using mapped_adapter         = playground::vector_adapter<std::int32_t>;
using mapped_nested_adapter  = playground::vector_adapter<mapped_sequence>;
using mapped_codec           = sequence_codec<mapped_adapter, int32_codec>;
using mapped_nested_codec    = sequence_codec<mapped_nested_adapter, mapped_codec>;
} // namespace metagen_examples
#else
#include <vector>
namespace metagen_examples {
using mapped_sequence        = std::vector<std::int32_t>;
using mapped_nested_sequence = std::vector<std::vector<std::int32_t>>;
using mapped_adapter         = metagen_examples::vector_adapter<std::int32_t>;
using mapped_nested_adapter  = metagen_examples::vector_adapter<mapped_sequence>;
using mapped_codec           = sequence_codec<mapped_adapter, int32_codec>;
using mapped_nested_codec    = sequence_codec<mapped_nested_adapter, mapped_codec>;
} // namespace metagen_examples
#endif
