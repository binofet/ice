#include "network.generated.h"
#include <algorithm>
#include <cfenv>
#include <cstdio>
#include <limits>

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::fprintf(stderr, "Network metadata failed at %d: %s\n", __LINE__, #condition);     \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

namespace {
using namespace ice::network_proof;
using transform = record_demo_transform;
using health    = record_demo_health;
using fire      = record_demo_fire_event;

int full_state_and_delta() {
	transform                 original{.125f, -.375f, 300.f}, decoded{9, 9, 9};
	std::vector<std::uint8_t> bytes;
	CHECK(encode(original, 7, bytes) == status::ok && bytes.size() == 83);
	const std::vector<std::uint8_t> payload{1, 0, 0,   0,   2, 0, 12, 0, 2, 0, 0,  0,
	                                        2, 0, 218, 255, 3, 0, 0,  0, 2, 0, 48, 117};
	CHECK(std::equal(payload.begin(), payload.end(), bytes.begin() + 59));
	CHECK(bytes[41] == 0 && bytes[49] == 7 && bytes[57] == 3);
	std::uint64_t revision = 999;
	CHECK(decode(bytes, decoded, revision) == status::ok && revision == 7);
	CHECK(decoded.f1 == .12f && decoded.f2 == -.38f && decoded.f3 == 300.f);
	CHECK(std::abs(original.f1 - decoded.f1) <= .00501f &&
	      std::abs(original.f2 - decoded.f2) <= .00501f);
	std::vector<std::uint8_t> empty;
	CHECK(encode(decoded, 8, empty, &decoded, 7) == status::ok && empty.size() == 59);
	transform next{};
	CHECK(decode(empty, next, revision) == status::baseline_mismatch);
	CHECK(decode(empty, next, revision, &decoded, 6) == status::baseline_mismatch);
	CHECK(decode(empty, next, revision, &decoded, 7) == status::ok && next == decoded &&
	      revision == 8);
	std::vector<std::uint8_t> full;
	CHECK(encode(decoded, 7, full) == status::ok);
	auto redundant = empty;
	redundant[57]  = 1;
	redundant.insert(redundant.end(), full.begin() + 59, full.begin() + 67);
	CHECK(decode(redundant, next, revision, &decoded, 7) == status::malformed);
	auto invalid_baseline = decoded;
	invalid_baseline.f1   = std::numeric_limits<float>::quiet_NaN();
	CHECK(decode(empty, next, revision, &invalid_baseline, 7) == status::invalid_value);
	auto changed = decoded;
	changed.f2   = 12.34f;
	CHECK(encode(changed, 9, bytes, &decoded, 7) == status::ok && bytes.size() == 67 &&
	      bytes[59] == 2);
	CHECK(decode(bytes, next, revision, &decoded, 7) == status::ok && next == changed &&
	      revision == 9);
	CHECK(encode(changed, 7, bytes, &decoded, 7) == status::invalid_value);
	CHECK(authorize_write<health>(false, 7, 7) == status::denied);
	CHECK(authorize_write<health>(true, 6, 7) == status::baseline_mismatch);
	CHECK(authorize_write<health>(true, 7, 7) == status::ok);
	CHECK(authorize_write<fire>(true, 7, 7) == status::denied);
	for (auto points : {0u, 1u, 255u, 256u, 65535u}) {
		health value{points}, out{};
		CHECK(encode(value, 1, bytes) == status::ok && bytes.size() == 67);
		CHECK(bytes[65] == (points & 255u) && bytes[66] == (points >> 8));
		CHECK(decode(bytes, out, revision) == status::ok && out == value);
	}
	const std::vector<std::uint8_t> sentinel{8, 9};
	bytes = sentinel;
	CHECK(encode(health{65536}, 1, bytes) == status::invalid_value && bytes == sentinel);
	CHECK(encode(transform{301, 0, 0}, 1, bytes) == status::invalid_value && bytes == sentinel);
	CHECK(encode(transform{std::numeric_limits<float>::quiet_NaN(), 0, 0}, 1, bytes) ==
	          status::invalid_value &&
	      bytes == sentinel);
	CHECK(encode(transform{0, 0, 0}, 0, bytes) == status::invalid_value && bytes == sentinel);
	return 0;
}

int quantization() {
	field_descriptor field{1, "coordinate", wire_codec::signed16_cm, 1, -328, 328, "m", "linear",
	                       ""};
	for (auto value : {.125f, .375f, -.125f, -.375f, -327.68f, 327.67f, 0.f, 1.23456f}) {
		std::vector<std::uint8_t> bytes;
		CHECK(detail::encode_value(field, value, bytes) && bytes.size() == 2);
		float decoded{};
		CHECK(detail::decode_value(field, bytes, decoded));
		CHECK(std::abs(decoded - value) <= .005f + .00004f);
		if (value == .125f)
			CHECK(bytes[0] == 12 && bytes[1] == 0);
		if (value == .375f)
			CHECK(bytes[0] == 38 && bytes[1] == 0);
		if (value == -.125f)
			CHECK(bytes[0] == 244 && bytes[1] == 255);
		if (value == -.375f)
			CHECK(bytes[0] == 218 && bytes[1] == 255);
		if (value == -327.68f)
			CHECK(bytes[0] == 0 && bytes[1] == 128);
		if (value == 327.67f)
			CHECK(bytes[0] == 255 && bytes[1] == 127);
	}
	for (auto value : {327.68f, -327.69f, std::numeric_limits<float>::infinity()}) {
		std::vector<std::uint8_t> bytes;
		CHECK(!detail::encode_value(field, value, bytes));
	}
	const int original_rounding = std::fegetround();
	for (const auto mode : {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
		CHECK(std::fesetround(mode) == 0);
		std::vector<std::uint8_t> bytes;
		CHECK(detail::encode_value(field, .125f, bytes) &&
		      bytes == std::vector<std::uint8_t>({12, 0}));
	}
	CHECK(std::fesetround(original_rounding) == 0);
	field.codec_version = 2;
	std::vector<std::uint8_t> bytes;
	CHECK(!detail::encode_value(field, 0.f, bytes));
	return 0;
}

int malformed_and_atomicity() {
	std::vector<std::uint8_t> valid;
	CHECK(encode(transform{1, 2, 3}, 4, valid) == status::ok);
	const transform unchanged{9, 8, 7};
	for (std::size_t size = 0; size < valid.size(); ++size) {
		auto          output   = unchanged;
		std::uint64_t revision = 101;
		CHECK(decode(std::span(valid).first(size), output, revision) != status::ok);
		CHECK(output == unchanged && revision == 101);
	}
	for (const auto offset : {0u, 4u, 5u, 37u, 41u, 57u, 59u, 63u, 67u, 71u}) {
		auto bad               = valid;
		bad[offset]            = 255;
		auto          output   = unchanged;
		std::uint64_t revision = 101;
		CHECK(decode(bad, output, revision) != status::ok && output == unchanged &&
		      revision == 101);
	}
	for (const auto size : {84u, 65537u}) {
		auto bad = valid;
		bad.resize(size);
		auto          output   = unchanged;
		std::uint64_t revision = 101;
		CHECK(decode(bad, output, revision) == status::malformed && output == unchanged &&
		      revision == 101);
	}
	auto duplicate         = valid;
	duplicate[67]          = 1;
	auto          output   = unchanged;
	std::uint64_t revision = 101;
	CHECK(decode(duplicate, output, revision) == status::malformed && output == unchanged);
	// Change only the final value to a validly framed but out-of-range coordinate.
	auto bad_range = valid;
	bad_range[81]  = 255;
	bad_range[82]  = 127;
	CHECK(decode(bad_range, output, revision) == status::malformed && output == unchanged &&
	      revision == 101);
	health wrong{};
	CHECK(decode(valid, wrong, revision) == status::schema_mismatch);
	return 0;
}

struct host final : event_host {
	entity_reference expected{{1}, 11, 42};
	bool             grants = true, owns = true, ready = true, publishes = true;
	unsigned         calls{};
	bool             owned_weapon(const entity_reference &weapon) const override {
        return owns && weapon == expected;
	}
	bool capability(std::string_view required) const override {
		return grants && required == "weapon.fire";
	}
	bool cooldown(const entity_reference &) const override { return ready; }
	bool dispatch(const entity_reference &, std::uint32_t, const std::array<float, 3> &) override {
		if (!publishes)
			return false;
		++calls;
		return true;
	}
};

int events() {
	host                      authority;
	sequence_window           window;
	fire                      value{authority.expected, 100, 0, 1, 0}, decoded{};
	std::vector<std::uint8_t> bytes;
	CHECK(encode(value, 1, bytes) == status::ok && bytes.size() == 137);
	CHECK(bytes[65] == 1 && bytes[81] == 11 && bytes[89] == 42 && bytes[103] == 100);
	std::uint64_t revision{};
	CHECK(decode(bytes, decoded, revision) == status::ok && decoded == value);
	auto valid_event = bytes;
	CHECK(encode(value, 2, bytes, &value, 1) == status::invalid_value && bytes == valid_event);
	auto event_delta = valid_event;
	event_delta[4]   = static_cast<std::uint8_t>(packet_kind::delta);
	CHECK(decode(event_delta, decoded, revision, &value, 1) == status::malformed);
	for (auto bits : {0x80000000u, 0x7fc00000u, 0x7f800000u}) {
		auto malformed = bytes;
		for (unsigned i = 0; i < 4; ++i)
			malformed[113 + i] = static_cast<std::uint8_t>(bits >> (i * 8));
		auto unchanged    = decoded;
		auto old_revision = revision;
		CHECK(decode(malformed, decoded, revision) == status::malformed && decoded == unchanged &&
		      revision == old_revision);
	}
	// No dispatch/sequence consumption on missing grant, wrong ownership, stale room/epoch,
	// cooldown, malformed aim, or failed publication. A corrected exact sequence may retry.
	authority.grants = false;
	CHECK(validate_and_dispatch(value, authority, window) == status::denied);
	authority.grants = true;
	authority.owns   = false;
	CHECK(validate_and_dispatch(value, authority, window) == status::denied);
	authority.owns = true;
	++value.f1.epoch;
	CHECK(validate_and_dispatch(value, authority, window) == status::denied);
	value.f1         = authority.expected;
	value.f1.room[0] = 2;
	CHECK(validate_and_dispatch(value, authority, window) == status::denied);
	value.f1        = authority.expected;
	authority.ready = false;
	CHECK(validate_and_dispatch(value, authority, window) == status::denied);
	authority.ready = true;
	value.f4        = 0;
	CHECK(validate_and_dispatch(value, authority, window) == status::invalid_value);
	value.f4            = 1;
	authority.publishes = false;
	CHECK(validate_and_dispatch(value, authority, window) == status::denied &&
	      authority.calls == 0);
	authority.publishes = true;
	CHECK(validate_and_dispatch(value, authority, window) == status::ok && authority.calls == 1);
	CHECK(validate_and_dispatch(value, authority, window) == status::duplicate &&
	      authority.calls == 1);
	value.f3 = 1;
	value.f4 = 0;
	CHECK(validate_and_dispatch(value, authority, window) == status::duplicate &&
	      authority.calls == 1);
	value.f2 = 37;
	CHECK(validate_and_dispatch(value, authority, window) == status::ok);
	value.f2 = 36;
	CHECK(validate_and_dispatch(value, authority, window) == status::stale_sequence);
	value.f2 = 164;
	CHECK(validate_and_dispatch(value, authority, window) == status::ok);
	value.f2 = 100;
	CHECK(validate_and_dispatch(value, authority, window) == status::stale_sequence);
	window.reset();
	value.f2 = std::numeric_limits<std::uint32_t>::max();
	CHECK(validate_and_dispatch(value, authority, window) == status::ok);
	value.f2 = 0;
	CHECK(validate_and_dispatch(value, authority, window) == status::stale_sequence);
	window.reset();
	CHECK(validate_and_dispatch(value, authority, window) == status::ok);
	value.f1.serial = 0;
	CHECK(encode(value, 1, bytes) == status::invalid_value);
	return 0;
}
} // namespace

int main(int argc, char **argv) {
	if (full_state_and_delta() || quantization() || malformed_and_atomicity() || events())
		return 1;
	CHECK(network_types.size() == 3);
	CHECK(record_demo_health_descriptor.fields[0].codec == wire_codec::uint16_exact);
	if (argc == 3 && std::string_view(argv[1]) == "--golden") {
		auto *file = std::fopen(argv[2], "wb");
		CHECK(file);
		std::vector<std::uint8_t> bytes;
		CHECK(encode(transform{.125f, -.375f, 300}, 7, bytes) == status::ok);
		CHECK(std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size());
		CHECK(encode(health{65535}, 8, bytes) == status::ok);
		CHECK(std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size());
		CHECK(encode(fire{entity_reference{{1}, 11, 42}, 100, 0, 1, 0}, 9, bytes) == status::ok);
		CHECK(std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size());
		CHECK(std::fclose(file) == 0);
	} else
		CHECK(argc == 1);
	std::puts("Network metadata passed: generated field traversal, full/delta bytes, atomic "
	          "rejection, host policy and replay boundaries");
	return 0;
}
