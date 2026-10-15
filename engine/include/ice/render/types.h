#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace ice::render {
/// Recoverable shader, device, surface or submission failure.
struct diagnostic {
	/// URI-qualified compiler output or native failure explanation.
	std::string message;
};
/// Owned RGBA8 capture, top row first, from an explicit offscreen render/readback.
struct image {
	/// Pixel dimensions of the owned image.
	int width = 0, height = 0;
	/// Four bytes per pixel, tightly packed in RGBA order.
	std::vector<std::uint8_t> rgba;
};
/// Submission outcome; unavailable/busy leave the previous presented image intact.
enum class frame_status { submitted, unavailable, busy };
} // namespace ice::render
