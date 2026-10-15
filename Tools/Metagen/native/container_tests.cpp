#include "containers.generated.h"
#include <cstdio>

#define CHECK(expression)                                                                          \
	do {                                                                                           \
		if (!(expression)) {                                                                       \
			std::fprintf(stderr, "Container proof failed %d: %s\n", __LINE__, #expression);        \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

namespace {
// Valid adapter whose detached construction intentionally fails after one element.
struct failing_adapter : metagen_examples::vector_adapter<std::int32_t> {
	static metagen_examples::result<void> append(builder_type &builder, std::int32_t value) {
		if (builder.size() == 1)
			return ice::unexpected(std::string("Simulated custom-container insertion failure"));
		builder.push_back(value);
		return {};
	}
};
} // namespace

int main() {
	using namespace metagen_examples;
	static_assert(sequence_adapter<mapped_adapter>);
	static_assert(sequence_adapter<mapped_nested_adapter>);
	auto inner = mapped_adapter::make(3);
	CHECK(inner);
	for (std::int32_t value : {1, -2, INT32_MAX})
		CHECK(mapped_adapter::append(*inner, value));
	auto outer = mapped_nested_adapter::make(2);
	CHECK(outer);
	CHECK(mapped_nested_adapter::append(*outer, *inner));
	CHECK(mapped_nested_adapter::append(*outer, mapped_sequence{}));
	std::vector<std::uint8_t> bytes;
	CHECK(mapped_nested_codec::write(*outer, bytes));
	// The adapter mapping changes memory/storage, while the serialized values stay identical.
	const std::vector<std::uint8_t> expected = {2,   0,   0,   0,   3,   0,   0,   0,   1, 0, 0, 0,
	                                            254, 255, 255, 255, 255, 255, 255, 127, 0, 0, 0, 0};
	CHECK(bytes == expected);
	reader input{bytes};
	auto   decoded = mapped_nested_codec::read(input);
	CHECK(decoded && *decoded == *outer && input.position == bytes.size());
	for (std::size_t size = 0; size < bytes.size(); ++size) {
		reader truncated{std::span(bytes).first(size)};
		CHECK(!mapped_nested_codec::read(truncated));
	}
	std::vector<std::uint8_t> too_many = {255, 255, 255, 255};
	reader                    oversized{too_many};
	CHECK(!mapped_nested_codec::read(oversized));
	std::vector<std::uint8_t> leaf_bytes;
	CHECK(mapped_codec::write(*inner, leaf_bytes));
	reader poisoned{leaf_bytes};
	auto   rejected = sequence_codec<failing_adapter, int32_codec>::read(poisoned);
	CHECK(!rejected && rejected.error().find("insertion failure") != std::string::npos);
	// Rejected detached reads never touched the original application value.
	CHECK(mapped_adapter::size(*inner) == 3);
	std::puts(
	    "Generated container mapping, nested serialization, bounds and detached failure passed");
	return 0;
}
