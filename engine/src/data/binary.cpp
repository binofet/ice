#include "internal.h"
#include <bit>
#include <cstring>

namespace ice::data {
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
namespace detail {
bool valid_text(std::string_view input, std::size_t limit) {
	if (input.size() > limit)
		return false;
	for (std::size_t i = 0; i < input.size();) {
		auto first = static_cast<unsigned char>(input[i++]);
		if (!first)
			return false;
		if (first < 128)
			continue;
		unsigned count = first >= 0xF0 ? 3 : first >= 0xE0 ? 2 : first >= 0xC2 ? 1 : 0;
		if (!count || first > 0xF4 || count > input.size() - i)
			return false;
		std::uint32_t point = first & ((1u << (6 - count)) - 1);
		for (unsigned j = 0; j < count; ++j) {
			auto next = static_cast<unsigned char>(input[i++]);
			if ((next & 0xC0) != 0x80)
				return false;
			point = (point << 6) | (next & 0x3F);
		}
		if (point < (count == 1   ? 0x80u
		             : count == 2 ? 0x800u
		                          : 0x10000u) ||
		    point > 0x10FFFF || (point >= 0xD800 && point <= 0xDFFF))
			return false;
	}
	return true;
}
} // namespace detail
namespace {
constexpr std::size_t max_bytes = 1024 * 1024;
struct tree_budget {
	std::size_t nodes = 65536;
	std::size_t bytes = max_bytes;
	void        visit(const value &input, unsigned depth = 0) {
        if (depth > 48 || nodes == 0 || bytes < 8)
            detail::fail("Tree depth/node/byte budget exceeded");
        --nodes;
        bytes -= 8;
        if (auto string = std::get_if<std::string>(&input.data)) {
            if (!detail::valid_text(*string) || string->size() > bytes)
                detail::fail("Invalid text or tree byte budget exceeded");
            bytes -= string->size();
        } else if (auto number = std::get_if<double>(&input.data)) {
            if (!std::isfinite(*number))
                detail::fail("Nonfinite portable number");
        } else if (auto array = std::get_if<value::sequence>(&input.data)) {
            if (array->size() > 4096)
                detail::fail("Container item budget exceeded");
            for (const auto &item : *array)
                visit(item, depth + 1);
        } else if (auto object = std::get_if<value::record>(&input.data)) {
            if (object->size() > 4096)
                detail::fail("Record item budget exceeded");
            for (const auto &[name, item] : *object) {
                visit(value{name}, depth + 1);
                visit(item, depth + 1);
            }
        } else if (std::holds_alternative<object_reference>(input.data)) {
            detail::fail("An authored tree cannot contain a runtime-resolved reference");
        }
	}
};
void uint(std::vector<std::byte> &output, std::uint64_t number) {
	while (number >= 128) {
		output.push_back(std::byte((number & 127) | 128));
		number >>= 7;
	}
	output.push_back(std::byte(number));
}
void encode(const value &input, std::vector<std::byte> &output) {
	auto tag = [&](unsigned number) { output.push_back(std::byte(number)); };
	if (detail::null(input)) {
		tag(0);
	} else if (auto boolean = std::get_if<bool>(&input.data)) {
		tag(*boolean ? 2 : 1);
	} else if (auto number = std::get_if<std::int64_t>(&input.data)) {
		tag(*number < 0 ? 4 : 3);
		uint(output, *number < 0 ? static_cast<std::uint64_t>(-1 - *number)
		                         : static_cast<std::uint64_t>(*number));
	} else if (auto number = std::get_if<std::uint64_t>(&input.data)) {
		tag(3);
		uint(output, *number);
	} else if (auto number = std::get_if<double>(&input.data)) {
		tag(5);
		auto bits = std::bit_cast<std::uint64_t>(*number);
		for (unsigned i = 0; i < 8; ++i)
			output.push_back(std::byte((bits >> (i * 8)) & 255));
	} else if (auto string = std::get_if<std::string>(&input.data)) {
		tag(6);
		uint(output, string->size());
		for (unsigned char c : *string)
			output.push_back(std::byte(c));
	} else if (auto array = std::get_if<value::sequence>(&input.data)) {
		tag(7);
		uint(output, array->size());
		for (const auto &item : *array)
			encode(item, output);
	} else if (auto object = std::get_if<value::record>(&input.data)) {
		tag(8);
		uint(output, object->size());
		for (const auto &[name, item] : *object) {
			encode(value{name}, output);
			encode(item, output);
		}
	} else {
		detail::fail("Unsupported binary value");
	}
	if (output.size() > max_bytes)
		detail::fail("Encoded byte budget exceeded");
}
struct decoder {
	std::span<const std::byte> input;
	std::size_t                position  = 36;
	std::size_t                remaining = 65536;
	unsigned                   byte() {
        if (position == input.size())
            detail::fail("Truncated binary document");
        return std::to_integer<unsigned>(input[position++]);
	}
	std::uint64_t uint() {
		std::uint64_t number = 0;
		for (unsigned i = 0; i < 10; ++i) {
			auto next = byte();
			if (i == 9 && next > 1)
				detail::fail("Varint overflow");
			number |= std::uint64_t(next & 127) << (i * 7);
			if (next < 128) {
				if (i && next == 0)
					detail::fail("Noncanonical varint");
				return number;
			}
		}
		detail::fail("Invalid varint");
	}
	value read(unsigned depth = 0) {
		if (depth > 48 || remaining == 0)
			detail::fail("Binary tree depth/node budget exceeded");
		--remaining;
		auto tag = byte();
		if (tag == 0)
			return {};
		if (tag <= 2)
			return value{tag == 2};
		if (tag == 3)
			return value{uint()};
		if (tag == 4) {
			auto number = uint();
			if (number > INT64_MAX)
				detail::fail("Negative integer overflow");
			return value{std::int64_t(-1 - static_cast<std::int64_t>(number))};
		}
		if (tag == 5) {
			std::uint64_t bits = 0;
			for (unsigned i = 0; i < 8; ++i)
				bits |= std::uint64_t(byte()) << (i * 8);
			auto number = std::bit_cast<double>(bits);
			if (!std::isfinite(number))
				detail::fail("Nonfinite portable number");
			return value{number};
		}
		if (tag == 6) {
			auto size = uint();
			if (size > 4096 || size > input.size() - position)
				detail::fail("Invalid string length");
			std::string string(reinterpret_cast<const char *>(input.data() + position),
			                   static_cast<std::size_t>(size));
			position += static_cast<std::size_t>(size);
			if (!detail::valid_text(string))
				detail::fail("Invalid UTF-8 or NUL");
			return value{std::move(string)};
		}
		if (tag != 7 && tag != 8)
			detail::fail("Unknown binary tag");
		auto count = uint();
		if (count > 4096 || count > remaining)
			detail::fail("Container item budget exceeded");
		if (tag == 7) {
			value::sequence result;
			result.reserve(static_cast<std::size_t>(count));
			for (std::uint64_t i = 0; i < count; ++i)
				result.push_back(read(depth + 1));
			return value{std::move(result)};
		}
		value::record              result;
		std::optional<std::string> previous;
		for (std::uint64_t i = 0; i < count; ++i) {
			auto key  = read(depth + 1);
			auto name = detail::text(key);
			if (previous && name <= *previous)
				detail::fail("Noncanonical or duplicate object key");
			previous = name;
			result.emplace(std::move(name), read(depth + 1));
		}
		return value{std::move(result)};
	}
};
} // namespace
ice::expected<void, diagnostic> validate_tree(const value &input) {
	try {
		tree_budget{}.visit(input);
		return {};
	} catch (const detail::invalid &error) {
		return ice::unexpected(error.error);
	}
}
ice::expected<std::vector<std::byte>, diagnostic>
encode_binary(const value &input, const schema_fingerprint &fingerprint) {
	try {
		tree_budget{}.visit(input);
		std::vector<std::byte> output = {std::byte{'M'}, std::byte{'D'}, std::byte{'B'},
		                                 std::byte{'1'}};
		output.insert(output.end(), fingerprint.begin(), fingerprint.end());
		encode(input, output);
		return output;
	} catch (const detail::invalid &error) {
		return ice::unexpected(error.error);
	}
}
ice::expected<value, diagnostic> decode_binary(std::span<const std::byte> input,
                                               const schema_fingerprint  &fingerprint) {
	try {
		if (input.size() < 37 || input.size() > max_bytes ||
		    std::memcmp(input.data(), "MDB1", 4) != 0 ||
		    !std::equal(fingerprint.begin(), fingerprint.end(), input.begin() + 4))
			detail::fail("Binary header/fingerprint/byte budget mismatch");
		decoder source{input};
		auto    result = source.read();
		if (source.position != input.size())
			detail::fail("Trailing binary data");
		tree_budget{}.visit(result);
		return result;
	} catch (const detail::invalid &error) {
		return ice::unexpected(error.error);
	}
}
} // namespace ice::data
