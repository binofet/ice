#pragma once
#include "ice/data/document.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <string_view>

namespace ice::data::detail {
// Private checked exception keeps recursive parsing/evaluation readable. Public entry points
// translate only these validation failures; allocator/user-reader exceptions retain their contract.
struct invalid {
	diagnostic error;
};
[[noreturn]] inline void fail(std::string message, std::string path = {}) {
	throw invalid{{std::move(message), std::move(path)}};
}
template <typename T> const T &as(const value &input, std::string_view path = {}) {
	auto result = std::get_if<T>(&input.data);
	if (!result)
		fail("Unexpected portable value kind", std::string(path));
	return *result;
}
inline const value *find(const value::record &input, std::string_view name) {
	auto result = input.find(name);
	return result == input.end() ? nullptr : &result->second;
}
inline const value &required(const value::record &input, std::string_view name) {
	auto result = find(input, name);
	if (!result)
		fail("Missing required property", std::string(name));
	return *result;
}
inline bool null(const value &input) { return std::holds_alternative<std::monostate>(input.data); }
inline std::uint64_t unsigned_integer(const value &input) {
	if (auto number = std::get_if<std::uint64_t>(&input.data))
		return *number;
	if (auto number = std::get_if<std::int64_t>(&input.data); number && *number >= 0)
		return static_cast<std::uint64_t>(*number);
	fail("Expected a nonnegative integer");
}
inline std::int64_t signed_integer(const value &input) {
	if (auto number = std::get_if<std::int64_t>(&input.data))
		return *number;
	if (auto number = std::get_if<std::uint64_t>(&input.data); number && *number <= INT64_MAX)
		return static_cast<std::int64_t>(*number);
	fail("Expected a signed-range integer");
}
inline std::uint32_t nonzero_id(const value &input) {
	auto number = unsigned_integer(input);
	if (number == 0 || number > UINT32_MAX)
		fail("Expected a nonzero uint32 identity/version");
	return static_cast<std::uint32_t>(number);
}
inline void properties(const value::record &input, std::initializer_list<std::string_view> names) {
	if (input.size() != names.size())
		fail("Unknown or missing properties");
	for (auto name : names)
		(void)required(input, name);
}
bool                      valid_text(std::string_view input, std::size_t limit = 4096);
inline const std::string &text(const value &input, std::size_t limit = 4096) {
	const auto &result = as<std::string>(input);
	if (!valid_text(result, limit))
		fail("Invalid UTF-8/NUL or text byte budget exceeded");
	return result;
}
inline bool identifier(std::string_view input, bool type_id = false) {
	if (input.empty() || input.front() < 'a' || input.front() > 'z')
		return false;
	return std::ranges::all_of(input, [type_id](char c) {
		return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
		       (type_id && c == '.');
	});
}
inline bool canonical_key(const type_expression &type) {
	return !type.nullable && (type.type == kind::string || type.type == kind::int32 ||
	                          type.type == kind::uint32 || type.type == kind::enumeration);
}
inline const field_schema *field(const type_schema &type, const std::string &name) {
	auto result = std::ranges::find(type.fields, name, &field_schema::name);
	return result == type.fields.end() ? nullptr : &*result;
}
value normalize_scalar(kind type, const value &input);
// Canonical ordering agrees with the tooling profile's ASCII JSON scalar-key spelling.
std::string key_token(const value &input);
void        validate_defaults(const catalog &definitions);
graph_data  build_graph(const catalog &definitions, const std::string &uri, const value &document,
                        document_reader reader);
} // namespace ice::data::detail
