#include "schema.generated.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>

namespace {
using namespace ice::meta;
ice::expected<value, diagnostic> parse_value(const field_descriptor &field, std::string text) {
	std::istringstream input(text);
	input.imbue(std::locale::classic());
	value result;
	if (text == "null")
		return validate_value(field, result);
	switch (field.kind) {
	case scalar_kind::boolean: {
		bool parsed;
		if (!(input >> std::boolalpha >> parsed))
			return ice::unexpected(diagnostic{"Expected true or false", field.id});
		result = parsed;
		break;
	}
	case scalar_kind::int32: {
		std::int64_t parsed;
		if (!(input >> parsed))
			return ice::unexpected(diagnostic{"Expected signed integer", field.id});
		result = parsed;
		break;
	}
	case scalar_kind::uint32: {
		std::uint64_t parsed;
		if (!(input >> parsed) || text.starts_with('-'))
			return ice::unexpected(diagnostic{"Expected unsigned integer", field.id});
		result = parsed;
		break;
	}
	case scalar_kind::float32:
	case scalar_kind::float64: {
		double parsed;
		if (!(input >> parsed))
			return ice::unexpected(diagnostic{"Expected finite number", field.id});
		result = parsed;
		break;
	}
	case scalar_kind::string: {
		std::string parsed;
		if (!(input >> std::quoted(parsed)))
			return ice::unexpected(diagnostic{"Expected quoted text", field.id});
		result = std::move(parsed);
		break;
	}
	}
	input >> std::ws;
	if (!input.eof())
		return ice::unexpected(diagnostic{"Trailing value text", field.id});
	return validate_value(field, result);
}
void show(const object_snapshot &snapshot, double distance, const schema_descriptor &schema) {
	std::cout << "revision=" << snapshot.revision << " distance=" << distance << '\n';
	for (const auto &[id, data] : snapshot.fields) {
		std::cout << id << ' ' << find_field(schema, id)->name << " = ";
		std::visit(
		    [](const auto &v) {
			    using type = std::decay_t<decltype(v)>;
			    if constexpr (std::is_same_v<type, std::monostate>)
				    std::cout << "null";
			    else if constexpr (std::is_same_v<type, std::string>)
				    std::cout << std::quoted(v);
			    else
				    std::cout << std::boolalpha << v;
		    },
		    data);
		std::cout << '\n';
	}
}
} // namespace
int main(int argc, char **argv) {
	using namespace ice::meta;
	const auto  &schema = ice::generated::paddle_settings_schema();
	object_store store(schema);
	auto         handle = store.create({});
	if (!handle)
		return 1;
	double distance = 0;
	auto   step     = [&](unsigned ticks) {
        auto snapshot = store.inspect(*handle);
        auto native   = ice::generated::paddle_settings_from_record(snapshot->fields);
        for (unsigned tick = 0; tick < ticks; ++tick)
            distance += native->maximum_speed / 60.0;
	};
	if (argc == 2 && std::string_view(argv[1]) == "--demo") {
		step(60);
		if (std::abs(distance - double(2.8f)) > 1e-9)
			return 2;
		const std::array edits{field_edit{1, value{6.0}}};
		if (!store.apply(*handle, 1, edits))
			return 3;
		step(60);
		const std::array invalid{field_edit{1, value{-1.0}}};
		if (store.apply(*handle, 2, invalid) || store.inspect(*handle)->revision != 2)
			return 4;
		if (!store.undo(*handle, 2))
			return 5;
		step(60);
		if (std::abs(distance - (2 * double(2.8f) + 6.0)) > 1e-9)
			return 6;
		show(*store.inspect(*handle), distance, schema);
		std::puts("Live-data lab passed: step -> edit -> step -> reject invalid edit -> undo -> "
		          "step, one process");
		return 0;
	}
	std::puts("Metagen live-data lab (host-thread prototype, no graphics). Commands:\ninspect | "
	          "set REVISION FIELD_ID VALUE | undo REVISION | step TICKS | quit\nStrings may be "
	          "quoted; null is explicit null. Undo restores data, not elapsed simulation time.");
	std::string line;
	while (std::cout << "> " && std::getline(std::cin, line)) {
		if (line.size() > 8192) {
			std::puts("error: command exceeds local-lab budget");
			continue;
		}
		std::istringstream input(line);
		std::string        command;
		input >> command;
		if (command == "quit")
			break;
		if (command == "inspect")
			show(*store.inspect(*handle), distance, schema);
		else if (command == "step") {
			unsigned ticks;
			if (!(input >> ticks) || ticks > 3600) {
				std::puts("error: step requires 0..3600 ticks");
				continue;
			}
			step(ticks);
			show(*store.inspect(*handle), distance, schema);
		} else if (command == "undo") {
			std::uint64_t revision;
			if (!(input >> revision)) {
				std::puts("error: undo requires revision");
				continue;
			}
			auto result = store.undo(*handle, revision);
			if (!result)
				std::cout << "error: " << result.error().message << '\n';
			else
				show(*store.inspect(*handle), distance, schema);
		} else if (command == "set") {
			std::uint64_t revision;
			field_id      id;
			if (!(input >> revision >> id)) {
				std::puts("error: set requires revision, ID and value");
				continue;
			}
			auto field = find_field(schema, id);
			if (!field) {
				std::puts("error: unknown field ID");
				continue;
			}
			std::string text;
			std::getline(input >> std::ws, text);
			auto parsed = parse_value(*field, text);
			if (!parsed) {
				std::cout << "error: " << parsed.error().message << '\n';
				continue;
			}
			const std::array edit{field_edit{id, *parsed}};
			auto             result = store.apply(*handle, revision, edit);
			if (!result)
				std::cout << "error: " << result.error().message << '\n';
			else
				show(*store.inspect(*handle), distance, schema);
		} else
			std::puts("error: unknown command");
	}
}
