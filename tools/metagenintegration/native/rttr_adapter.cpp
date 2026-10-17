#include "rttr_adapter.h"
#include <algorithm>
#include <array>

namespace ice::examples::rttr_bridge {
namespace {
template <class type> std::optional<type> read(const rttr::variant &value) {
	if (!value.is_type<type>())
		return std::nullopt;
	return value.get_value<type>();
}
rttr::type native_type(meta::scalar_kind kind, bool nullable) {
	switch (kind) {
	case meta::scalar_kind::boolean:
		return nullable ? rttr::type::get<std::optional<bool>>() : rttr::type::get<bool>();
	case meta::scalar_kind::int32:
		return nullable ? rttr::type::get<std::optional<std::int32_t>>()
		                : rttr::type::get<std::int32_t>();
	case meta::scalar_kind::uint32:
		return nullable ? rttr::type::get<std::optional<std::uint32_t>>()
		                : rttr::type::get<std::uint32_t>();
	case meta::scalar_kind::float32:
		return nullable ? rttr::type::get<std::optional<float>>() : rttr::type::get<float>();
	case meta::scalar_kind::float64:
		return nullable ? rttr::type::get<std::optional<double>>() : rttr::type::get<double>();
	case meta::scalar_kind::string:
		return nullable ? rttr::type::get<std::optional<std::string>>()
		                : rttr::type::get<std::string>();
	}
	return rttr::type::get<void>();
}
} // namespace
ice::expected<meta::schema_descriptor, meta::diagnostic> describe(rttr::type type) {
	auto id          = read<std::string>(type.get_metadata("metagen.type_id"));
	auto native_name = read<std::string>(type.get_metadata("metagen.native_name"));
	auto type_tags   = read<std::map<std::string, std::string>>(type.get_metadata("metagen.tags"));
	auto version     = read<std::uint32_t>(type.get_metadata("metagen.version"));
	if (!id || !version || !native_name || !type_tags)
		return ice::unexpected(meta::diagnostic{"RTTR type lacks portable schema identity"});
	meta::schema_descriptor result;
	result.id      = *id;
	result.version = *version;
	result.name    = *native_name;
	result.tags    = *type_tags;
	constexpr std::array names{"boolean", "int32", "uint32", "float32", "float64", "string"};
	for (const auto &property : type.get_properties()) {
		auto field_tags =
		    read<std::map<std::string, std::string>>(property.get_metadata("metagen.tags"));
		auto field_id    = read<std::uint32_t>(property.get_metadata("metagen.field_id"));
		auto kind        = read<std::string>(property.get_metadata("metagen.kind"));
		auto nullable    = read<bool>(property.get_metadata("metagen.nullable"));
		auto has_default = read<bool>(property.get_metadata("metagen.has_default"));
		if (!field_id || !kind || !nullable || !has_default || !field_tags)
			return ice::unexpected(meta::diagnostic{"RTTR property lacks typed portable metadata"});
		meta::field_descriptor field;
		field.id          = *field_id;
		field.name        = property.get_name().to_string();
		field.nullable    = *nullable;
		field.has_default = *has_default;
		field.tags        = *field_tags;
		auto found        = std::ranges::find(names, *kind);
		if (found == names.end())
			return ice::unexpected(meta::diagnostic{"Unsupported RTTR scalar kind", field.id});
		field.kind = static_cast<meta::scalar_kind>(found - names.begin());
		if (property.get_type() != native_type(field.kind, field.nullable))
			return ice::unexpected(
			    meta::diagnostic{"RTTR C++ property type disagrees with schema", field.id});
		if (field.has_default) {
			auto value = read<meta::value>(property.get_metadata("metagen.default"));
			if (!value)
				return ice::unexpected(
				    meta::diagnostic{"RTTR literal default is missing or mistyped", field.id});
			field.default_value = *value;
		}
		for (const auto &[key, target] : {std::pair{"metagen.minimum", &field.minimum},
		                                  std::pair{"metagen.maximum", &field.maximum}}) {
			auto value = property.get_metadata(std::string(key));
			if (value.is_valid()) {
				auto number = read<double>(value);
				if (!number)
					return ice::unexpected(
					    meta::diagnostic{"RTTR numeric bound is mistyped", field.id});
				*target = *number;
			}
		}
		result.fields.push_back(std::move(field));
	}
	auto checked = meta::validate_definition(result);
	if (!checked)
		return ice::unexpected(checked.error());
	return result;
}
} // namespace ice::examples::rttr_bridge
