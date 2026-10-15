#include "internal.h"

namespace ice::data {
namespace {
kind read_kind(const std::string &name) {
	static const std::map<std::string, kind, std::less<>> kinds = {
	    {"boolean", kind::boolean}, {"int32", kind::int32},      {"uint32", kind::uint32},
	    {"float32", kind::float32}, {"float64", kind::float64},  {"string", kind::string},
	    {"record", kind::record},   {"enum", kind::enumeration}, {"sequence", kind::sequence},
	    {"array", kind::array},     {"map", kind::map},          {"set", kind::set}};
	auto result = kinds.find(name);
	if (result == kinds.end())
		detail::fail("Unknown logical type kind", name);
	return result->second;
}
type_expression read_expression(const value &input, unsigned depth = 0) {
	using namespace detail;
	if (depth > 32)
		fail("Type-expression nesting budget exceeded");
	const auto     &raw = as<value::record>(input);
	type_expression result;
	result.type     = read_kind(text(required(raw, "kind")));
	result.nullable = as<bool>(required(raw, "nullable"));
	if (result.type == kind::record || result.type == kind::enumeration) {
		properties(raw, {"kind", "nullable", "id"});
		result.identity = text(required(raw, "id"), 128);
	} else if (result.type >= kind::sequence) {
		properties(raw, {"kind", "nullable", "arguments"});
		const auto &arguments = as<value::sequence>(required(raw, "arguments"));
		if (arguments.size() != (result.type == kind::map ? 2u : 1u))
			fail("Container argument arity mismatch");
		for (const auto &argument : arguments)
			result.arguments.push_back(read_expression(argument, depth + 1));
		if ((result.type == kind::map || result.type == kind::set) &&
		    !canonical_key(result.arguments[0]))
			fail("This profile requires canonical scalar map keys/set elements");
	} else {
		properties(raw, {"kind", "nullable"});
	}
	return result;
}
void check_expression(const type_expression &expression, const catalog &definitions) {
	if (expression.type == kind::record || expression.type == kind::enumeration) {
		auto target = definitions.find(expression.identity);
		if (!target || target->enumeration != (expression.type == kind::enumeration))
			detail::fail("Unknown/mismatched named type", expression.identity);
	}
	for (const auto &argument : expression.arguments)
		check_expression(argument, definitions);
}
bool has_record(const type_expression &expression) {
	return expression.type == kind::record || std::ranges::any_of(expression.arguments, has_record);
}
} // namespace
std::span<const type_schema> catalog::types() const { return types_; }
const type_schema           *catalog::find(const std::string &identity) const {
    auto result = std::ranges::find(types_, identity, &type_schema::identity);
    return result == types_.end() ? nullptr : &*result;
}
ice::expected<catalog, diagnostic> catalog::read(const value &projection) {
	using namespace detail;
	try {
		auto checked = validate_tree(projection);
		if (!checked)
			throw invalid{checked.error()};
		const auto &types = as<value::sequence>(projection);
		if (types.empty() || types.size() > 128)
			fail("Expected one to 128 named types");
		catalog               result;
		std::set<std::string> identities, names;
		for (const auto &type_value : types) {
			const auto &raw = as<value::record>(type_value);
			properties(raw, {"id", "version", "name", "kind", "prototype", "fields", "symbols"});
			type_schema type;
			type.identity = text(required(raw, "id"), 128);
			type.name     = text(required(raw, "name"), 128);
			if (!identifier(type.identity, true) || !identities.insert(type.identity).second ||
			    !identifier(type.name) || !names.insert(type.name).second)
				fail("Invalid/duplicate type identity or name", type.identity);
			type.version   = nonzero_id(required(raw, "version"));
			auto type_kind = text(required(raw, "kind"));
			if (type_kind != "record" && type_kind != "enum")
				fail("Named type must be a record or enum", type.identity);
			type.enumeration    = type_kind == "enum";
			type.prototype      = as<bool>(required(raw, "prototype"));
			const auto &fields  = as<value::sequence>(required(raw, "fields"));
			const auto &symbols = as<value::sequence>(required(raw, "symbols"));
			if (fields.size() > 128 || symbols.size() > 128 ||
			    (type.enumeration ? (!fields.empty() || symbols.empty() || type.prototype)
			                      : !symbols.empty()))
				fail("Inconsistent record/enum shape or member budget", type.identity);
			std::set<std::uint32_t> member_ids;
			std::set<std::string>   member_names;
			auto                    member_identity = [&](const value::record &member) {
                auto id   = nonzero_id(required(member, "id"));
                auto name = text(required(member, "name"), 128);
                if (!identifier(name) || !member_ids.insert(id).second ||
                    !member_names.insert(name).second)
                    fail("Invalid/duplicate member identity", type.identity + "." + name);
                return std::pair{id, std::move(name)};
			};
			std::set<std::int32_t> numbers;
			for (const auto &symbol_value : symbols) {
				const auto &symbol = as<value::record>(symbol_value);
				properties(symbol, {"id", "name", "value"});
				auto [id, name] = member_identity(symbol);
				auto number     = signed_integer(required(symbol, "value"));
				if (number < INT32_MIN || number > INT32_MAX ||
				    !numbers.insert(static_cast<std::int32_t>(number)).second)
					fail("Invalid enum value; aliases need an explicit future policy",
					     type.identity);
				type.symbols.push_back({id, std::move(name), static_cast<std::int32_t>(number)});
			}
			for (const auto &field_value : fields) {
				const auto &raw_field = as<value::record>(field_value);
				properties(raw_field, {"id", "name", "type", "storage", "merge", "key",
				                       "has_default", "default"});
				auto [id, name] = member_identity(raw_field);
				field_schema field;
				field.id     = id;
				field.name   = std::move(name);
				field.type   = read_expression(required(raw_field, "type"));
				auto storage = text(required(raw_field, "storage"));
				if (storage == "inline")
					field.storage = reference_policy::inline_value;
				else if (storage == "local")
					field.storage = reference_policy::local;
				else if (storage == "external")
					field.storage = reference_policy::external;
				else if (storage == "any")
					field.storage = reference_policy::any;
				else
					fail("Unknown object occurrence policy", type.identity + "." + field.name);
				auto merge = text(required(raw_field, "merge"));
				if (merge == "replace")
					field.merge = merge_policy::replace;
				else if (merge == "record")
					field.merge = merge_policy::record;
				else if (merge == "keyed")
					field.merge = merge_policy::keyed;
				else if (merge == "map")
					field.merge = merge_policy::map;
				else if (merge == "set")
					field.merge = merge_policy::set;
				else
					fail("Unknown container merge policy", type.identity + "." + field.name);
				field.key = text(required(raw_field, "key"), 128);
				if (as<bool>(required(raw_field, "has_default")))
					field.default_value = required(raw_field, "default");
				else if (!null(required(raw_field, "default")))
					fail("An absent default cannot carry a value");
				type.fields.push_back(std::move(field));
			}
			std::ranges::sort(type.fields, {}, &field_schema::id);
			std::ranges::sort(type.symbols, {}, &enum_symbol::id);
			result.types_.push_back(std::move(type));
		}
		std::ranges::sort(result.types_, {}, &type_schema::identity);
		for (const auto &type : result.types_) {
			for (const auto &field : type.fields) {
				check_expression(field.type, result);
				if (field.storage != reference_policy::inline_value && !has_record(field.type))
					fail("Reference policy requires object leaves",
					     type.identity + "." + field.name);
				bool merge_ok =
				    field.merge == merge_policy::replace ||
				    (field.merge == merge_policy::record && field.type.type == kind::record) ||
				    (field.merge == merge_policy::keyed && field.type.type == kind::sequence) ||
				    (field.merge == merge_policy::map && field.type.type == kind::map) ||
				    (field.merge == merge_policy::set && field.type.type == kind::set);
				if (!merge_ok || (field.merge != merge_policy::keyed && !field.key.empty()))
					fail("Merge policy/type/key mismatch", type.identity + "." + field.name);
				if (field.merge == merge_policy::keyed) {
					const auto &element = field.type.arguments[0];
					if (element.type != kind::record || element.nullable ||
					    field.storage != reference_policy::inline_value)
						fail("Keyed sequences require nonnullable inline record elements");
					auto key = detail::field(*result.find(element.identity), field.key);
					if (!key || !canonical_key(key->type) ||
					    key->storage != reference_policy::inline_value)
						fail("Keyed sequence needs a stable scalar identity field", field.key);
				}
			}
		}
		validate_defaults(result);
		return result;
	} catch (const invalid &error) {
		return ice::unexpected(error.error);
	}
}
} // namespace ice::data
