#ifndef IC_JOIN_QR_H
#define IC_JOIN_QR_H
#include <array>
#include <string>
// Small, deliberately bounded encoder for ASCII LAN join URLs.
// QR version 3, error correction L, byte mode, mask 0. Four-module quiet zone
// belongs to the renderer. No URL is sent to an external QR service.
struct icJoinQR {
    static constexpr int Size=29;
    std::array<std::array<bool,Size>,Size> dark{};
    bool Encode(const std::string& text); // max 53 bytes; clears on failure
};
#endif
