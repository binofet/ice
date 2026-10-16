#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

/// Experimental ICE-owned scalar network codec; no sockets or authenticated room are provided.
namespace ice::network_proof {
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559,
              "The scalar wire profile requires IEEE-754 binary32");
/// A live reference binds room, authority epoch and non-reused spawn serial.
struct entity_reference {
	std::array<std::uint8_t, 16> room{};
	std::uint64_t                epoch{};
	std::uint64_t                serial{};
	bool                         operator==(const entity_reference &) const = default;
};
/// Registered scalar codecs, all version 1 in this proof profile.
enum class wire_codec { signed16_cm, uint16_exact, uint32_exact, float32_finite, entity_v1 };
/// Stable semantic field data generated from raw Metagen declarations.
struct field_descriptor {
	std::uint32_t    id{};
	std::string_view name;
	wire_codec       codec{};
	std::uint32_t    codec_version{1};
	double           minimum{}, maximum{};
	std::string_view unit, interpolation, reference;
};
/// A fingerprint covers semantic policy and stable IDs; presentation names are excluded.
struct type_descriptor {
	std::string_view             id, name;
	std::uint32_t                version{};
	std::array<std::uint8_t, 32> fingerprint{};
	std::string_view writer, audience, delivery, conflict, direction, capability, deduplication;
	std::span<const field_descriptor> fields;
};
/// Distinct framing, validation and authority failures remain inspectable to consumers.
enum class status {
	ok,
	invalid_value,
	malformed,
	schema_mismatch,
	baseline_mismatch,
	denied,
	duplicate,
	stale_sequence
};
/// Full snapshots contain every field; deltas require an exact acknowledged baseline revision.
enum class packet_kind : std::uint8_t { full = 1, delta = 2 };

namespace detail {
template <class U> void write(std::vector<std::uint8_t> &out, U value) {
	static_assert(std::is_unsigned_v<U>);
	for (std::size_t i = 0; i < sizeof(U); ++i)
		out.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
}
template <class U> bool read(std::span<const std::uint8_t> &in, U &value) {
	static_assert(std::is_unsigned_v<U>);
	if (in.size() < sizeof(U))
		return false;
	value = 0;
	for (std::size_t i = 0; i < sizeof(U); ++i)
		value |= static_cast<U>(in[i]) << (i * 8);
	in = in.subspan(sizeof(U));
	return true;
}
inline bool valid_reference(const entity_reference &value) {
	bool nonzero = false;
	for (auto byte : value.room)
		nonzero |= byte != 0;
	return nonzero && value.epoch != 0 && value.serial != 0;
}
// Explicit nearest-even avoids dependence on the host floating-point rounding mode.
inline double nearest_even(double value) {
	const double lo = std::floor(value), fraction = value - lo;
	return lo +
	       ((fraction > .5 || (fraction == .5 && std::fmod(std::abs(lo), 2.) == 1.)) ? 1. : 0.);
}
template <class T>
bool encode_value(const field_descriptor &field, const T &value, std::vector<std::uint8_t> &out) {
	if (field.codec_version != 1)
		return false;
	if constexpr (std::is_same_v<T, float>) {
		if (!std::isfinite(value) || static_cast<double>(value) < field.minimum ||
		    static_cast<double>(value) > field.maximum)
			return false;
		if (field.codec == wire_codec::signed16_cm) {
			const double code = nearest_even(static_cast<double>(value) * 100.);
			if (code < -32768. || code > 32767.)
				return false;
			write(out, static_cast<std::uint16_t>(static_cast<std::int16_t>(code)));
		} else if (field.codec == wire_codec::float32_finite) {
			write(out, std::bit_cast<std::uint32_t>(value == 0 ? 0.f : value));
		} else
			return false;
	} else if constexpr (std::is_same_v<T, std::uint32_t>) {
		if (static_cast<double>(value) < field.minimum ||
		    static_cast<double>(value) > field.maximum)
			return false;
		if (field.codec == wire_codec::uint16_exact && value <= 65535)
			write(out, static_cast<std::uint16_t>(value));
		else if (field.codec == wire_codec::uint32_exact)
			write(out, value);
		else
			return false;
	} else if constexpr (std::is_same_v<T, entity_reference>) {
		if (field.codec != wire_codec::entity_v1 || !valid_reference(value))
			return false;
		out.insert(out.end(), value.room.begin(), value.room.end());
		write(out, value.epoch);
		write(out, value.serial);
	} else
		return false;
	return true;
}
template <class T>
bool decode_value(const field_descriptor &field, std::span<const std::uint8_t> in, T &value) {
	if (field.codec_version != 1)
		return false;
	if constexpr (std::is_same_v<T, float>) {
		if (field.codec == wire_codec::signed16_cm) {
			std::uint16_t bits{};
			if (!read(in, bits))
				return false;
			const auto code = bits <= 32767 ? static_cast<std::int32_t>(bits)
			                                : static_cast<std::int32_t>(bits) - 65536;
			value           = static_cast<float>(code) / 100.f;
		} else if (field.codec == wire_codec::float32_finite) {
			std::uint32_t bits{};
			if (!read(in, bits) || bits == 0x80000000u)
				return false;
			value = std::bit_cast<float>(bits);
		} else
			return false;
		if (!std::isfinite(value) || static_cast<double>(value) < field.minimum ||
		    static_cast<double>(value) > field.maximum)
			return false;
	} else if constexpr (std::is_same_v<T, std::uint32_t>) {
		if (field.codec == wire_codec::uint16_exact) {
			std::uint16_t small{};
			if (!read(in, small))
				return false;
			value = small;
		} else if (field.codec == wire_codec::uint32_exact) {
			if (!read(in, value))
				return false;
		} else
			return false;
		if (static_cast<double>(value) < field.minimum ||
		    static_cast<double>(value) > field.maximum)
			return false;
	} else if constexpr (std::is_same_v<T, entity_reference>) {
		if (field.codec != wire_codec::entity_v1 || in.size() < 32)
			return false;
		for (std::size_t i = 0; i < value.room.size(); ++i)
			value.room[i] = in[i];
		in = in.subspan(16);
		if (!read(in, value.epoch) || !read(in, value.serial) || !valid_reference(value))
			return false;
	} else
		return false;
	return in.empty();
}
struct encoded_field {
	std::uint32_t             id;
	std::vector<std::uint8_t> bytes;
};
template <class T> bool collect(const T &value, std::vector<encoded_field> &fields) {
	return visit_fields(value, [&](const field_descriptor &field, const auto &member) {
		encoded_field item{field.id, {}};
		if (!encode_value(field, member, item.bytes))
			return false;
		fields.push_back(std::move(item));
		return true;
	});
}
} // namespace detail

/// Produce a bounded canonical packet; failure preserves the caller's output bytes.
template <class T>
status encode(const T &value, std::uint64_t revision, std::vector<std::uint8_t> &out,
              const T *baseline = nullptr, std::uint64_t baseline_revision = 0) {
	const auto &type = descriptor(static_cast<const T *>(nullptr));
	if (!type.direction.empty() && baseline)
		return status::invalid_value;
	if (revision == 0 || (baseline && (baseline_revision == 0 || revision <= baseline_revision)) ||
	    (!baseline && baseline_revision))
		return status::invalid_value;
	std::vector<detail::encoded_field> fields, previous;
	if (!detail::collect(value, fields) || (baseline && !detail::collect(*baseline, previous)))
		return status::invalid_value;
	std::vector<std::uint8_t> candidate{'I', 'N', 'W', '1'};
	candidate.push_back(
	    static_cast<std::uint8_t>(baseline ? packet_kind::delta : packet_kind::full));
	candidate.insert(candidate.end(), type.fingerprint.begin(), type.fingerprint.end());
	detail::write(candidate, type.version);
	detail::write(candidate, baseline_revision);
	detail::write(candidate, revision);
	std::uint16_t count{};
	for (std::size_t i = 0; i < fields.size(); ++i)
		if (!baseline || fields[i].bytes != previous[i].bytes)
			++count;
	detail::write(candidate, count);
	for (std::size_t i = 0; i < fields.size(); ++i) {
		if (baseline && fields[i].bytes == previous[i].bytes)
			continue;
		detail::write(candidate, fields[i].id);
		detail::write(candidate, static_cast<std::uint16_t>(fields[i].bytes.size()));
		candidate.insert(candidate.end(), fields[i].bytes.begin(), fields[i].bytes.end());
	}
	if (candidate.size() > 65536)
		return status::invalid_value;
	out = std::move(candidate);
	return status::ok;
}

/// Decode against the negotiated schema and optional exact baseline; no partial record is
/// published.
template <class T>
status decode(std::span<const std::uint8_t> input, T &out, std::uint64_t &revision,
              const T *baseline = nullptr, std::uint64_t baseline_revision = 0) {
	const auto &type = descriptor(static_cast<const T *>(nullptr));
	if (input.size() < 59 || input.size() > 65536 || input[0] != 'I' || input[1] != 'N' ||
	    input[2] != 'W' || input[3] != '1')
		return status::malformed;
	const auto kind = static_cast<packet_kind>(input[4]);
	if (kind != packet_kind::full && kind != packet_kind::delta)
		return status::malformed;
	if (!type.direction.empty() && kind == packet_kind::delta)
		return status::malformed;
	for (std::size_t i = 0; i < 32; ++i)
		if (input[5 + i] != type.fingerprint[i])
			return status::schema_mismatch;
	input = input.subspan(37);
	std::uint32_t version{};
	std::uint64_t base{}, next{};
	std::uint16_t count{};
	if (!detail::read(input, version) || !detail::read(input, base) || !detail::read(input, next) ||
	    !detail::read(input, count))
		return status::malformed;
	if (version != type.version)
		return status::schema_mismatch;
	if (!next || (kind == packet_kind::full && base) ||
	    (kind == packet_kind::delta && (!base || next <= base)))
		return status::malformed;
	if (kind == packet_kind::delta && (!baseline || base != baseline_revision))
		return status::baseline_mismatch;
	std::vector<detail::encoded_field> baseline_fields;
	if (kind == packet_kind::delta) {
		if (!detail::collect(*baseline, baseline_fields))
			return status::invalid_value;
	}
	if (count > type.fields.size() || (kind == packet_kind::full && count != type.fields.size()))
		return status::malformed;
	T             candidate = kind == packet_kind::delta ? *baseline : T{};
	std::uint32_t previous{};
	for (std::uint16_t i = 0; i < count; ++i) {
		std::uint32_t id{};
		std::uint16_t size{};
		if (!detail::read(input, id) || !detail::read(input, size) || id <= previous ||
		    input.size() < size)
			return status::malformed;
		previous   = id;
		bool found = false, valid = false;
		visit_fields(candidate, [&](const field_descriptor &field, auto &member) {
			if (field.id == id) {
				found = true;
				valid = detail::decode_value(field, input.first(size), member);
			}
			return true;
		});
		if (!found || !valid)
			return status::malformed;
		if (kind == packet_kind::delta) {
			const auto old = std::find_if(baseline_fields.begin(), baseline_fields.end(),
			                              [&](const auto &field) { return field.id == id; });
			if (old == baseline_fields.end() || std::equal(old->bytes.begin(), old->bytes.end(),
			                                               input.begin(), input.begin() + size))
				return status::malformed;
		}
		input = input.subspan(size);
	}
	if (!input.empty())
		return status::malformed;
	out      = std::move(candidate);
	revision = next;
	return status::ok;
}

/// Authority-only component mutation hook; caller identity must come from a trusted host.
template <class T>
status authorize_write(bool caller_is_authority, std::uint64_t expected_revision,
                       std::uint64_t actual_revision) {
	const auto &type = descriptor(static_cast<const T *>(nullptr));
	if (!type.direction.empty() || type.writer != "authority" || !caller_is_authority)
		return status::denied;
	return expected_revision == actual_revision ? status::ok : status::baseline_mismatch;
}

/// A 64-sequence replay window scoped by the host to one authenticated principal and authority
/// epoch. No uint32 wrap is accepted; rotate the authenticated input epoch before rollover.
class sequence_window {
public:
	/// Start a new authenticated stream. An epoch transition must be authorized outside this codec.
	void reset() {
		initialized_ = false;
		highest_     = 0;
		bits_        = 0;
	}
	/// Check without consuming a sequence; rejected gameplay requests leave the window unchanged.
	status check(std::uint32_t sequence) const {
		if (!initialized_ || sequence > highest_)
			return status::ok;
		const auto distance = highest_ - sequence;
		if (distance >= 64)
			return status::stale_sequence;
		return (bits_ & (std::uint64_t{1} << distance)) ? status::duplicate : status::ok;
	}
	/// Record an already validated request; call on the simulation owner thread after acceptance.
	void accept(std::uint32_t sequence) {
		if (!initialized_) {
			initialized_ = true;
			highest_     = sequence;
			bits_        = 1;
		} else if (sequence > highest_) {
			const auto advance = sequence - highest_;
			bits_              = advance >= 64 ? 1 : (bits_ << advance) | 1;
			highest_           = sequence;
		} else if (highest_ - sequence < 64)
			bits_ |= std::uint64_t{1} << (highest_ - sequence);
	}

private:
	bool          initialized_{};
	std::uint32_t highest_{};
	std::uint64_t bits_{};
};

/// Host-implemented checks, invoked outside transport/physics callbacks on the authority thread.
/// The host binds this object and replay window to authenticated principal/room/epoch, never
/// payload identity.
struct event_host {
	virtual ~event_host() = default;
	/// Resolve room/epoch/serial, current weapon type and ownership for the bound principal.
	virtual bool owned_weapon(const entity_reference &weapon) const = 0;
	/// Check the current grants of the bound principal.
	virtual bool capability(std::string_view required) const = 0;
	/// Check the current authority tick's cooldown without side effects.
	virtual bool cooldown(const entity_reference &weapon) const = 0;
	/// Publish the accepted fire action, or fail without any effect. Only success consumes its
	/// sequence.
	virtual bool dispatch(const entity_reference &weapon, std::uint32_t sequence,
	                      const std::array<float, 3> &aim) = 0;
};

/// Generic implementation called by the generated event adapter after field validation.
inline status dispatch_fire(const entity_reference &weapon, std::uint32_t sequence,
                            const std::array<float, 3> &aim, std::string_view required,
                            event_host &host, sequence_window &window) {
	if (!detail::valid_reference(weapon))
		return status::invalid_value;
	double length = 0;
	for (auto value : aim) {
		if (!std::isfinite(value) || value < -1 || value > 1)
			return status::invalid_value;
		length += static_cast<double>(value) * value;
	}
	if (std::abs(length - 1.) > .001)
		return status::invalid_value;
	if (!host.capability(required) || !host.owned_weapon(weapon) || !host.cooldown(weapon))
		return status::denied;
	const auto replay = window.check(sequence);
	if (replay != status::ok)
		return replay;
	if (!host.dispatch(weapon, sequence, aim))
		return status::denied;
	window.accept(sequence);
	return status::ok;
}
} // namespace ice::network_proof
