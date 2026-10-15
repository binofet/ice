#include "ice/io/resource_uri.h"
#include "internal.h"

namespace ice::data::detail {
value normalize_scalar(kind type, const value &input) {
	if (type == kind::boolean)
		return value{as<bool>(input)};
	if (type == kind::string)
		return value{text(input)};
	if (type == kind::int32) {
		auto number = signed_integer(input);
		if (number < INT32_MIN || number > INT32_MAX)
			fail("int32 range mismatch");
		return value{number};
	}
	if (type == kind::uint32) {
		auto number = unsigned_integer(input);
		if (number > UINT32_MAX)
			fail("uint32 range mismatch");
		return value{number};
	}
	double number;
	if (auto real = std::get_if<double>(&input.data))
		number = *real;
	else if (auto integer = std::get_if<std::int64_t>(&input.data))
		number = static_cast<double>(*integer);
	else if (auto integer = std::get_if<std::uint64_t>(&input.data))
		number = static_cast<double>(*integer);
	else
		fail("Expected a numeric value; booleans are not numbers");
	if (type == kind::float32 &&
	    (!std::isfinite(number) || std::abs(number) > std::numeric_limits<float>::max()))
		fail("float32 overflow");
	if (type == kind::float32)
		number = static_cast<double>(static_cast<float>(number));
	if (!std::isfinite(number))
		fail("Nonfinite/overflowing floating-point value");
	return value{number};
}
std::string key_token(const value &input) {
	if (auto number = std::get_if<std::int64_t>(&input.data))
		return std::to_string(*number);
	if (auto number = std::get_if<std::uint64_t>(&input.data))
		return std::to_string(*number);
	const auto           &string  = text(input);
	std::string           output  = "\"";
	static constexpr char hex[]   = "0123456789abcdef";
	auto                  escaped = [&](std::uint32_t point) {
        output += "\\u";
        for (int shift = 12; shift >= 0; shift -= 4)
            output += hex[(point >> shift) & 15];
	};
	for (std::size_t i = 0; i < string.size();) {
		auto first = static_cast<unsigned char>(string[i++]);
		if (first == '"' || first == '\\') {
			output += '\\';
			output += char(first);
		} else if (first == '\b')
			output += "\\b";
		else if (first == '\f')
			output += "\\f";
		else if (first == '\n')
			output += "\\n";
		else if (first == '\r')
			output += "\\r";
		else if (first == '\t')
			output += "\\t";
		else if (first < 32 || first == 127)
			escaped(first);
		else if (first < 128)
			output += char(first);
		else {
			unsigned      count = first >= 0xF0 ? 3 : first >= 0xE0 ? 2 : 1;
			std::uint32_t point = first & ((1u << (6 - count)) - 1);
			for (unsigned j = 0; j < count; ++j)
				point = (point << 6) | (static_cast<unsigned char>(string[i++]) & 63);
			if (point > 0xFFFF) {
				point -= 0x10000;
				escaped(0xD800 + (point >> 10));
				escaped(0xDC00 + (point & 1023));
			} else
				escaped(point);
		}
	}
	return output + '"';
}
namespace {
struct copy_budget {
	std::size_t nodes = 262144;
	std::size_t bytes = 8 * 1024 * 1024;
	void        charge(const value &input, unsigned depth = 0) {
        if (nodes == 0 || bytes < 8 || depth > 48)
            fail("Aggregate graph/copy budget exceeded");
        --nodes;
        bytes -= 8;
        auto string = [&](const std::string &s) {
            if (s.size() > bytes)
                fail("Aggregate graph byte budget exceeded");
            bytes -= s.size();
        };
        if (auto s = std::get_if<std::string>(&input.data))
            string(*s);
        else if (auto ref = std::get_if<object_reference>(&input.data)) {
            string(ref->document);
            string(ref->identity);
            string(ref->type);
        } else if (auto list = std::get_if<value::sequence>(&input.data)) {
            for (const auto &item : *list)
                charge(item, depth + 1);
        } else if (auto object = std::get_if<value::record>(&input.data)) {
            for (const auto &[name, item] : *object) {
                charge(value{name}, depth + 1);
                charge(item, depth + 1);
            }
        }
	}
};
class graph_builder {
public:
	graph_builder(const catalog &definitions, document_reader reader)
	    : definitions_(definitions), reader_(std::move(reader)) {}
	graph_data load(const std::string &uri, const value &document) {
		io::resource_uri canonical;
		std::string      error;
		if (!io::resource_uri::parse(uri, canonical, error) || canonical.scheme.empty() ||
		    canonical.has_fragment)
			fail("Expected an absolute document URI without a fragment: " + error, uri);
		const auto origin = canonical.document();
		add_document(origin, document);
		result_.root =
		    reference(required(as<value::record>(result_.authored.at(origin)), "root"), origin, 0);
		std::set<std::string> checked_roots;
		while (result_.objects.size() < index_.size() ||
		       checked_roots.size() < result_.authored.size()) {
			// std::map keeps references/iterators valid as resolving a reference discovers a
			// document.
			for (const auto &[source, authored] : result_.authored) {
				if (checked_roots.insert(source).second)
					(void)reference(required(as<value::record>(authored), "root"), source, 0);
			}
			for (const auto &[address, definition] : index_) {
				auto type = text(required(as<value::record>(*definition), "$type"), 128);
				(void)object({address.first, address.second, type}, 0);
			}
		}
		return std::move(result_);
	}
	void defaults() {
		for (const auto &type : definitions_.types()) {
			for (const auto &field : type.fields) {
				if (!field.default_value)
					continue;
				default_controls(*field.default_value);
				(void)normalize(field.type, *field.default_value, "memory://defaults/",
				                field.storage, 0);
			}
		}
	}

private:
	const catalog                          &definitions_;
	document_reader                         reader_;
	graph_data                              result_;
	std::map<object_address, const value *> index_;
	std::set<object_address>                active_;
	std::size_t                             work_ = 0;
	copy_budget                             authored_budget_, effective_budget_, copy_budget_;
	void                                    spend(unsigned depth) {
        if (depth > 48 || ++work_ > 262144)
            fail("Graph depth/evaluation budget exceeded");
	}
	value clone(const value &input) {
		copy_budget_.charge(input);
		return input;
	}
	void default_controls(const value &input) {
		if (auto object = std::get_if<value::record>(&input.data)) {
			for (const auto &[name, child] : *object) {
				if (name.starts_with('$') && name != "$type")
					fail("Defaults cannot contain identity/reference/edit control operations");
				default_controls(child);
			}
		} else if (auto list = std::get_if<value::sequence>(&input.data)) {
			for (const auto &child : *list)
				default_controls(child);
		}
	}
	void dependencies(const type_expression &type, const value::record &versions) {
		if ((type.type == kind::record || type.type == kind::enumeration) &&
		    !versions.contains(type.identity))
			fail("Missing version for a named type dependency", type.identity);
		for (const auto &argument : type.arguments)
			dependencies(argument, versions);
	}
	void add_document(const std::string &uri, const value &input) {
		if (result_.authored.size() >= 16)
			fail("External document budget exceeded", uri);
		auto checked = validate_tree(input);
		if (!checked)
			throw invalid{checked.error()};
		authored_budget_.charge(input);
		const auto &raw = as<value::record>(input);
		properties(raw, {"schemas", "objects", "root"});
		const auto &versions = as<value::record>(required(raw, "schemas"));
		for (const auto &[identity, version] : versions) {
			auto type = definitions_.find(identity);
			if (!type || unsigned_integer(version) != type->version)
				fail("Document schema version mismatch; migrate explicitly", uri + "#" + identity);
			for (const auto &field : type->fields)
				dependencies(field.type, versions);
		}
		// Own the complete tree before indexing: pointers never point into caller-owned storage.
		auto [stored, inserted] = result_.authored.emplace(uri, input);
		if (!inserted)
			fail("Duplicate resolved document identity", uri);
		const auto &owned = as<value::record>(stored->second);
		for (const auto &entry : as<value::sequence>(required(owned, "objects"))) {
			if (!as<value::record>(entry).contains("$id"))
				fail("Object table entries require identities", uri);
			index(entry, uri, versions, 0);
		}
	}
	void index(const value &input, const std::string &uri, const value::record &versions,
	           unsigned depth) {
		spend(depth);
		if (auto object = std::get_if<value::record>(&input.data)) {
			if (auto id = find(*object, "$id")) {
				auto identity = text(*id, 128);
				auto type_id  = text(required(*object, "$type"), 128);
				auto type     = definitions_.find(type_id);
				if (identity.empty() || !versions.contains(type_id) || !type || type->enumeration ||
				    !index_.emplace(object_address{uri, identity}, &input).second)
					fail("Empty/duplicate identity or unknown object type", uri + "#" + identity);
			}
			for (const auto &[name, child] : *object) {
				(void)name;
				index(child, uri, versions, depth + 1);
			}
		} else if (auto list = std::get_if<value::sequence>(&input.data)) {
			for (const auto &child : *list)
				index(child, uri, versions, depth + 1);
		}
	}
	object_reference reference(const value &input, const std::string &uri, unsigned depth) {
		spend(depth);
		const auto &raw    = as<value::record>(input);
		std::string source = uri, identity;
		if (raw.size() == 1 && raw.contains("$ref")) {
			identity = text(raw.at("$ref"), 128);
		} else if (raw.size() == 1 && raw.contains("$uri")) {
			io::resource_uri base, target;
			std::string      error;
			if (!io::resource_uri::parse(uri, base, error) ||
			    !io::resource_uri::resolve(base, text(raw.at("$uri")), target, error) ||
			    !target.has_fragment)
				fail("Invalid external object URI: " + error, uri);
			source = target.document();
			// URI parsing retains escaped reserved bytes; object IDs use decoded UTF-8 identities.
			for (std::size_t i = 0; i < target.fragment.size(); ++i) {
				char c = target.fragment[i];
				if (c == '%') {
					auto hex = [](char digit) {
						return digit <= '9'   ? digit - '0'
						       : digit <= 'F' ? digit - 'A' + 10
						                      : digit - 'a' + 10;
					};
					// resource_uri validated and normalized every percent escape before this point.
					c = static_cast<char>((hex(target.fragment[i + 1]) << 4) |
					                      hex(target.fragment[i + 2]));
					i += 2;
				}
				identity += c;
			}
			if (!valid_text(identity, 128))
				fail("Invalid external object identity", source);
			if (!result_.authored.contains(source)) {
				if (!reader_)
					fail("No reader for external document", source);
				auto document = reader_(source);
				if (!document)
					throw invalid{document.error()};
				add_document(source, *document);
			}
		} else
			fail("Ambiguous or malformed object reference", uri);
		auto target = index_.find({source, identity});
		if (identity.empty() || target == index_.end())
			fail("Unresolved object identity", source + "#" + identity);
		return {source, identity, text(required(as<value::record>(*target->second), "$type"), 128)};
	}
	value normalize(const type_expression &type, const value &input, const std::string &uri,
	                reference_policy storage, unsigned depth) {
		spend(depth);
		if (null(input)) {
			if (!type.nullable)
				fail("Null is not permitted");
			return {};
		}
		if (type.type <= kind::string)
			return normalize_scalar(type.type, input);
		if (type.type == kind::enumeration) {
			const auto &symbols = definitions_.find(type.identity)->symbols;
			if (auto name = std::get_if<std::string>(&input.data)) {
				auto symbol = std::ranges::find(symbols, *name, &enum_symbol::name);
				if (symbol != symbols.end())
					return value{std::int64_t(symbol->number)};
			} else {
				auto number = signed_integer(input);
				if (std::ranges::any_of(
				        symbols, [number](const auto &symbol) { return symbol.number == number; }))
					return value{number};
			}
			fail("Unknown closed enum value", type.identity);
		}
		if (type.type == kind::record) {
			const auto &raw = as<value::record>(input);
			if (raw.contains("$ref") || raw.contains("$uri")) {
				auto form =
				    raw.contains("$ref") ? reference_policy::local : reference_policy::external;
				if (storage != form && storage != reference_policy::any)
					fail("Reference form violates occurrence policy");
				auto target = reference(input, uri, depth + 1);
				if (target.type != type.identity)
					fail("Reference target type mismatch", type.identity);
				return value{std::move(target)};
			}
			if (storage != reference_policy::inline_value && storage != reference_policy::any)
				fail("Inline object violates reference-only policy");
			if (auto named = find(raw, "$type"); named && text(*named, 128) != type.identity)
				fail("Inline object type mismatch", type.identity);
			if (auto id = find(raw, "$id"))
				return object({uri, text(*id, 128), type.identity}, depth + 1);
			return record(raw, type.identity, uri, depth + 1);
		}
		const auto &list = as<value::sequence>(input);
		if (list.size() > 4096)
			fail("Container item budget exceeded");
		const auto     &arguments = type.arguments;
		value::sequence result;
		if (type.type == kind::map) {
			std::map<std::string, value> entries;
			for (const auto &entry : list) {
				const auto &pair = as<value::sequence>(entry);
				if (pair.size() != 2)
					fail("Map entries must be explicit key/value pairs");
				auto key   = normalize(arguments[0], pair[0], uri, reference_policy::inline_value,
				                       depth + 1);
				auto token = key_token(key);
				auto item  = normalize(arguments[1], pair[1], uri, storage, depth + 1);
				if (!entries.emplace(token, value{value::sequence{std::move(key), std::move(item)}})
				         .second)
					fail("Duplicate canonical map key");
			}
			for (auto &[token, item] : entries) {
				(void)token;
				result.push_back(std::move(item));
			}
		} else if (type.type == kind::set) {
			std::map<std::string, value> entries;
			for (const auto &entry : list) {
				auto item = normalize(arguments[0], entry, uri, storage, depth + 1);
				if (!entries.emplace(key_token(item), item).second)
					fail("Duplicate set element");
			}
			for (auto &[token, item] : entries) {
				(void)token;
				result.push_back(std::move(item));
			}
		} else {
			for (const auto &entry : list)
				result.push_back(normalize(arguments[0], entry, uri, storage, depth + 1));
		}
		return value{std::move(result)};
	}
	value object(const object_reference &target, unsigned depth) {
		spend(depth);
		object_address address{target.document, target.identity};
		if (auto found = result_.objects.find(address); found != result_.objects.end())
			return clone(found->second);
		if (!active_.insert(address).second)
			fail("Prototype/inline containment cycle", target.document + "#" + target.identity);
		auto source = index_.find(address);
		if (source == index_.end())
			fail("Unknown inline object identity", target.identity);
		auto materialized =
		    record(as<value::record>(*source->second), target.type, target.document, depth + 1);
		effective_budget_.charge(materialized);
		auto [stored, inserted] = result_.objects.emplace(address, std::move(materialized));
		(void)inserted;
		active_.erase(address);
		return clone(stored->second);
	}
	value record(const value::record &raw, const std::string &type_id, const std::string &uri,
	             unsigned depth, const value::record *inherited = nullptr) {
		spend(depth);
		const auto &type = *definitions_.find(type_id);
		if (auto named = find(raw, "$type"); named && text(*named, 128) != type_id)
			fail("Record type mismatch", type_id);
		if (inherited && (inherited->contains("$id") || raw.contains("$id")))
			fail("An occurrence merge cannot fork a named inline object's identity");
		for (const auto &[name, item] : raw) {
			(void)item;
			if (name != "$id" && name != "$type" && name != "$prototype" && !field(type, name))
				fail("Unknown record field/control operation", uri + "#" + type_id + "." + name);
		}
		value prototype;
		if (auto base = find(raw, "$prototype")) {
			if (inherited || !type.prototype)
				fail("Prototype inheritance is not allowed here", type_id);
			auto target = reference(*base, uri, depth + 1);
			if (target.type != type_id)
				fail("Prototype type mismatch", type_id);
			prototype = object(target, depth + 1);
			inherited = &as<value::record>(prototype);
		}
		value::record result{{"$type", value{type_id}}};
		if (auto id = find(raw, "$id"))
			result.emplace("$id", clone(*id));
		for (const auto &definition : type.fields) {
			try {
				auto  own  = find(raw, definition.name);
				auto  base = inherited ? find(*inherited, definition.name) : nullptr;
				value item;
				if (!own) {
					if (base)
						item = clone(*base);
					else if (definition.default_value)
						item = normalize(definition.type, *definition.default_value, uri,
						                 definition.storage, depth + 1);
					else
						fail("Missing required field");
				} else {
					auto controls = std::get_if<value::record>(&own->data);
					if (controls && controls->size() == 1 && controls->contains("$reset")) {
						if (!as<bool>(controls->at("$reset")) || !definition.default_value)
							fail("Reset requires a declared default");
						item = normalize(definition.type, *definition.default_value, uri,
						                 definition.storage, depth + 1);
					} else if (controls && controls->size() == 1 &&
					           controls->contains("$replace")) {
						item = normalize(definition.type, controls->at("$replace"), uri,
						                 definition.storage, depth + 1);
					} else if (controls && controls->contains("$ops")) {
						if (controls->size() != 1 || !base)
							fail("Container edits need an inherited value");
						item = patch(definition, *base, controls->at("$ops"), uri, depth + 1);
					} else if (definition.merge == merge_policy::record && base && controls &&
					           std::holds_alternative<value::record>(base->data)) {
						item = record(*controls, definition.type.identity, uri, depth + 1,
						              &as<value::record>(*base));
					} else
						item = normalize(definition.type, *own, uri, definition.storage, depth + 1);
				}
				if (definition.merge == merge_policy::keyed && !null(item)) {
					std::set<std::string> identities;
					for (const auto &entry : as<value::sequence>(item)) {
						auto token = key_token(required(as<value::record>(entry), definition.key));
						if (!identities.insert(token).second)
							fail("Duplicate keyed sequence identity");
					}
				}
				result.emplace(definition.name, std::move(item));
			} catch (invalid &error) {
				if (error.error.path.empty())
					error.error.path = uri + "#" + type_id + "." + definition.name;
				throw;
			}
		}
		return value{std::move(result)};
	}
	value patch(const field_schema &definition, const value &base, const value &input,
	            const std::string &uri, unsigned depth) {
		const auto &operations = as<value::sequence>(input);
		if (operations.size() > 128 || null(base))
			fail("Invalid patch operation count or null base");
		if (definition.merge != merge_policy::keyed && definition.merge != merge_policy::map &&
		    definition.merge != merge_policy::set)
			fail("Container does not permit patch inheritance");
		const auto &arguments = definition.type.arguments;
		auto        copied    = clone(base);
		auto       &items     = std::get<value::sequence>(copied.data);
		if (definition.merge == merge_policy::keyed) {
			const auto &key_type =
			    field(*definitions_.find(arguments[0].identity), definition.key)->type;
			auto key = [&](const value &raw) {
				return normalize(key_type, raw, uri, reference_policy::inline_value, depth + 1);
			};
			auto position = [&](const value &id) {
				auto token = key_token(id);
				for (std::size_t i = 0; i < items.size(); ++i)
					if (key_token(required(as<value::record>(items[i]), definition.key)) == token)
						return i;
				fail("Missing keyed element/anchor");
			};
			for (std::size_t operation = 0; operation < operations.size(); ++operation) {
				spend(depth);
				const auto &op     = as<value::record>(operations[operation]);
				auto        action = text(required(op, "op"));
				if (action != "insert" && action != "update" && action != "remove" &&
				    action != "move")
					fail("Unknown keyed operation");
				std::set<std::string> required_names =
				    action == "insert"   ? std::set<std::string>{"op", "value"}
				    : action == "update" ? std::set<std::string>{"op", "id", "changes"}
				                         : std::set<std::string>{"op", "id"};
				for (const auto &name : required_names)
					(void)required(op, name);
				for (const auto &[name, ignored] : op) {
					(void)ignored;
					if (!required_names.contains(name) &&
					    !((action == "insert" || action == "move") &&
					      (name == "before" || name == "after")))
						fail("Unknown keyed operation property");
				}
				if (op.contains("before") && op.contains("after"))
					fail("Choose one sequence anchor");
				value element, identity;
				if (action == "insert") {
					element  = normalize(arguments[0], required(op, "value"), uri,
					                     definition.storage, depth + 1);
					identity = clone(required(as<value::record>(element), definition.key));
					for (const auto &entry : items)
						if (key_token(required(as<value::record>(entry), definition.key)) ==
						    key_token(identity))
							fail("Duplicate sequence insertion");
				} else {
					identity   = key(required(op, "id"));
					auto index = position(identity);
					if (action == "update") {
						const auto &changes = as<value::record>(required(op, "changes"));
						for (const auto &[name, ignored] : changes) {
							(void)ignored;
							if (name == definition.key || name.starts_with('$'))
								fail("An update cannot mutate identity/control fields");
						}
						items[index] = record(changes, arguments[0].identity, uri, depth + 1,
						                      &as<value::record>(items[index]));
						continue;
					}
					element = std::move(items[index]);
					items.erase(items.begin() + static_cast<std::ptrdiff_t>(index));
					if (action == "remove")
						continue;
				}
				std::size_t destination = items.size();
				auto        anchor      = find(op, "before");
				bool        after       = !anchor && op.contains("after");
				if (after)
					anchor = find(op, "after");
				if (anchor) {
					auto anchor_id = key(*anchor);
					if (key_token(anchor_id) == key_token(identity))
						fail("An element cannot anchor itself");
					destination = position(anchor_id) + (after ? 1 : 0);
				}
				items.insert(items.begin() + static_cast<std::ptrdiff_t>(destination),
				             std::move(element));
				if (items.size() > 4096)
					fail("Container item budget exceeded");
			}
		} else if (definition.merge == merge_policy::map || definition.merge == merge_policy::set) {
			bool                         map = definition.merge == merge_policy::map;
			std::map<std::string, value> entries;
			for (auto &item : items) {
				auto &id = map ? as<value::sequence>(item)[0] : item;
				entries.emplace(key_token(id), std::move(item));
			}
			for (const auto &raw_op : operations) {
				spend(depth);
				const auto &op     = as<value::record>(raw_op);
				auto        action = text(required(op, "op"));
				if (map && action == "put")
					properties(op, {"op", "key", "value"});
				else if (map && action == "update")
					properties(op, {"op", "key", "changes"});
				else if (action == "remove" || (!map && action == "add"))
					properties(op, {"op", "key"});
				else
					fail("Unknown map/set operation");
				auto id       = normalize(arguments[0], required(op, "key"), uri,
				                          reference_policy::inline_value, depth + 1);
				auto token    = key_token(id);
				auto existing = entries.find(token);
				if (action == "remove" || action == "update") {
					if (existing == entries.end())
						fail("Missing map/set edit target");
				}
				if (action == "remove")
					entries.erase(existing);
				else if (action == "add") {
					if (!entries.emplace(token, std::move(id)).second)
						fail("Duplicate set insertion");
				} else if (action == "put") {
					auto item = normalize(arguments[1], required(op, "value"), uri,
					                      definition.storage, depth + 1);
					entries.insert_or_assign(
					    token, value{value::sequence{std::move(id), std::move(item)}});
				} else {
					if (arguments[1].type != kind::record ||
					    definition.storage != reference_policy::inline_value)
						fail("Map update needs inline records; use put for replacement");
					const auto &changes = as<value::record>(required(op, "changes"));
					for (const auto &[name, ignored] : changes) {
						(void)ignored;
						if (name.starts_with('$'))
							fail("Map update cannot change object control fields");
					}
					const auto &previous = as<value::sequence>(existing->second)[1];
					auto        item     = record(changes, arguments[1].identity, uri, depth + 1,
					                              &as<value::record>(previous));
					existing->second     = value{value::sequence{std::move(id), std::move(item)}};
				}
				if (entries.size() > 4096)
					fail("Container item budget exceeded");
			}
			items.clear();
			for (auto &[token, item] : entries) {
				(void)token;
				items.push_back(std::move(item));
			}
		} else
			fail("Container does not permit patch inheritance");
		return copied;
	}
};
} // namespace
void validate_defaults(const catalog &definitions) { graph_builder(definitions, {}).defaults(); }
graph_data build_graph(const catalog &definitions, const std::string &uri, const value &document,
                       document_reader reader) {
	return graph_builder(definitions, std::move(reader)).load(uri, document);
}
} // namespace ice::data::detail
