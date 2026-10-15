#pragma once
#include <array>
#include <string>
namespace ice::util {
/// Small, deliberately bounded encoder for ASCII LAN join URLs.
/// QR version 3, error correction L, byte mode, mask 0. Four-module quiet zone
/// belongs to the renderer. No URL is sent to an external QR service.
struct join_qr {
	/// QR module count along each side; quiet-zone pixels are added by the renderer.
	static constexpr int size = 29;
	/// Row-major module grid: true draws a dark module, false draws the background.
	std::array<std::array<bool, size>, size> dark{};
	/// Encode the supplied text as an owned QR symbol; false rejects unsupported size/content.
	bool encode(const std::string &text); // max 53 bytes; clears on failure
};

} // namespace ice::util
