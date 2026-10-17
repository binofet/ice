// Generated RTTR consumer; do not edit.
#include "schema.generated.h"
#include <rttr/registration>
RTTR_REGISTRATION {
	using namespace rttr;
	registration::class_<ice::generated::paddle_settings>("playground.paddle_settings")(
	    metadata("metagen.type_id", std::string("playground.paddle_settings")),
	    metadata("metagen.version", std::uint32_t(2)),
	    metadata("metagen.native_name", std::string("paddle_settings")),
	    metadata("metagen.tags", std::map<std::string, std::string>{{"game.category", "gameplay"}}),
	    metadata("game.category", std::string("gameplay")))
	    .constructor<>()(policy::ctor::as_object)
	    .property("maximum_speed", &ice::generated::paddle_settings::maximum_speed)(
	        metadata("metagen.field_id", std::uint32_t(1)),
	        metadata("metagen.kind", std::string("float32")), metadata("metagen.nullable", false),
	        metadata("metagen.has_default", true),
	        metadata("metagen.tags", std::map<std::string, std::string>{}),
	        metadata("metagen.default", ice::meta::value{2.799999952316284}),
	        metadata("metagen.minimum", 0.0), metadata("metagen.maximum", 20.0))
	    .property("predict_rebounds", &ice::generated::paddle_settings::predict_rebounds)(
	        metadata("metagen.field_id", std::uint32_t(2)),
	        metadata("metagen.kind", std::string("boolean")), metadata("metagen.nullable", false),
	        metadata("metagen.has_default", true),
	        metadata("metagen.tags", std::map<std::string, std::string>{}),
	        metadata("metagen.default", ice::meta::value{true}))
	    .property("winning_score", &ice::generated::paddle_settings::winning_score)(
	        metadata("metagen.field_id", std::uint32_t(3)),
	        metadata("metagen.kind", std::string("uint32")), metadata("metagen.nullable", false),
	        metadata("metagen.has_default", true),
	        metadata("metagen.tags", std::map<std::string, std::string>{}),
	        metadata("metagen.default", ice::meta::value{std::uint64_t(500)}),
	        metadata("metagen.minimum", 1.0), metadata("metagen.maximum", 10000.0))
	    .property("player_name", &ice::generated::paddle_settings::player_name)(
	        metadata("metagen.field_id", std::uint32_t(4)),
	        metadata("metagen.kind", std::string("string")), metadata("metagen.nullable", false),
	        metadata("metagen.has_default", true),
	        metadata("metagen.tags",
	                 std::map<std::string, std::string>{{"editor.label", "Player name"}}),
	        metadata("metagen.default", ice::meta::value{std::string("Player")}),
	        metadata("editor.label", std::string("Player name")))
	    .property("boost", &ice::generated::paddle_settings::boost)(
	        metadata("metagen.field_id", std::uint32_t(5)),
	        metadata("metagen.kind", std::string("float32")), metadata("metagen.nullable", true),
	        metadata("metagen.has_default", true),
	        metadata("metagen.tags", std::map<std::string, std::string>{}),
	        metadata("metagen.default", ice::meta::value{ice::meta::value{}}),
	        metadata("metagen.minimum", 0.0), metadata("metagen.maximum", 10.0))
	    .property("simulation_seed", &ice::generated::paddle_settings::simulation_seed)(
	        metadata("metagen.field_id", std::uint32_t(6)),
	        metadata("metagen.kind", std::string("int32")), metadata("metagen.nullable", false),
	        metadata("metagen.has_default", true),
	        metadata("metagen.tags", std::map<std::string, std::string>{}),
	        metadata("metagen.default", ice::meta::value{std::int64_t(-7)}),
	        metadata("metagen.minimum", -1000.0), metadata("metagen.maximum", 1000.0));
}
