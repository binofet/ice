#include "ice/meta/schema.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <set>
#include <utility>

namespace ice::meta {
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);
namespace {
auto failure(std::string message, field_id field = 0) {
	return ice::unexpected(diagnostic{std::move(message), field});
}
bool valid_text(const std::string &text, std::size_t limit) {
	// Reject malformed UTF-8, including surrogate encodings, overlong forms and NUL.
	if (text.size() > limit)
		return false;
	for (std::size_t i = 0; i < text.size();) {
		auto c = static_cast<unsigned char>(text[i++]);
		if (c == 0)
			return false;
		if (c < 0x80)
			continue;
		unsigned count = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC2 ? 1 : 0;
		if (!count || c > 0xF4 || i + count > text.size())
			return false;
		std::uint32_t code = c & ((1u << (6 - count)) - 1);
		for (unsigned j = 0; j < count; ++j) {
			auto next = static_cast<unsigned char>(text[i++]);
			if ((next & 0xC0) != 0x80)
				return false;
			code = (code << 6) | (next & 0x3F);
		}
		if (code < (count == 1   ? 0x80u
		            : count == 2 ? 0x800u
		                         : 0x10000u) ||
		    code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF))
			return false;
	}
	return true;
}
} // namespace
const field_descriptor *find_field(const schema_descriptor &schema, field_id id) {
	auto found = std::ranges::find(schema.fields, id, &field_descriptor::id);
	return found == schema.fields.end() ? nullptr : &*found;
}
ice::expected<value, diagnostic> validate_value(const field_descriptor &field, const value &input) {
	if (std::holds_alternative<std::monostate>(input)) {
		if (field.nullable)
			return input;
		return failure("Null is not permitted", field.id);
	}
	value                 normalized = input;
	std::optional<double> number;
	switch (field.kind) {
	case scalar_kind::boolean:
		if (!std::holds_alternative<bool>(input))
			return failure("Expected boolean", field.id);
		break;
	case scalar_kind::int32: {
		auto p = std::get_if<std::int64_t>(&input);
		if (!p || *p < INT32_MIN || *p > INT32_MAX)
			return failure("Expected int32", field.id);
		number = static_cast<double>(*p);
		break;
	}
	case scalar_kind::uint32: {
		auto p = std::get_if<std::uint64_t>(&input);
		if (!p || *p > UINT32_MAX)
			return failure("Expected uint32", field.id);
		number = static_cast<double>(*p);
		break;
	}
	case scalar_kind::float32:
	case scalar_kind::float64: {
		auto p = std::get_if<double>(&input);
		if (!p || !std::isfinite(*p))
			return failure("Expected finite floating point", field.id);
		double result = *p;
		if (field.kind == scalar_kind::float32) {
			if (std::abs(result) > std::numeric_limits<float>::max())
				return failure("float32 overflow", field.id);
			result = static_cast<float>(result);
		}
		number     = result;
		normalized = result;
		break;
	}
	case scalar_kind::string: {
		auto p = std::get_if<std::string>(&input);
		if (!p || !valid_text(*p, 4096))
			return failure("Expected bounded UTF-8 text", field.id);
		break;
	}
	default:
		return failure("Unknown scalar kind", field.id);
	}
	if (number && ((field.minimum && *number < *field.minimum) ||
	               (field.maximum && *number > *field.maximum)))
		return failure("Value violates numeric constraints", field.id);
	return normalized;
}
ice::expected<void, diagnostic> validate_definition(const schema_descriptor &schema) {
	if (!valid_text(schema.id, 128) || schema.id.empty() || !schema.version ||
	    !valid_text(schema.name, 128) || schema.name.empty() || schema.fields.empty() ||
	    schema.fields.size() > 128)
		return failure("Invalid schema identity or field budget");
	std::set<field_id>    ids;
	std::set<std::string> names;
	for (const auto &field : schema.fields) {
		if (!field.id || !ids.insert(field.id).second || field.name.empty() ||
		    !valid_text(field.name, 128) || !names.insert(field.name).second ||
		    static_cast<std::uint8_t>(field.kind) > static_cast<std::uint8_t>(scalar_kind::string))
			return failure("Invalid/duplicate field identity or representation", field.id);
		if ((field.minimum && !std::isfinite(*field.minimum)) ||
		    (field.maximum && !std::isfinite(*field.maximum)) ||
		    (field.minimum && field.maximum && *field.minimum > *field.maximum) ||
		    ((field.kind == scalar_kind::boolean || field.kind == scalar_kind::string) &&
		     (field.minimum || field.maximum)))
			return failure("Invalid numeric constraints", field.id);
		if (field.has_default) {
			auto result = validate_value(field, field.default_value);
			if (!result)
				return ice::unexpected(result.error());
		}
	}
	return {};
}
ice::expected<record, diagnostic> validate(const schema_descriptor &schema, const record &input) {
	auto definition = validate_definition(schema);
	if (!definition)
		return ice::unexpected(definition.error());
	if (input.size() > 128)
		return failure("Record exceeds field budget");
	for (const auto &[id, ignored] : input)
		if (!find_field(schema, id))
			return failure("Unknown field ID", id);
	record result;
	for (const auto &field : schema.fields) {
		auto found = input.find(field.id);
		if (found == input.end() && !field.has_default)
			return failure("Missing required field", field.id);
		auto checked =
		    validate_value(field, found == input.end() ? field.default_value : found->second);
		if (!checked)
			return ice::unexpected(checked.error());
		result.emplace(field.id, std::move(*checked));
	}
	return result;
}
namespace {
void put(std::vector<std::byte> &bytes, std::uint64_t value, unsigned count) {
	for (unsigned i = 0; i < count; ++i)
		bytes.push_back(static_cast<std::byte>((value >> (i * 8)) & 255));
}
struct reader {
	std::span<const std::byte> bytes;
	std::size_t                position = 0;
	bool                       get(std::uint64_t &value, unsigned count) {
        if (position > bytes.size() || count > bytes.size() - position)
            return false;
        value = 0;
        for (unsigned i = 0; i < count; ++i)
            value |= std::uint64_t(std::to_integer<unsigned char>(bytes[position++])) << (i * 8);
        return true;
	}
	bool text(std::string &value) {
		std::uint64_t size;
		if (!get(size, 4) || size > 4096 || size > bytes.size() - position)
			return false;
		value.assign(reinterpret_cast<const char *>(bytes.data() + position),
		             static_cast<std::size_t>(size));
		position += static_cast<std::size_t>(size);
		return valid_text(value, 4096);
	}
};
void put_text(std::vector<std::byte> &bytes, const std::string &text) {
	put(bytes, text.size(), 4);
	for (unsigned char c : text)
		bytes.push_back(static_cast<std::byte>(c));
}
} // namespace
ice::expected<std::vector<std::byte>, diagnostic> serialize(const schema_descriptor &schema,
                                                            const record            &input) {
	auto checked = validate(schema, input);
	if (!checked)
		return ice::unexpected(checked.error());
	std::vector<std::byte> bytes;
	put(bytes, 0x314D4349, 4); // ICM1, little endian. This test format is not a shipped asset ABI.
	put_text(bytes, schema.id);
	put(bytes, schema.version, 4);
	put(bytes, checked->size(), 4);
	for (const auto &[id, field_value] : *checked) {
		const auto &field = *find_field(schema, id);
		put(bytes, id, 4);
		put(bytes, static_cast<std::uint8_t>(field.kind), 1);
		bool present = !std::holds_alternative<std::monostate>(field_value);
		put(bytes, present, 1);
		if (!present)
			continue;
		switch (field.kind) {
		case scalar_kind::boolean:
			put(bytes, std::get<bool>(field_value), 1);
			break;
		case scalar_kind::int32:
			put(bytes, static_cast<std::uint32_t>(std::get<std::int64_t>(field_value)), 4);
			break;
		case scalar_kind::uint32:
			put(bytes, std::get<std::uint64_t>(field_value), 4);
			break;
		case scalar_kind::float32:
			put(bytes,
			    std::bit_cast<std::uint32_t>(static_cast<float>(std::get<double>(field_value))), 4);
			break;
		case scalar_kind::float64:
			put(bytes, std::bit_cast<std::uint64_t>(std::get<double>(field_value)), 8);
			break;
		case scalar_kind::string:
			put_text(bytes, std::get<std::string>(field_value));
			break;
		}
	}
	return bytes;
}
ice::expected<record, diagnostic> deserialize(const schema_descriptor   &schema,
                                              std::span<const std::byte> bytes) {
	if (bytes.size() > 1024 * 1024)
		return failure("Document exceeds one MiB budget");
	auto definition = validate_definition(schema);
	if (!definition)
		return ice::unexpected(definition.error());
	reader        source{bytes};
	std::uint64_t magic, version, count;
	std::string   type;
	if (!source.get(magic, 4) || magic != 0x314D4349 || !source.text(type) || type != schema.id ||
	    !source.get(version, 4) || version != schema.version || !source.get(count, 4) ||
	    count > 128)
		return failure("Invalid document header/schema version");
	record result;
	for (std::uint64_t i = 0; i < count; ++i) {
		std::uint64_t id, kind, present, raw;
		if (!source.get(id, 4) || !source.get(kind, 1) || !source.get(present, 1) || present > 1)
			return failure("Truncated/invalid field header");
		auto field = find_field(schema, static_cast<field_id>(id));
		if (!field || kind != static_cast<std::uint8_t>(field->kind) ||
		    result.contains(static_cast<field_id>(id)))
			return failure("Unknown, duplicate or mistyped field", static_cast<field_id>(id));
		value decoded;
		if (present) {
			if (field->kind == scalar_kind::string) {
				std::string text;
				if (!source.text(text))
					return failure("Invalid string payload", field->id);
				decoded = std::move(text);
			} else {
				unsigned width = field->kind == scalar_kind::boolean   ? 1
				                 : field->kind == scalar_kind::float64 ? 8
				                                                       : 4;
				if (!source.get(raw, width))
					return failure("Truncated scalar payload", field->id);
				switch (field->kind) {
				case scalar_kind::boolean:
					if (raw > 1)
						return failure("Invalid boolean payload", field->id);
					decoded = bool(raw);
					break;
				case scalar_kind::int32:
					decoded =
					    std::int64_t(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(raw)));
					break;
				case scalar_kind::uint32:
					decoded = raw;
					break;
				case scalar_kind::float32:
					decoded = double(std::bit_cast<float>(static_cast<std::uint32_t>(raw)));
					break;
				case scalar_kind::float64:
					decoded = std::bit_cast<double>(raw);
					break;
				case scalar_kind::string:
					break;
				}
			}
		}
		result.emplace(field->id, std::move(decoded));
	}
	if (source.position != bytes.size())
		return failure("Trailing document bytes");
	return validate(schema, result);
}
ice::expected<record, diagnostic> migrate(const schema_descriptor &previous,
                                          const schema_descriptor &next, const record &input,
                                          std::span<const field_id> removed_fields) {
	if (previous.id != next.id || previous.version == UINT32_MAX ||
	    next.version != previous.version + 1)
		return failure("Migration requires the same stable type and exactly one version step");
	auto old = validate(previous, input);
	if (!old)
		return ice::unexpected(old.error());
	auto definition = validate_definition(next);
	if (!definition)
		return ice::unexpected(definition.error());
	std::set<field_id> removals;
	for (auto id : removed_fields)
		if (!find_field(previous, id) || find_field(next, id) || !removals.insert(id).second)
			return failure("Invalid/duplicate acknowledged removal", id);
	record result;
	for (const auto &field : previous.fields) {
		auto replacement = find_field(next, field.id);
		if (!replacement) {
			if (!removals.contains(field.id))
				return failure("Removal requires explicit acknowledgement", field.id);
			continue;
		}
		if (field.kind != replacement->kind || field.nullable != replacement->nullable)
			return failure("Representation change requires a custom migration", field.id);
		result.emplace(field.id, old->at(field.id));
	}
	return validate(next, result);
}
object_store::object_store(schema_descriptor schema) : schema_(std::move(schema)) {}
ice::expected<std::size_t, diagnostic> object_store::resolve(object_handle handle) const {
	if (!handle.slot || handle.slot > slots_.size())
		return failure("Unknown object handle");
	const auto &entry = slots_[handle.slot - 1];
	if (!entry.alive || entry.generation != handle.generation)
		return failure("Stale object handle");
	return handle.slot - 1;
}
ice::expected<object_handle, diagnostic> object_store::create(const record &initial) {
	auto checked = validate(schema_, initial);
	if (!checked)
		return ice::unexpected(checked.error());
	std::size_t index = 0;
	while (index < slots_.size() && (slots_[index].alive || slots_[index].generation == UINT64_MAX))
		++index;
	if (index == slots_.size()) {
		if (slots_.size() == 64)
			return failure("Object budget exhausted");
		slots_.emplace_back();
	}
	auto &entry    = slots_[index];
	entry.fields   = std::move(*checked);
	entry.revision = 1;
	entry.alive    = true;
	entry.history.clear();
	return object_handle{static_cast<std::uint32_t>(index + 1), entry.generation};
}
ice::expected<void, diagnostic> object_store::erase(object_handle handle) {
	auto index = resolve(handle);
	if (!index)
		return ice::unexpected(index.error());
	auto &entry = slots_[*index];
	entry.alive = false;
	entry.fields.clear();
	entry.history.clear();
	++entry.generation;
	return {};
}
ice::expected<object_snapshot, diagnostic> object_store::inspect(object_handle handle) const {
	auto index = resolve(handle);
	if (!index)
		return ice::unexpected(index.error());
	const auto &entry = slots_[*index];
	return object_snapshot{entry.revision, entry.fields};
}
ice::expected<std::uint64_t, diagnostic> object_store::apply(object_handle handle,
                                                             std::uint64_t expected_revision,
                                                             std::span<const field_edit> edits) {
	auto index = resolve(handle);
	if (!index)
		return ice::unexpected(index.error());
	auto &entry = slots_[*index];
	if (expected_revision != entry.revision || entry.revision == UINT64_MAX)
		return failure("Stale/exhausted object revision");
	if (edits.empty() || edits.size() > 128)
		return failure("Edit batch must contain one to 128 changes");
	record             candidate = entry.fields;
	std::set<field_id> ids;
	for (const auto &edit : edits) {
		if (!find_field(schema_, edit.id) || !ids.insert(edit.id).second)
			return failure("Unknown/duplicate edit field", edit.id);
		if (edit.replacement)
			candidate[edit.id] = *edit.replacement;
		else
			candidate.erase(edit.id);
	}
	auto checked = validate(schema_, candidate);
	if (!checked)
		return ice::unexpected(checked.error());
	// Finish all potentially allocating preparation before the no-throw record swap.
	entry.history.push_back(entry.fields);
	if (entry.history.size() > 32)
		entry.history.erase(entry.history.begin());
	entry.fields.swap(*checked);
	return ++entry.revision;
}
ice::expected<std::uint64_t, diagnostic> object_store::undo(object_handle handle,
                                                            std::uint64_t expected_revision) {
	auto index = resolve(handle);
	if (!index)
		return ice::unexpected(index.error());
	auto &entry = slots_[*index];
	if (entry.revision != expected_revision || entry.revision == UINT64_MAX)
		return failure("Stale/exhausted object revision");
	if (entry.history.empty())
		return failure("No undo history");
	entry.fields.swap(entry.history.back());
	entry.history.pop_back();
	return ++entry.revision;
}
} // namespace ice::meta
