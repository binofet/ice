#pragma once
#include "ice/platform/input_types.h"
#include <array>
#include <string_view>
#include <utility>
namespace ice::platform::detail {
inline physical_key mac_physical_key(unsigned code) {
	using enum physical_key;
	static constexpr std::pair<unsigned, physical_key> keys[] = {{0, a},
	                                                             {1, s},
	                                                             {2, d},
	                                                             {3, f},
	                                                             {4, h},
	                                                             {5, g},
	                                                             {6, z},
	                                                             {7, x},
	                                                             {8, c},
	                                                             {9, v},
	                                                             {11, b},
	                                                             {12, q},
	                                                             {13, w},
	                                                             {14, e},
	                                                             {15, r},
	                                                             {16, y},
	                                                             {17, t},
	                                                             {18, digit_1},
	                                                             {19, digit_2},
	                                                             {20, digit_3},
	                                                             {21, digit_4},
	                                                             {22, digit_6},
	                                                             {23, digit_5},
	                                                             {24, equal},
	                                                             {25, digit_9},
	                                                             {26, digit_7},
	                                                             {27, minus},
	                                                             {28, digit_8},
	                                                             {29, digit_0},
	                                                             {30, right_bracket},
	                                                             {31, o},
	                                                             {32, u},
	                                                             {33, left_bracket},
	                                                             {34, i},
	                                                             {35, p},
	                                                             {36, enter},
	                                                             {37, l},
	                                                             {38, j},
	                                                             {39, apostrophe},
	                                                             {40, k},
	                                                             {41, semicolon},
	                                                             {42, backslash},
	                                                             {43, comma},
	                                                             {44, slash},
	                                                             {45, n},
	                                                             {46, m},
	                                                             {47, period},
	                                                             {48, tab},
	                                                             {49, space},
	                                                             {50, grave},
	                                                             {51, backspace},
	                                                             {53, escape},
	                                                             {54, right_super},
	                                                             {55, left_super},
	                                                             {56, left_shift},
	                                                             {57, caps_lock},
	                                                             {58, left_alt},
	                                                             {59, left_control},
	                                                             {60, right_shift},
	                                                             {61, right_alt},
	                                                             {62, right_control},
	                                                             {65, keypad_period},
	                                                             {67, keypad_multiply},
	                                                             {69, keypad_plus},
	                                                             {71, num_lock},
	                                                             {75, keypad_divide},
	                                                             {76, keypad_enter},
	                                                             {78, keypad_minus},
	                                                             {81, keypad_equal},
	                                                             {82, keypad_0},
	                                                             {83, keypad_1},
	                                                             {84, keypad_2},
	                                                             {85, keypad_3},
	                                                             {86, keypad_4},
	                                                             {87, keypad_5},
	                                                             {88, keypad_6},
	                                                             {89, keypad_7},
	                                                             {91, keypad_8},
	                                                             {92, keypad_9},
	                                                             {96, f5},
	                                                             {97, f6},
	                                                             {98, f7},
	                                                             {99, f3},
	                                                             {100, f8},
	                                                             {101, f9},
	                                                             {103, f11},
	                                                             {105, f13},
	                                                             {106, f16},
	                                                             {107, f14},
	                                                             {109, f10},
	                                                             {111, f12},
	                                                             {113, f15},
	                                                             {114, insert},
	                                                             {115, home},
	                                                             {116, page_up},
	                                                             {117, delete_forward},
	                                                             {118, f4},
	                                                             {119, end},
	                                                             {120, f2},
	                                                             {121, page_down},
	                                                             {122, f1},
	                                                             {123, left},
	                                                             {124, right},
	                                                             {125, down},
	                                                             {126, up}};
	for (auto [native, key] : keys)
		if (native == code)
			return key;
	return unknown;
}
inline physical_key win32_physical_key(unsigned scan, bool extended) {
	using enum physical_key;
	if (extended) {
		switch (scan) {
		case 0x1c:
			return keypad_enter;
		case 0x1d:
			return right_control;
		case 0x35:
			return keypad_divide;
		case 0x37:
			return print_screen;
		case 0x38:
			return right_alt;
		case 0x47:
			return home;
		case 0x48:
			return up;
		case 0x49:
			return page_up;
		case 0x4b:
			return left;
		case 0x4d:
			return right;
		case 0x4f:
			return end;
		case 0x50:
			return down;
		case 0x51:
			return page_down;
		case 0x52:
			return insert;
		case 0x53:
			return delete_forward;
		case 0x5b:
			return left_super;
		case 0x5c:
			return right_super;
		case 0x5d:
			return application;
		default:
			return unknown;
		}
	}
	static constexpr std::pair<unsigned, physical_key> keys[] = {{1, escape},
	                                                             {2, digit_1},
	                                                             {3, digit_2},
	                                                             {4, digit_3},
	                                                             {5, digit_4},
	                                                             {6, digit_5},
	                                                             {7, digit_6},
	                                                             {8, digit_7},
	                                                             {9, digit_8},
	                                                             {10, digit_9},
	                                                             {11, digit_0},
	                                                             {12, minus},
	                                                             {13, equal},
	                                                             {14, backspace},
	                                                             {15, tab},
	                                                             {16, q},
	                                                             {17, w},
	                                                             {18, e},
	                                                             {19, r},
	                                                             {20, t},
	                                                             {21, y},
	                                                             {22, u},
	                                                             {23, i},
	                                                             {24, o},
	                                                             {25, p},
	                                                             {26, left_bracket},
	                                                             {27, right_bracket},
	                                                             {28, enter},
	                                                             {29, left_control},
	                                                             {30, a},
	                                                             {31, s},
	                                                             {32, d},
	                                                             {33, f},
	                                                             {34, g},
	                                                             {35, h},
	                                                             {36, j},
	                                                             {37, k},
	                                                             {38, l},
	                                                             {39, semicolon},
	                                                             {40, apostrophe},
	                                                             {41, grave},
	                                                             {42, left_shift},
	                                                             {43, backslash},
	                                                             {44, z},
	                                                             {45, x},
	                                                             {46, c},
	                                                             {47, v},
	                                                             {48, b},
	                                                             {49, n},
	                                                             {50, m},
	                                                             {51, comma},
	                                                             {52, period},
	                                                             {53, slash},
	                                                             {54, right_shift},
	                                                             {55, keypad_multiply},
	                                                             {56, left_alt},
	                                                             {57, space},
	                                                             {58, caps_lock},
	                                                             {59, f1},
	                                                             {60, f2},
	                                                             {61, f3},
	                                                             {62, f4},
	                                                             {63, f5},
	                                                             {64, f6},
	                                                             {65, f7},
	                                                             {66, f8},
	                                                             {67, f9},
	                                                             {68, f10},
	                                                             {69, num_lock},
	                                                             {70, scroll_lock},
	                                                             {71, keypad_7},
	                                                             {72, keypad_8},
	                                                             {73, keypad_9},
	                                                             {74, keypad_minus},
	                                                             {75, keypad_4},
	                                                             {76, keypad_5},
	                                                             {77, keypad_6},
	                                                             {78, keypad_plus},
	                                                             {79, keypad_1},
	                                                             {80, keypad_2},
	                                                             {81, keypad_3},
	                                                             {82, keypad_0},
	                                                             {83, keypad_period},
	                                                             {86, non_us_backslash},
	                                                             {87, f11},
	                                                             {88, f12}};
	for (auto [native, key] : keys)
		if (native == scan)
			return key;
	return unknown;
}
inline physical_key x11_physical_key(std::string_view name) {
	using enum physical_key;
	static constexpr std::pair<std::string_view, physical_key> names[] = {
	    {"ESC", escape},
	    {"BKSP", backspace},
	    {"TAB", tab},
	    {"RTRN", enter},
	    {"SPCE", space},
	    {"TLDE", grave},
	    {"BKSL", backslash},
	    {"LSGT", non_us_backslash},
	    {"CAPS", caps_lock},
	    {"LFSH", left_shift},
	    {"RTSH", right_shift},
	    {"LCTL", left_control},
	    {"RCTL", right_control},
	    {"LALT", left_alt},
	    {"RALT", right_alt},
	    {"LWIN", left_super},
	    {"RWIN", right_super},
	    {"MENU", application},
	    {"COMP", application},
	    {"PRSC", print_screen},
	    {"SCLK", scroll_lock},
	    {"PAUS", pause},
	    {"INS", insert},
	    {"HOME", home},
	    {"PGUP", page_up},
	    {"DELE", delete_forward},
	    {"END", end},
	    {"PGDN", page_down},
	    {"LEFT", left},
	    {"RGHT", right},
	    {"UP", up},
	    {"DOWN", down},
	    {"NMLK", num_lock},
	    {"KPDV", keypad_divide},
	    {"KPMU", keypad_multiply},
	    {"KPSU", keypad_minus},
	    {"KPAD", keypad_plus},
	    {"KPEN", keypad_enter},
	    {"KPDL", keypad_period},
	    {"KPEQ", keypad_equal}};
	while (!name.empty() && name.back() == '\0')
		name.remove_suffix(1);
	for (auto [native, key] : names)
		if (native == name)
			return key;
	if (name.size() == 3 && name.starts_with("KP") && name[2] >= '0' && name[2] <= '9')
		return name[2] == '0' ? keypad_0
		                      : physical_key(unsigned(keypad_1) + unsigned(name[2] - '1'));
	if (name.size() != 4 || name[2] < '0' || name[2] > '9' || name[3] < '0' || name[3] > '9')
		return unknown;
	unsigned index = unsigned(name[2] - '0') * 10 + unsigned(name[3] - '0');
	constexpr std::array<physical_key, 12> ae{digit_1, digit_2, digit_3, digit_4, digit_5, digit_6,
	                                          digit_7, digit_8, digit_9, digit_0, minus,   equal};
	constexpr std::array<physical_key, 12> ad{
	    q, w, e, r, t, y, u, i, o, p, left_bracket, right_bracket};
	constexpr std::array<physical_key, 11> ac{a, s, d, f, g, h, j, k, l, semicolon, apostrophe};
	constexpr std::array<physical_key, 10> ab{z, x, c, v, b, n, m, comma, period, slash};
	if (index && name.starts_with("AE") && index <= ae.size())
		return ae[index - 1];
	if (index && name.starts_with("AD") && index <= ad.size())
		return ad[index - 1];
	if (index && name.starts_with("AC") && index <= ac.size())
		return ac[index - 1];
	if (index && name.starts_with("AB") && index <= ab.size())
		return ab[index - 1];
	if (index && name.starts_with("FK") && index <= 24)
		return index <= 12 ? physical_key(unsigned(f1) + index - 1)
		                   : physical_key(unsigned(f13) + index - 13);
	return unknown;
}
} // namespace ice::platform::detail
