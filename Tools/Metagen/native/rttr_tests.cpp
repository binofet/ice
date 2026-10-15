#include "rttr_adapter.h"
#include "schema.generated.h"
#include <array>
#include <cstdio>
#include <rttr/type>

#define CHECK(expression)                                                                          \
	do {                                                                                           \
		if (!(expression)) {                                                                       \
			std::fprintf(stderr, "RTTR integration failed %d: %s\n", __LINE__, #expression);       \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

int main() {
	using ice::generated::paddle_settings;
	using namespace ice::meta;
	auto type = rttr::type::get_by_name("playground.paddle_settings");
	CHECK(type.is_valid() && type == rttr::type::get<paddle_settings>());
	CHECK(type.get_metadata("metagen.type_id").get_value<std::string>() ==
	      "playground.paddle_settings");
	CHECK(type.get_metadata("metagen.version").get_value<std::uint32_t>() == 2);
	CHECK(type.get_properties().size() == 6);
	CHECK(type.get_metadata("game.category").get_value<std::string>() == "gameplay");
	auto instance = type.create();
	CHECK(instance.is_valid());
	auto property = type.get_property("maximum_speed");
	CHECK(property.is_valid() && property.get_type() == rttr::type::get<float>());
	CHECK(property.get_metadata("metagen.field_id").get_value<std::uint32_t>() == 1);
	CHECK(property.get_metadata("metagen.kind").get_value<std::string>() == "float32");
	CHECK(property.get_value(instance).get_value<float>() == 2.8f);
	CHECK(property.get_metadata("metagen.minimum").get_value<double>() == 0);
	CHECK(property.get_metadata("metagen.maximum").get_value<double>() == 20);
	CHECK(type.get_property("player_name").get_metadata("editor.label").get_value<std::string>() ==
	      "Player name");
	CHECK(property.set_value(instance, 5.5f));
	CHECK(property.get_value(instance).get_value<float>() == 5.5f);
	CHECK(!property.set_value(instance, std::string("not a float")));
	CHECK(type.get_property("boost").get_metadata("metagen.nullable").get_value<bool>());
	CHECK(!type.get_property("does_not_exist").is_valid());

	const auto &schema    = ice::generated::paddle_settings_schema();
	auto        reflected = ice::examples::rttr_bridge::describe(type);
	CHECK(reflected && reflected->id == schema.id && reflected->version == schema.version);
	CHECK(find_field(*reflected, 1)->minimum == find_field(schema, 1)->minimum);
	CHECK(find_field(*reflected, 1)->maximum == find_field(schema, 1)->maximum);
	CHECK(reflected->name == schema.name && reflected->tags == schema.tags);
	CHECK(find_field(*reflected, 4)->tags == find_field(schema, 4)->tags);
	CHECK(validate(*reflected, {}) && *validate(*reflected, {}) == *validate(schema, {}));
	CHECK(!ice::examples::rttr_bridge::describe(rttr::type::get<int>()));
	CHECK(type.get_property("boost").get_value(instance).get_value<std::optional<float>>() ==
	      std::nullopt);
	CHECK(type.get_property("boost").set_value(instance, std::optional<float>(1.5f)));
	CHECK(type.get_property("boost").get_value(instance).get_value<std::optional<float>>() ==
	      std::optional<float>(1.5f));
	object_store live(*reflected);
	auto         handle = live.create({});
	CHECK(handle);
	auto snapshot = live.inspect(*handle);
	CHECK(snapshot);
	auto staged = ice::generated::paddle_settings_from_record(snapshot->fields);
	CHECK(staged);
	// RTTR is a consumer, not a privileged write path. Mutate a detached native
	// candidate, then submit its field value through the same validated command API.
	CHECK(property.set_value(*staged, 6.0f));
	auto             detached = ice::generated::to_record(*staged);
	const std::array changes{field_edit{
	    property.get_metadata("metagen.field_id").get_value<std::uint32_t>(), detached.at(1)}};
	CHECK(live.apply(*handle, snapshot->revision, changes));
	CHECK(std::get<double>(live.inspect(*handle)->fields.at(1)) == 6);
	CHECK(property.set_value(*staged, -10.0f)); // C++ type-correct, schema-invalid
	detached = ice::generated::to_record(*staged);
	const std::array invalid{field_edit{1, detached.at(1)}};
	CHECK(!live.apply(*handle, 2, invalid));
	CHECK(std::get<double>(live.inspect(*handle)->fields.at(1)) == 6);
	CHECK(live.undo(*handle, 2));
	CHECK(std::get<double>(live.inspect(*handle)->fields.at(1)) == double(2.8f));
	auto persisted = serialize(schema, live.inspect(*handle)->fields);
	CHECK(persisted && deserialize(schema, *persisted));
	instance.clear(); // as_object construction gives the variant ownership of native state.
	CHECK(!instance.is_valid());
	std::puts("RTTR passed: generated registration, metadata, dynamic construction/properties, "
	          "validated edits, undo and persistence");
}
