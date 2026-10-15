#include "schema.generated.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
#include <random>
#include <utility>

#define CHECK(expression)                                                                          \
	do {                                                                                           \
		if (!(expression)) {                                                                       \
			std::fprintf(stderr, "Metagen failed %d: %s\n", __LINE__, #expression);                \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

int main() {
	using namespace ice::meta;
	using namespace ice::generated;
	const auto &schema = paddle_settings_schema();
	CHECK(validate_definition(schema));
	auto defaults = validate(schema, {});
	CHECK(defaults && defaults->size() == 6);
	CHECK(std::get<double>(defaults->at(1)) == double(2.8f));
	CHECK(std::holds_alternative<std::monostate>(defaults->at(5)));
	CHECK(schema.tags.at("game.category") == "gameplay");
	CHECK(find_field(schema, 4)->tags.at("editor.label") == "Player name");
	auto native = paddle_settings_from_record(*defaults);
	CHECK(native && native->maximum_speed == 2.8f && native->simulation_seed == -7);
	CHECK(native->predict_rebounds && native->winning_score == 500 && !native->boost);
	CHECK(to_record(*native) == *defaults);

	for (const value &invalid :
	     {value{true}, value{std::int64_t(2)}, value{-1.0},
	      value{std::numeric_limits<double>::infinity()},
	      value{std::numeric_limits<double>::quiet_NaN()}, value{1e100}, value{}})
		CHECK(!validate(schema, {{1, invalid}}));
	CHECK(!validate(schema, {{3, std::uint64_t(UINT32_MAX) + 1}}));
	CHECK(!validate(schema, {{3, std::int64_t(500)}}));
	CHECK(!validate(schema, {{2, std::uint64_t(1)}}));
	CHECK(!validate(schema, {{99, true}}));
	CHECK(!validate(schema, {{4, std::string("\xC0\xAF")}}));
	CHECK(!validate(schema, {{4, std::string("x\0y", 3)}}));
	CHECK(!validate(schema, {{4, std::string(4097, 'x')}}));
	CHECK(validate(schema, {{4, std::string("玩家 🎮")}}));
	CHECK(!validate(schema, {{6, std::int64_t(INT32_MAX) + 1}}));
	CHECK(!validate(schema, {{6, std::int64_t(INT32_MIN) - 1}}));
	auto required                  = schema;
	required.fields[4].has_default = false;
	CHECK(!validate(required, {}));
	CHECK(validate(required, {{5, value{}}})); // explicit null is not missing
	auto invalid_schema         = schema;
	invalid_schema.fields[0].id = 2;
	CHECK(!validate_definition(invalid_schema));
	invalid_schema                   = schema;
	invalid_schema.fields[0].minimum = 40;
	CHECK(!validate_definition(invalid_schema));
	invalid_schema                         = schema;
	invalid_schema.fields[0].default_value = true;
	CHECK(!validate_definition(invalid_schema));
	invalid_schema                = schema;
	invalid_schema.fields[0].kind = static_cast<scalar_kind>(255);
	CHECK(!validate_definition(invalid_schema));

	auto bytes = serialize(schema, *defaults);
	CHECK(bytes && deserialize(schema, *bytes) && *deserialize(schema, *bytes) == *defaults);
	CHECK(bytes->at(0) == std::byte{'I'} && bytes->at(3) == std::byte{'1'});
	const auto first = 4 + 4 + schema.id.size() + 4 + 4;
	CHECK(bytes->at(first + 4) == std::byte{static_cast<std::uint8_t>(scalar_kind::float32)});
	auto bits = std::bit_cast<std::uint32_t>(2.8f);
	for (unsigned i = 0; i < 4; ++i)
		CHECK(bytes->at(first + 6 + i) == std::byte((bits >> (8 * i)) & 255));
	for (std::size_t size = 0; size < bytes->size(); ++size)
		CHECK(!deserialize(schema, std::span(*bytes).first(size)));
	auto malformed = *bytes;
	malformed.push_back(std::byte{});
	CHECK(!deserialize(schema, malformed));
	malformed            = *bytes;
	malformed[first + 4] = std::byte{255};
	CHECK(!deserialize(schema, malformed));
	malformed            = *bytes;
	malformed[first + 5] = std::byte{2};
	CHECK(!deserialize(schema, malformed));
	malformed        = *bytes;
	malformed[first] = std::byte{99};
	CHECK(!deserialize(schema, malformed));
	malformed             = *bytes;
	malformed[first + 10] = std::byte{1}; // duplicate the next field identity
	CHECK(!deserialize(schema, malformed));
	malformed             = *bytes;
	malformed[first + 16] = std::byte{9}; // invalid boolean representation
	CHECK(!deserialize(schema, malformed));
	CHECK(!deserialize(schema, std::vector<std::byte>(1024 * 1024 + 1)));
	std::mt19937 random(42);
	for (int i = 0; i < 1000; ++i) {
		std::vector<std::byte> noise(random() % 192);
		for (auto &byte : noise)
			byte = std::byte(random() & 255);
		CHECK(!deserialize(schema, noise));
	}
	// Exercise scalar representations absent from this particular authored fixture too.
	auto wide           = schema;
	wide.fields[0].kind = scalar_kind::float64;
	wide.fields[0].maximum.reset();
	auto wide_data = validate(wide, {{1, 1e100}});
	CHECK(wide_data && std::get<double>(wide_data->at(1)) == 1e100);
	auto wide_bytes = serialize(wide, *wide_data);
	CHECK(wide_bytes && deserialize(wide, *wide_bytes) &&
	      *deserialize(wide, *wide_bytes) == *wide_data);
	wide.fields[2].maximum.reset();
	wide.fields[5].minimum.reset();
	wide.fields[5].maximum.reset();
	CHECK(validate(wide, {{3, std::uint64_t(UINT32_MAX)}, {6, std::int64_t(INT32_MIN)}}));
	wide.fields[3].nullable = true;
	auto null_text          = serialize(wide, {{4, value{}}});
	CHECK(null_text && deserialize(wide, *null_text) &&
	      std::holds_alternative<std::monostate>(deserialize(wide, *null_text)->at(4)));

	auto previous           = schema;
	previous.version        = 1;
	previous.fields[0].name = "speed"; // renaming does not change ID or binary field meaning
	previous.fields.pop_back();
	auto old_data = validate(previous, {{1, 3.25}});
	CHECK(old_data);
	auto upgraded = migrate(previous, schema, *old_data);
	CHECK(upgraded && std::get<double>(upgraded->at(1)) == 3.25 &&
	      std::get<std::int64_t>(upgraded->at(6)) == -7);
	auto old_bytes = serialize(previous, *old_data);
	CHECK(old_bytes && !deserialize(schema, *old_bytes));
	CHECK(deserialize(previous, *old_bytes) && *deserialize(previous, *old_bytes) == *old_data);
	auto changed           = schema;
	changed.fields[0].kind = scalar_kind::float64;
	CHECK(!migrate(previous, changed, *old_data));
	changed = schema;
	changed.fields.erase(changed.fields.begin() + 3);
	CHECK(!migrate(previous, changed, *old_data));
	const std::array<field_id, 1> dropped{4};
	CHECK(migrate(previous, changed, *old_data, dropped));
	changed                           = schema;
	changed.fields.back().has_default = false;
	CHECK(!migrate(previous, changed, *old_data));
	changed                   = schema;
	changed.fields[0].maximum = 3;
	CHECK(!migrate(previous, changed, *old_data));

	object_store store(schema);
	auto         handle = store.create({});
	CHECK(handle);
	auto before = store.inspect(*handle);
	CHECK(before && before->revision == 1);
	const std::array good{field_edit{1, value{5.0}}, field_edit{4, value{std::string("Live")}}};
	auto             revision = store.apply(*handle, before->revision, good);
	CHECK(revision && *revision == 2);
	CHECK(std::get<double>(store.inspect(*handle)->fields.at(1)) == 5);
	CHECK(before->fields == *defaults); // the inspector owns its snapshot
	CHECK(!store.apply(*handle, 1, good));
	const std::array bad{field_edit{1, value{6.0}}, field_edit{3, value{std::uint64_t(0)}}};
	CHECK(!store.apply(*handle, 2, bad));
	CHECK(store.inspect(*handle)->revision == 2 &&
	      std::get<double>(store.inspect(*handle)->fields.at(1)) == 5);
	const std::array duplicate{field_edit{1, value{6.0}}, field_edit{1, value{7.0}}};
	CHECK(!store.apply(*handle, 2, duplicate));
	CHECK(!store.undo(*handle, 1));
	auto undone = store.undo(*handle, 2);
	CHECK(undone && *undone == 3);
	CHECK(store.inspect(*handle)->fields == *defaults);
	CHECK(!store.undo(*handle, 3)); // rejected edits never entered history
	const std::array nullable_edit{field_edit{5, value{2.0}}};
	CHECK(store.apply(*handle, 3, nullable_edit));
	const std::array reset{field_edit{5, std::nullopt}};
	CHECK(store.apply(*handle, 4, reset));
	CHECK(std::holds_alternative<std::monostate>(store.inspect(*handle)->fields.at(5)));
	// Bounded undo retains the most recent 32 successful changes.
	for (unsigned i = 0; i < 40; ++i) {
		const std::array edit{field_edit{1, value{double(i % 20)}}};
		CHECK(store.apply(*handle, store.inspect(*handle)->revision, edit));
	}
	for (unsigned i = 0; i < 32; ++i)
		CHECK(store.undo(*handle, store.inspect(*handle)->revision));
	CHECK(!store.undo(*handle, store.inspect(*handle)->revision));

	CHECK(store.erase(*handle));
	CHECK(!store.inspect(*handle));
	auto replacement = store.create({});
	CHECK(replacement && replacement->slot == handle->slot &&
	      replacement->generation != handle->generation);
	CHECK(!store.apply(*handle, 1, good));
	CHECK(store.inspect(*replacement)->revision == 1);
	for (int i = 0; i < 63; ++i)
		CHECK(store.create({}));
	CHECK(!store.create({}));
	std::puts("Metagen native passed: defaults/types, bounded binary round-trip/rejection, "
	          "migration, atomic edits, undo and stale handles");
	return 0;
}
