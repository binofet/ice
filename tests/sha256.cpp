#include "ice/core/sha256.h"
#include <cstdio>
#include <string>
#include <string_view>

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::fprintf(stderr, "SHA-256 check %d: %s\n", __LINE__, #condition);                  \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

int main() {
	using ice::core::sha256;
	// Published SHA-256 examples exercise empty input and one/two padding blocks.
	CHECK(sha256({}) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
	CHECK(sha256("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
	CHECK(sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
	      "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
	CHECK(sha256("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
	             "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu") ==
	      "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1");
	CHECK(sha256(std::string(1000000, 'a')) ==
	      "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
	// Padding boundaries and byte-oriented vectors were independently checked with
	// Python hashlib/OpenSSL. Views deliberately start unaligned and end before a
	// non-NUL suffix, so hashing the backing allocation instead of the view fails.
	struct vector {
		std::size_t      length;
		std::string_view expected;
	};
	constexpr vector boundaries[]{
	    {55, "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318"},
	    {56, "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a"},
	    {63, "7d3e74a05d7db15bce4ad9ec0658ea98e3f06eeecf16b4c6fff2da457ddc2f34"},
	    {64, "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb"},
	    {65, "635361c48bb9eab14198e76ea8ab7f1a41685d6ad62aa9146d301d4f17eb0ae0"}};
	for (const auto &test : boundaries) {
		const auto padded = "!" + std::string(test.length, 'a') + "not part of this digest";
		CHECK(sha256(std::string_view(padded).substr(1, test.length)) == test.expected);
	}
	std::string binary;
	for (unsigned byte = 0; byte < 256; ++byte)
		binary.push_back(static_cast<char>(byte));
	CHECK(sha256(binary) == "40aff2e9d2d8922e47afd4648e6967497158785fbd1da870e7110266bf944880");
	CHECK(sha256(std::string_view("a\0b\xff\x80", 5)) ==
	      "6601e324917d6d5bf6d5c55f1efd0fef503537e15625c284b15835d761b30038");
	std::puts(
	    "SHA-256 passed: published vectors, padding/block boundaries, binary and bounded views");
}
