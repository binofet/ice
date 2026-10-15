#ifndef ICE_COMPANION_OVERLAY_H
#define ICE_COMPANION_OVERLAY_H
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
// A small procedural diagnostic font; no packaged assets or renderer dependency.
struct CompanionOverlay {
	std::uint32_t *pixels;
	int            width_, height_, stride, scale;
	void           rect(int x, int y, int w, int h, std::uint32_t color_) {
        for (int row = std::max(0, y); row < std::min(height_, y + h); ++row)
            for (int col = std::max(0, x); col < std::min(width_, x + w); ++col)
                pixels[row * stride + col] = color_;
	}
	static void glyph(char c, unsigned char *rows) {
		const char         *chars     = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-./:%+";
		const unsigned char data[][7] = {{14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30},
		                                 {14, 17, 16, 16, 16, 17, 14}, {30, 17, 17, 17, 17, 17, 30},
		                                 {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16},
		                                 {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17},
		                                 {31, 4, 4, 4, 4, 4, 31},      {7, 2, 2, 2, 18, 18, 12},
		                                 {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
		                                 {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17},
		                                 {14, 17, 17, 17, 17, 17, 14}, {30, 17, 17, 30, 16, 16, 16},
		                                 {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17},
		                                 {15, 16, 16, 14, 1, 1, 30},   {31, 4, 4, 4, 4, 4, 4},
		                                 {17, 17, 17, 17, 17, 17, 14}, {17, 17, 17, 17, 17, 10, 4},
		                                 {17, 17, 17, 21, 21, 21, 10}, {17, 17, 10, 4, 10, 17, 17},
		                                 {17, 17, 10, 4, 4, 4, 4},     {31, 1, 2, 4, 8, 16, 31},
		                                 {14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14},
		                                 {14, 17, 1, 2, 4, 8, 31},     {30, 1, 1, 14, 1, 1, 30},
		                                 {2, 6, 10, 18, 31, 2, 2},     {31, 16, 16, 30, 1, 1, 30},
		                                 {14, 16, 16, 30, 17, 17, 14}, {31, 1, 2, 4, 8, 8, 8},
		                                 {14, 17, 17, 14, 17, 17, 14}, {14, 17, 17, 15, 1, 1, 14},
		                                 {0, 0, 0, 31, 0, 0, 0},       {0, 0, 0, 0, 0, 12, 12},
		                                 {1, 2, 2, 4, 8, 8, 16},       {0, 4, 4, 0, 4, 4, 0},
		                                 {17, 2, 4, 8, 17, 0, 0},      {0, 4, 4, 31, 4, 4, 0}};
		const char         *found     = std::strchr(chars, std::toupper((unsigned char)c));
		if (found)
			std::memcpy(rows, data[found - chars], 7);
		else
			std::memset(rows, 0, 7);
	}
	void text(int x, int y, const char *message, std::uint32_t color_ = 0xffffffffu) {
		int left = x;
		for (const char *c = message; *c; ++c) {
			if (*c == '\n' || x + 6 * scale > width_ - 12) {
				y += 10 * scale;
				x = left;
				if (*c == '\n')
					continue;
			}
			unsigned char rows[7];
			glyph(*c, rows);
			for (int r = 0; r < 7; ++r)
				for (int col = 0; col < 5; ++col)
					if (rows[r] & (1 << (4 - col)))
						rect(x + col * scale, y + r * scale, scale, scale, color_);
			x += 6 * scale;
		}
	}
	void button(float left, float top, float w, float h, const char *label, bool selected = false) {
		int x = left * width_, y = top * height_;
		rect(x, y, w * width_ - 6, h * height_, selected ? 0xffa08028 : 0xff4b3c2c);
		text(x + 12, y + (int)(h * height_ - 7 * scale) / 2, label);
	}
};
#endif
