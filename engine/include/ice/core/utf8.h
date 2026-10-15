#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace ice::core {
/// Validate bounded UTF-8 text without allocation. Reject NUL, malformed/overlong sequences,
/// surrogate code points and values above U+10FFFF; empty text is valid.
inline bool valid_utf8_text(std::string_view input,
                            std::size_t      max_bytes = std::numeric_limits<std::size_t>::max()) {
	if (input.size() > max_bytes)
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
} // namespace ice::core
