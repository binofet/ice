#include "ice/data/json.h"
#include <bit>
#include <cstdio>
#include <limits>

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::fprintf(stderr, "JSON check failed at %d: %s\n", __LINE__, #condition);           \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

int main() {
	using ice::data::decode_json;
	using ice::data::encode_json;
	using ice::data::value;
	auto decoded = decode_json(
	    R"({"z":[null,true,false,-9223372036854775808,18446744073709551615,1.5,-0.0],"a":"猫 \uD83D\uDE00\n\\\""})");
	CHECK(decoded);
	auto encoded = encode_json(*decoded);
	CHECK(encoded && encoded->starts_with("{\"a\":"));
	auto roundtrip = decode_json(*encoded);
	CHECK(roundtrip && *roundtrip == *decoded);
	const auto &numbers =
	    std::get<value::sequence>(std::get<value::record>(decoded->data).at("z").data);
	CHECK(std::get<std::uint64_t>(numbers[4].data) == UINT64_MAX);
	CHECK(std::get<std::int64_t>(numbers[3].data) == INT64_MIN);
	CHECK(std::bit_cast<std::uint64_t>(std::get<double>(numbers[6].data)) ==
	      (std::uint64_t(1) << 63));
	for (auto invalid : {"",
	                     " ",
	                     "[1,]",
	                     "{\"a\":1,}",
	                     "{\"a\":1,\"\\u0061\":2}",
	                     "01",
	                     "-01",
	                     "+1",
	                     ".1",
	                     "1.",
	                     "1e",
	                     "1e+",
	                     "1e9999",
	                     "1e-9999",
	                     "18446744073709551616",
	                     "-9223372036854775809",
	                     "true false",
	                     "NaN",
	                     "Infinity",
	                     "\"\\u0000\"",
	                     "\"\\uD800\"",
	                     "\"\\uDC00\"",
	                     "\"\\uD800\\u1234\"",
	                     "\"\\uGGGG\"",
	                     "\"\\x01\"",
	                     "\"\n\"",
	                     "/*comment*/null"})
		CHECK(!decode_json(invalid));
	CHECK(!decode_json(std::string("\"\xC0\x80\"")));
	CHECK(!decode_json(std::string("\"\xED\xA0\x80\"")));
	CHECK(!decode_json(std::string("\"x\0y\"", 5)));
	CHECK(!decode_json(std::string(1024 * 1024 + 1, ' ')));
	CHECK(!decode_json("\"" + std::string(4097, 'x') + "\""));
	CHECK(decode_json("\"" + std::string(4096, 'x') + "\""));
	CHECK(!decode_json(std::string(50, '[') + "0" + std::string(50, ']')));
	std::string array = "[";
	for (int i = 0; i < 4097; ++i)
		array += i ? ",0" : "0";
	CHECK(!decode_json(array + "]"));
	CHECK(!encode_json(value{std::numeric_limits<double>::infinity()}));
	CHECK(!encode_json(value{ice::data::object_reference{"file:///x", "x", "type"}}));
	value::sequence large;
	for (int i = 0; i < 200; ++i)
		large.emplace_back(std::string(4096, '\n'));
	CHECK(!encode_json(value{std::move(large)}));
	// Exercise owned map movement while nested JSON arrays grow on older libc++.
	for (unsigned count : {1u, 2u, 3u, 16u, 129u}) {
		std::string input = "[";
		for (unsigned i = 0; i < count; ++i)
			input += i ? ",{\"key\":[\"value\",{}]}" : "{\"key\":[\"value\",{}]}";
		auto tree = decode_json(input + "]");
		CHECK(tree);
		auto text = encode_json(*tree);
		CHECK(text && decode_json(*text));
	}
	CHECK(decode_json(
	    " \r\n\t {\"quote\":\"\\\"\",\"solidus\":\"\\/\",\"ctrl\":\"\\b\\f\\r\\t\"} \n"));
}
