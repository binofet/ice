#pragma once
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <string>

namespace ice::platform::detail {
inline bool append_utf8(std::string &text, std::uint32_t point) {
	if (!point || point > 0x10FFFF || (point >= 0xD800 && point <= 0xDFFF))
		return false;
	if (point < 0x80)
		text.push_back(char(point));
	else if (point < 0x800) {
		text.push_back(char(0xC0 | (point >> 6)));
		text.push_back(char(0x80 | (point & 63)));
	} else if (point < 0x10000) {
		text.push_back(char(0xE0 | (point >> 12)));
		text.push_back(char(0x80 | ((point >> 6) & 63)));
		text.push_back(char(0x80 | (point & 63)));
	} else {
		text.push_back(char(0xF0 | (point >> 18)));
		text.push_back(char(0x80 | ((point >> 12) & 63)));
		text.push_back(char(0x80 | ((point >> 6) & 63)));
		text.push_back(char(0x80 | (point & 63)));
	}
	return text.size() <= 4096;
}
// XIM supplies a character count, not a byte extent or a terminator. Feed the
// locale decoder one byte at a time and stop immediately after the declared final
// character. Each read belongs to that native character; no trailing byte is probed.
inline bool locale_characters(const char *text, std::size_t characters, std::string &result) {
	result.clear();
	if ((!text && characters) || characters > 4096)
		return false;
	std::mbstate_t state{};
	std::size_t    total_bytes = 0;
	for (std::size_t index = 0; index < characters; ++index) {
		bool complete = false;
		for (int byte = 0; byte < MB_LEN_MAX && total_bytes < 4096; ++byte) {
			wchar_t    point = 0;
			const auto count = std::mbrtowc(&point, text + total_bytes, 1, &state);
			++total_bytes;
			if (count == std::size_t(-1) || count == 0)
				return false;
			if (count == std::size_t(-2))
				continue;
			if (!append_utf8(result, std::uint32_t(point)))
				return false;
			complete = true;
			break;
		}
		if (!complete)
			return false;
	}
	return true;
}
} // namespace ice::platform::detail
