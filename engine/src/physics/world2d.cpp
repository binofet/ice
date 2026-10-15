#include "ice/physics/world2d.h"
#include "internal.h"
#include <algorithm>
#include <box2d/box2d.h>
#include <cmath>
#include <limits>
#include <mutex>
#include <numbers>
#include <vector>

namespace ice::physics {
namespace {
std::mutex registry_mutex;
bool bounded(float value, float limit) { return std::isfinite(value) && std::abs(value) <= limit; }
bool bounded(const math::vec2 &value, float limit) {
	return bounded(value.x, limit) && bounded(value.y, limit);
}
bool between(float value, float minimum, float maximum) {
	return std::isfinite(value) && value >= minimum && value <= maximum;
}
b2Vec2     native(const math::vec2 &value) { return {value.x, value.y}; }
math::vec2 engine(b2Vec2 value) { return {value.x, value.y}; }
b2Rot      native_rotation(float angle) {
    return b2MakeRot(std::remainder(angle, 2.0f * std::numbers::pi_v<float>));
}
ice::unexpected<diagnostic> rejected(std::string message) {
	return ice::unexpected(diagnostic{std::move(message)});
}
bool valid_motion(motion_type motion) {
	return motion == motion_type::static_body || motion == motion_type::kinematic ||
	       motion == motion_type::dynamic;
}
bool valid_shape(const shape2d_description &shape) {
	if (!bounded(shape.half_extent, 1000) || !bounded(shape.radius, 1000) ||
	    !bounded(shape.half_height, 1000))
		return false;
	switch (shape.kind) {
	case shape2d_kind::box:
		return shape.half_extent.x >= 0.005f && shape.half_extent.y >= 0.005f;
	case shape2d_kind::circle:
		return shape.radius >= 0.005f;
	case shape2d_kind::capsule:
		return shape.radius >= 0.005f && shape.half_height >= 0;
	}
	return false;
}
} // namespace

struct world2d::implementation {
	struct slot {
		b2BodyId      body{};
		b2ShapeId     shape{};
		std::uint64_t generation = 1;
		std::uint32_t index      = 0;
		motion_type   motion     = motion_type::static_body;
		bool          live       = false;
		bool          stepped    = false;
	};
	struct retired_shape {
		std::uint64_t native_id = 0;
		body_id       body;
	};
	b2WorldId                  world{};
	world2d_description        description;
	std::uint64_t              identity = 0, tick = 0;
	std::uint32_t              body_count = 0;
	bool                       faulted    = false;
	std::vector<slot>          slots;
	std::vector<retired_shape> retired;
	std::vector<contact_event> events;

	~implementation() {
		if (B2_IS_NON_NULL(world)) {
			std::lock_guard lock(registry_mutex);
			b2DestroyWorld(world);
		}
	}
	body_id handle(const slot &body) const { return {identity, body.index + 1, body.generation}; }
	slot   *find(body_id body) {
        if (body.owner != identity || !body.owner || body.slot == 0 || body.slot > slots.size())
            return nullptr;
        auto &candidate = slots[body.slot - 1];
        return candidate.live && candidate.generation == body.generation ? &candidate : nullptr;
	}
	const slot *find(body_id body) const { return const_cast<implementation *>(this)->find(body); }
	body_id     attribute(b2ShapeId shape, bool end) const {
        const auto key = b2StoreShapeId(shape);
        if (end) {
            auto old =
                std::lower_bound(retired.begin(), retired.end(), key,
			                         [](const auto &item, auto id) { return item.native_id < id; });
            if (old != retired.end() && old->native_id == key)
                return old->body;
        }
        if (!b2Shape_IsValid(shape))
            return {};
        const auto *body = static_cast<const slot *>(b2Shape_GetUserData(shape));
        return body && body->live && b2StoreShapeId(body->shape) == key ? handle(*body) : body_id{};
	}
	void append(contact_phase phase, b2ShapeId first, b2ShapeId second, bool sensor,
	            bool &truncated) {
		const bool end = phase == contact_phase::end;
		auto       a = attribute(first, end), b = attribute(second, end);
		if (!a.owner || !b.owner || events.size() == description.max_contact_events) {
			truncated = true;
			return;
		}
		events.push_back({phase, a, b, sensor});
	}
	static float ray_hit(b2ShapeId shape, b2Vec2 point, b2Vec2 normal, float fraction,
	                     void *context);
	struct ray_context {
		const implementation    &state;
		std::optional<ray_hit2d> hit;
	};
};

float world2d::implementation::ray_hit(b2ShapeId shape, b2Vec2 point, b2Vec2 normal, float fraction,
                                       void *context) {
	auto &query = *static_cast<ray_context *>(context);
	if (b2Shape_IsSensor(shape))
		return -1;
	auto body = query.state.attribute(shape, false);
	if (!body.owner)
		return -1;
	query.hit = ray_hit2d{body, engine(point), engine(normal), fraction};
	return fraction;
}

world2d::world2d(std::unique_ptr<implementation> state) : state_(std::move(state)) {}
world2d::~world2d()                                   = default;
world2d::world2d(world2d &&other) noexcept            = default;
world2d &world2d::operator=(world2d &&other) noexcept = default;

ice::expected<world2d, diagnostic> world2d::create(const world2d_description &description) {
	if (!bounded(description.gravity, 10000) ||
	    !between(description.time_step, 1.0f / 1000.0f, 1.0f / 15.0f) ||
	    description.substeps == 0 || description.substeps > 16 || description.max_bodies == 0 ||
	    description.max_bodies > 65536 || description.max_contact_events == 0 ||
	    description.max_contact_events > 65536)
		return rejected("Invalid fixed-step settings, gravity or physics storage budget");
	auto state         = std::make_unique<implementation>();
	state->description = description;
	state->slots.resize(description.max_bodies);
	for (std::uint32_t index = 0; index < description.max_bodies; ++index)
		state->slots[index].index = index;
	state->retired.reserve(description.max_bodies);
	state->events.reserve(description.max_contact_events);
	state->identity = detail::next_world_identity();
	if (!state->identity)
		return rejected("Physics world lifetime identity space is exhausted");
	auto definition    = b2DefaultWorldDef();
	definition.gravity = native(description.gravity);
	// No task callbacks or hidden worker pool: the caller owns scheduling.
	definition.workerCount = 0;
	{
		std::lock_guard lock(registry_mutex);
		state->world = b2CreateWorld(&definition);
	}
	if (B2_IS_NULL(state->world))
		return rejected("Box2D native world capacity is exhausted");
	return world2d(std::move(state));
}

ice::expected<body_id, diagnostic> world2d::create_body(const body2d_description &description) {
	if (!state_ || state_->faulted)
		return rejected("Physics world is unavailable or faulted");
	if (!valid_motion(description.motion) || !valid_shape(description.shape) ||
	    !bounded(description.position, 1000000) || !std::isfinite(description.rotation) ||
	    !bounded(description.linear_velocity, 10000) ||
	    !bounded(description.angular_velocity, 10000) ||
	    !between(description.mass, 0.0001f, 1000000) || !between(description.friction, 0, 10) ||
	    !between(description.restitution, 0, 1))
		return rejected("Invalid body geometry, motion, material, mass, pose or velocity");
	auto &state = *state_;
	auto  found = std::find_if(state.slots.begin(), state.slots.end(), [](const auto &item) {
        return !item.live && item.generation != std::numeric_limits<std::uint64_t>::max();
    });
	if (found == state.slots.end())
		return rejected("Physics body budget or slot generation space is exhausted");
	auto &slot                 = *found;
	auto  definition           = b2DefaultBodyDef();
	definition.type            = description.motion == motion_type::dynamic     ? b2_dynamicBody
	                             : description.motion == motion_type::kinematic ? b2_kinematicBody
	                                                                            : b2_staticBody;
	definition.position        = native(description.position);
	definition.rotation        = native_rotation(description.rotation);
	definition.linearVelocity  = native(description.linear_velocity);
	definition.angularVelocity = description.angular_velocity;
	definition.isBullet     = description.continuous && description.motion == motion_type::dynamic;
	slot.body               = b2CreateBody(state.world, &definition);
	auto shape              = b2DefaultShapeDef();
	shape.userData          = &slot;
	shape.material.friction = description.friction;
	shape.material.restitution = description.restitution;
	shape.isSensor             = description.sensor;
	shape.enableSensorEvents   = true;
	shape.enableContactEvents  = true;
	switch (description.shape.kind) {
	case shape2d_kind::box: {
		auto geometry = b2MakeBox(description.shape.half_extent.x, description.shape.half_extent.y);
		slot.shape    = b2CreatePolygonShape(slot.body, &shape, &geometry);
		break;
	}
	case shape2d_kind::circle:
	case shape2d_kind::capsule:
		if (description.shape.kind == shape2d_kind::circle || description.shape.half_height == 0) {
			b2Circle geometry{{0, 0}, description.shape.radius};
			slot.shape = b2CreateCircleShape(slot.body, &shape, &geometry);
		} else {
			b2Capsule geometry{{0, -description.shape.half_height},
			                   {0, description.shape.half_height},
			                   description.shape.radius};
			slot.shape = b2CreateCapsuleShape(slot.body, &shape, &geometry);
		}
		break;
	}
	if (description.motion == motion_type::dynamic) {
		auto mass = b2Body_GetMassData(slot.body);
		mass.rotationalInertia *= description.mass / mass.mass;
		mass.mass = description.mass;
		b2Body_SetMassData(slot.body, mass);
	}
	slot.motion  = description.motion;
	slot.live    = true;
	slot.stepped = false;
	++state.body_count;
	return state.handle(slot);
}

ice::expected<void, diagnostic> world2d::erase(body_id body) {
	auto *slot = state_ ? state_->find(body) : nullptr;
	if (!slot)
		return rejected("Stale, foreign or unavailable physics body");
	// Only bodies present at the last step can own buffered contact ends. Consequently
	// at most max_bodies tombstones are needed, even with arbitrary retire/recreate churn
	// between steps. End-event attribution checks old lifetimes before recycled native IDs.
	if (slot->stepped)
		state_->retired.push_back({b2StoreShapeId(slot->shape), body});
	b2DestroyBody(slot->body);
	slot->live    = false;
	slot->stepped = false;
	if (slot->generation != std::numeric_limits<std::uint64_t>::max())
		++slot->generation;
	--state_->body_count;
	return {};
}

ice::expected<body2d_snapshot, diagnostic> world2d::inspect(body_id body) const {
	const auto *slot = state_ ? state_->find(body) : nullptr;
	if (!slot)
		return rejected("Stale, foreign or unavailable physics body");
	return body2d_snapshot{body,
	                       slot->motion,
	                       engine(b2Body_GetPosition(slot->body)),
	                       b2Rot_GetAngle(b2Body_GetRotation(slot->body)),
	                       engine(b2Body_GetLinearVelocity(slot->body)),
	                       b2Body_GetAngularVelocity(slot->body),
	                       slot->motion == motion_type::dynamic && !b2Body_IsAwake(slot->body)};
}

ice::expected<void, diagnostic> world2d::set_pose(body_id body, const math::vec2 &position,
                                                  float rotation, bool reset_velocity) {
	auto *slot = state_ ? state_->find(body) : nullptr;
	if (!slot || state_->faulted)
		return rejected("Stale, foreign, unavailable or faulted physics body");
	if (!bounded(position, 1000000) || !std::isfinite(rotation))
		return rejected("Invalid physics pose");
	b2Body_SetTransform(slot->body, native(position), native_rotation(rotation));
	if (reset_velocity) {
		b2Body_SetLinearVelocity(slot->body, {0, 0});
		b2Body_SetAngularVelocity(slot->body, 0);
	}
	if (slot->motion == motion_type::dynamic)
		b2Body_SetAwake(slot->body, true);
	return {};
}

ice::expected<void, diagnostic> world2d::apply_impulse(body_id body, const math::vec2 &impulse) {
	auto *slot = state_ ? state_->find(body) : nullptr;
	if (!slot || state_->faulted)
		return rejected("Stale, foreign, unavailable or faulted physics body");
	if (slot->motion != motion_type::dynamic)
		return rejected("Only dynamic bodies accept impulses");
	if (!bounded(impulse, 100000000))
		return rejected("Invalid or excessive physics impulse");
	auto         velocity = b2Body_GetLinearVelocity(slot->body);
	const double mass     = b2Body_GetMass(slot->body);
	if (std::abs(double(velocity.x) + double(impulse.x) / mass) > 10000 ||
	    std::abs(double(velocity.y) + double(impulse.y) / mass) > 10000)
		return rejected("Impulse would exceed the configured body speed budget");
	b2Body_ApplyLinearImpulseToCenter(slot->body, native(impulse), true);
	return {};
}

ice::expected<step_report, diagnostic> world2d::step() {
	if (!state_ || state_->faulted)
		return rejected("Physics world is unavailable or faulted");
	auto &state = *state_;
	if (state.tick == std::numeric_limits<std::uint64_t>::max())
		return rejected("Physics tick space is exhausted");
	state.events.clear();
	std::sort(state.retired.begin(), state.retired.end(),
	          [](const auto &a, const auto &b) { return a.native_id < b.native_id; });
	b2World_Step(state.world, state.description.time_step, int(state.description.substeps));
	bool       truncated = false;
	const auto contacts  = b2World_GetContactEvents(state.world);
	const auto sensors   = b2World_GetSensorEvents(state.world);
	for (int index = 0; index < contacts.endCount; ++index)
		state.append(contact_phase::end, contacts.endEvents[index].shapeIdA,
		             contacts.endEvents[index].shapeIdB, false, truncated);
	for (int index = 0; index < sensors.endCount; ++index)
		state.append(contact_phase::end, sensors.endEvents[index].sensorShapeId,
		             sensors.endEvents[index].visitorShapeId, true, truncated);
	for (int index = 0; index < contacts.beginCount; ++index)
		state.append(contact_phase::begin, contacts.beginEvents[index].shapeIdA,
		             contacts.beginEvents[index].shapeIdB, false, truncated);
	for (int index = 0; index < sensors.beginCount; ++index)
		state.append(contact_phase::begin, sensors.beginEvents[index].sensorShapeId,
		             sensors.beginEvents[index].visitorShapeId, true, truncated);
	state.retired.clear();
	for (auto &slot : state.slots) {
		if (!slot.live)
			continue;
		slot.stepped        = true;
		const auto position = b2Body_GetPosition(slot.body),
		           velocity = b2Body_GetLinearVelocity(slot.body);
		if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
		    !std::isfinite(velocity.x) || !std::isfinite(velocity.y) ||
		    !std::isfinite(b2Body_GetAngularVelocity(slot.body))) {
			state.faulted = true;
			return rejected(
			    "Native physics produced nonfinite state; further stepping is disabled");
		}
	}
	return step_report{++state.tick, truncated};
}

std::span<const contact_event> world2d::contacts() const {
	return state_ ? std::span<const contact_event>(state_->events)
	              : std::span<const contact_event>{};
}

ice::expected<std::optional<ray_hit2d>, diagnostic>
world2d::ray_cast(const math::vec2 &origin, const math::vec2 &displacement) const {
	if (!state_)
		return rejected("Physics world is unavailable");
	if (!bounded(origin, 1000000) || !bounded(displacement, 1000000) ||
	    glm::dot(displacement, displacement) < 1e-12f || !bounded(origin + displacement, 1000000))
		return rejected("Invalid or zero physics ray");
	implementation::ray_context query{*state_, std::nullopt};
	b2World_CastRay(state_->world, native(origin), native(displacement), b2DefaultQueryFilter(),
	                implementation::ray_hit, &query);
	return query.hit;
}

std::uint64_t world2d::identity() const { return state_ ? state_->identity : 0; }
std::uint64_t world2d::tick() const { return state_ ? state_->tick : 0; }
float         world2d::time_step() const { return state_ ? state_->description.time_step : 0; }
std::uint32_t world2d::body_count() const { return state_ ? state_->body_count : 0; }
} // namespace ice::physics
