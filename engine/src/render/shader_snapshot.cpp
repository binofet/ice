#include "ice/render/shader_snapshot.h"
#include "ice/core/utf8.h"
#include <array>
#include <bit>
#include <cstdint>
#include <utility>
#include <vector>

namespace ice::render {
namespace {
// SHA-256's specified compression constants and big-endian message layout. The source
// budget bounds padding arithmetic and all temporary storage; no external crypto runtime
// is needed to identify a source revision. This hash does not authenticate its provider.
std::string digest(std::string_view source) {
	static constexpr std::array<std::uint32_t, 64> constants{
	    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
	    0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
	    0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
	    0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
	    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
	    0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
	    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
	    0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
	    0xc67178f2};
	std::array<std::uint32_t, 8> state{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
	                                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
	std::vector<std::uint8_t>    padded(source.begin(), source.end());
	padded.push_back(0x80);
	while (padded.size() % 64 != 56)
		padded.push_back(0);
	const auto bits = std::uint64_t(source.size()) * 8;
	for (int shift = 56; shift >= 0; shift -= 8)
		padded.push_back(std::uint8_t(bits >> shift));
	for (std::size_t block = 0; block < padded.size(); block += 64) {
		std::array<std::uint32_t, 64> words{};
		for (std::size_t index = 0; index < 16; ++index)
			for (std::size_t byte = 0; byte < 4; ++byte)
				words[index] = (words[index] << 8) | padded[block + index * 4 + byte];
		for (std::size_t index = 16; index < words.size(); ++index) {
			const auto a = words[index - 15], b = words[index - 2];
			const auto small_a = std::rotr(a, 7) ^ std::rotr(a, 18) ^ (a >> 3);
			const auto small_b = std::rotr(b, 17) ^ std::rotr(b, 19) ^ (b >> 10);
			words[index]       = words[index - 16] + small_a + words[index - 7] + small_b;
		}
		auto work = state;
		for (std::size_t index = 0; index < words.size(); ++index) {
			const auto big_e =
			    std::rotr(work[4], 6) ^ std::rotr(work[4], 11) ^ std::rotr(work[4], 25);
			const auto choose = (work[4] & work[5]) ^ (~work[4] & work[6]);
			const auto first  = work[7] + big_e + choose + constants[index] + words[index];
			const auto big_a =
			    std::rotr(work[0], 2) ^ std::rotr(work[0], 13) ^ std::rotr(work[0], 22);
			const auto majority = (work[0] & work[1]) ^ (work[0] & work[2]) ^ (work[1] & work[2]);
			for (std::size_t move = 7; move > 0; --move)
				work[move] = work[move - 1];
			work[4] += first;
			work[0] = first + big_a + majority;
		}
		for (std::size_t index = 0; index < state.size(); ++index)
			state[index] += work[index];
	}
	constexpr char hex[] = "0123456789abcdef";
	std::string    result;
	result.reserve(64);
	for (auto word : state)
		for (int shift = 28; shift >= 0; shift -= 4)
			result.push_back(hex[(word >> shift) & 15]);
	return result;
}
} // namespace
shader_snapshot::shader_snapshot(std::string uri, std::string source, std::string sha256)
    : uri_(std::move(uri)), source_(std::move(source)), sha256_(std::move(sha256)) {}
ice::expected<shader_snapshot, diagnostic>
shader_snapshot::read(const ice::io::resource_resolver &resolver, const std::string &uri) {
	ice::io::resource_uri canonical;
	std::string           error;
	if (!ice::core::valid_utf8_text(uri, 4096) || uri.empty() ||
	    !resolver.canonical(uri, canonical, error))
		return ice::unexpected(diagnostic{"Invalid shader URI: " + error});
	auto name = canonical.string();
	if (name.size() > 4096)
		return ice::unexpected(diagnostic{"Canonical shader URI exceeds 4096 bytes"});
	std::vector<unsigned char> bytes;
	if (!resolver.read(name, bytes, error))
		return ice::unexpected(diagnostic{name + ": " + error});
	if (bytes.empty() || bytes.size() > 1024 * 1024)
		return ice::unexpected(diagnostic{name + ": shader source must contain 1..1048576 bytes"});
	std::string source(bytes.begin(), bytes.end());
	if (!ice::core::valid_utf8_text(source))
		return ice::unexpected(diagnostic{name + ": shader source must be UTF-8 without NUL"});
	auto hash = digest(source);
	return shader_snapshot(std::move(name), std::move(source), std::move(hash));
}
} // namespace ice::render
