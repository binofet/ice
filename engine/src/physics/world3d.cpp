#include "ice/physics/world3d.h"
#include "internal.h"

// Jolt requires its umbrella header before every other Jolt header.
#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace ice::physics {
namespace {
bool finite(const math::vec3 &value, float bound = 1.0e6f) {
	return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) &&
	       std::abs(value.x) <= bound && std::abs(value.y) <= bound && std::abs(value.z) <= bound;
}
bool orientation(const math::quat &value) {
	return std::isfinite(value.w) && finite(math::vec3(value.x, value.y, value.z)) &&
	       std::abs(value.w) <= 1.0e6f && glm::dot(value, value) > 1.0e-12f;
}
JPH::Vec3 native(const math::vec3 &value) { return {value.x, value.y, value.z}; }
JPH::Quat native(const math::quat &value) {
	auto unit = glm::normalize(value);
	return {unit.x, unit.y, unit.z, unit.w};
}
math::vec3 portable(JPH::Vec3Arg value) { return {value.GetX(), value.GetY(), value.GetZ()}; }
math::quat portable(JPH::QuatArg value) {
	return {value.GetW(), value.GetX(), value.GetY(), value.GetZ()};
}
struct backend_runtime {
	backend_runtime() {
		// One module-owned registration lifetime, shared by all independent ICE worlds.
		if (JPH::Factory::sInstance)
			throw std::runtime_error("Jolt was initialized outside the ICE physics module");
		JPH::RegisterDefaultAllocator();
		JPH::Factory::sInstance = new JPH::Factory();
		JPH::RegisterTypes();
	}
	~backend_runtime() {
		JPH::UnregisterTypes();
		delete JPH::Factory::sInstance;
		JPH::Factory::sInstance = nullptr;
	}
};
void initialize_backend() {
	static backend_runtime runtime;
	(void)runtime;
}
struct broad_layers final : JPH::BroadPhaseLayerInterface {
	JPH::uint            GetNumBroadPhaseLayers() const override { return 2; }
	JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
		return JPH::BroadPhaseLayer(static_cast<JPH::BroadPhaseLayer::Type>(layer));
	}
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
		return layer.GetValue() ? "moving" : "static";
	}
#endif
};
struct layer_pairs final : JPH::ObjectLayerPairFilter {
	bool ShouldCollide(JPH::ObjectLayer first, JPH::ObjectLayer second) const override {
		return first != 0 || second != 0;
	}
};
struct broad_pairs final : JPH::ObjectVsBroadPhaseLayerFilter {
	bool ShouldCollide(JPH::ObjectLayer first, JPH::BroadPhaseLayer second) const override {
		return first != 0 || second.GetValue() != 0;
	}
};
struct nonsensors final : JPH::BodyFilter {
	bool ShouldCollideLocked(const JPH::Body &body) const override { return !body.IsSensor(); }
};
JPH::EMotionType native(motion_type motion) {
	switch (motion) {
	case motion_type::static_body:
		return JPH::EMotionType::Static;
	case motion_type::kinematic:
		return JPH::EMotionType::Kinematic;
	case motion_type::dynamic:
		return JPH::EMotionType::Dynamic;
	}
	return JPH::EMotionType::Static;
}
bool valid(motion_type motion) {
	return motion == motion_type::static_body || motion == motion_type::kinematic ||
	       motion == motion_type::dynamic;
}
ice::expected<JPH::RefConst<JPH::Shape>, diagnostic> prepare(const shape3d_description &shape) {
	JPH::ShapeSettings::ShapeResult result;
	switch (shape.kind) {
	case shape3d_kind::sphere:
		if (!std::isfinite(shape.radius) || shape.radius < .001f || shape.radius > 10000)
			return ice::unexpected(diagnostic{"Sphere radius must be in [0.001,10000] meters"});
		result = JPH::SphereShapeSettings(shape.radius).Create();
		break;
	case shape3d_kind::box: {
		if (!finite(shape.half_extent, 10000) ||
		    glm::any(glm::lessThan(shape.half_extent, math::vec3(.001f))))
			return ice::unexpected(diagnostic{"Box half extents must be in [0.001,10000] meters"});
		float radius = std::min({.05f, shape.half_extent.x * .1f, shape.half_extent.y * .1f,
		                         shape.half_extent.z * .1f});
		result       = JPH::BoxShapeSettings(native(shape.half_extent), radius).Create();
		break;
	}
	case shape3d_kind::capsule:
		if (!std::isfinite(shape.radius) || shape.radius < .001f || shape.radius > 10000 ||
		    !std::isfinite(shape.half_height) || shape.half_height < 0 || shape.half_height > 10000)
			return ice::unexpected(diagnostic{"Invalid capsule radius or cylinder half height"});
		// A zero cylinder length is a sphere; Jolt's capsule requires a positive half height.
		result = shape.half_height == 0
		             ? JPH::SphereShapeSettings(shape.radius).Create()
		             : JPH::CapsuleShapeSettings(shape.half_height, shape.radius).Create();
		break;
	default:
		return ice::unexpected(diagnostic{"Unknown 3D collision shape"});
	}
	if (result.HasError())
		return ice::unexpected(diagnostic{result.GetError().c_str()});
	if (!result.IsValid())
		return ice::unexpected(diagnostic{"Jolt did not prepare a collision shape"});
	return JPH::RefConst<JPH::Shape>(result.Get());
}
// Contact callbacks cannot allocate or inspect a body after its native destruction.
// Retain engine identities in a fixed-capacity hash table keyed by the complete native pair.
struct contact_cache {
	struct entry {
		JPH::SubShapeIDPair key;
		contact_event       event;
		unsigned char       state = 0; // empty / live / retired
	};
	std::vector<entry> entries, scratch;
	std::size_t        retired = 0;
	explicit contact_cache(std::uint32_t maximum) {
		auto capacity = std::bit_ceil(std::size_t(maximum) * 2);
		entries.resize(capacity);
		scratch.resize(capacity);
	}
	entry *find(const JPH::SubShapeIDPair &key, bool insert) {
		auto   index  = std::size_t(key.GetHash()) & (entries.size() - 1);
		entry *vacant = nullptr;
		for (std::size_t count = 0; count < entries.size(); ++count) {
			auto &slot = entries[index];
			if (slot.state == 1 && slot.key == key)
				return &slot;
			if (slot.state != 1 && !vacant)
				vacant = &slot;
			if (slot.state == 0)
				break;
			index = (index + 1) & (entries.size() - 1);
		}
		if (!insert || !vacant)
			return nullptr;
		if (vacant->state == 2)
			--retired;
		vacant->key   = key;
		vacant->state = 1;
		return vacant;
	}
	void compact() {
		if (retired <= entries.size() / 4)
			return;
		for (auto &slot : scratch)
			slot.state = 0;
		entries.swap(scratch);
		retired = 0;
		for (const auto &slot : scratch)
			if (slot.state == 1)
				*find(slot.key, true) = slot;
	}
};
} // namespace

struct world3d::implementation final : JPH::ContactListener {
	struct slot {
		JPH::BodyID   native;
		std::uint64_t generation = 1;
		motion_type   motion     = motion_type::static_body;
		bool          alive      = false;
		bool          stepped    = false;
	};
	world3d_description description;
	std::uint64_t       owner = 0, ticks = 0;
	std::uint32_t       count = 0;
	bool faulted = false, stepping = false, truncated = false, pending_truncated = false;
	std::vector<slot>                        slots;
	std::vector<std::uint32_t>               free;
	std::vector<contact_event>               events, pending;
	std::vector<JPH::BodyID>                 retired_bodies;
	contact_cache                            cache;
	broad_layers                             layers;
	layer_pairs                              pairs;
	broad_pairs                              broad;
	JPH::TempAllocatorImplWithMallocFallback temporary;
	JPH::JobSystemSingleThreaded             jobs;
	JPH::PhysicsSystem                       system;
	explicit implementation(world3d_description settings)
	    : description(settings), cache(settings.max_contact_events), temporary(4 * 1024 * 1024),
	      jobs(2048) {
		owner = detail::next_world_identity();
		slots.resize(settings.max_bodies);
		free.reserve(settings.max_bodies);
		for (auto i = settings.max_bodies; i > 0; --i)
			free.push_back(i - 1);
		events.reserve(settings.max_contact_events);
		pending.reserve(settings.max_contact_events);
		retired_bodies.reserve(settings.max_bodies * 2);
		auto constraints =
		    std::max<std::uint32_t>(settings.max_contact_events, settings.max_bodies * 4);
		// A removed body which participated in a step retains its native ID until the next
		// successful update retires Jolt's cached manifolds. Native IDs have only eight sequence
		// bits: immediate reuse can alias a still-cached contact after 256 authoring edits.
		// At most max_bodies were stepped; later, unstepped allocations can retire immediately.
		system.Init(settings.max_bodies * 2, 0, constraints * 2, constraints, layers, broad, pairs);
		system.SetGravity(native(settings.gravity));
		system.SetContactListener(this);
	}
	~implementation() override {
		system.SetContactListener(nullptr);
		auto &bodies = system.GetBodyInterface();
		for (auto &entry : slots)
			if (entry.alive) {
				bodies.RemoveBody(entry.native);
				bodies.DestroyBody(entry.native);
			}
		for (auto body : retired_bodies)
			bodies.DestroyBody(body);
	}
	ice::expected<std::size_t, diagnostic> resolve(body_id id) const {
		if (id.owner != owner || !id.slot || id.slot > slots.size())
			return ice::unexpected(diagnostic{"Foreign or invalid body identity"});
		auto index = std::size_t(id.slot - 1);
		if (!slots[index].alive || slots[index].generation != id.generation)
			return ice::unexpected(diagnostic{"Stale body identity"});
		return index;
	}
	body_id identify(const JPH::Body &body) const {
		auto index = body.GetUserData();
		if (index >= slots.size() || !slots[index].alive || slots[index].native != body.GetID())
			return {};
		return {owner, std::uint32_t(index + 1), slots[index].generation};
	}
	void emit(contact_event event) {
		auto &output = stepping ? events : pending;
		if (output.size() == description.max_contact_events) {
			(stepping ? truncated : pending_truncated) = true;
			return;
		}
		output.push_back(event);
	}
	void OnContactAdded(const JPH::Body &first, const JPH::Body &second,
	                    const JPH::ContactManifold &manifold,
	                    JPH::ContactSettings       &settings) override {
		JPH::SubShapeIDPair key(first.GetID(), manifold.mSubShapeID1, second.GetID(),
		                        manifold.mSubShapeID2);
		auto               *entry = cache.find(key, true);
		if (!entry) {
			(stepping ? truncated : pending_truncated) = true;
			return;
		}
		entry->event = {contact_phase::begin, identify(first), identify(second),
		                settings.mIsSensor};
		emit(entry->event);
	}
	void OnContactRemoved(const JPH::SubShapeIDPair &key) override {
		auto *entry = cache.find(key, false);
		if (!entry)
			return;
		auto event  = entry->event;
		event.phase = contact_phase::end;
		emit(event);
		entry->state = 2;
		++cache.retired;
	}
};
world3d::world3d(std::unique_ptr<implementation> state) : state_(std::move(state)) {}
world3d::~world3d()                                                             = default;
world3d::world3d(world3d &&other) noexcept                                      = default;
world3d                           &world3d::operator=(world3d &&other) noexcept = default;
ice::expected<world3d, diagnostic> world3d::create(const world3d_description &description) {
	if (!finite(description.gravity, 10000) || !std::isfinite(description.time_step) ||
	    description.time_step < .001f || description.time_step > 1.0f / 15 ||
	    !description.substeps || description.substeps > 16 || !description.max_bodies ||
	    description.max_bodies > 65536 || !description.max_contact_events ||
	    description.max_contact_events > 65536)
		return ice::unexpected(
		    diagnostic{"Invalid fixed-step 3D world settings or resource budgets"});
	try {
		initialize_backend();
	} catch (const std::exception &error) {
		return ice::unexpected(diagnostic{error.what()});
	}
	auto state = std::make_unique<implementation>(description);
	if (!state->owner)
		return ice::unexpected(diagnostic{"World lifetime identity exhausted"});
	return world3d(std::move(state));
}
ice::expected<body_id, diagnostic> world3d::create_body(const body3d_description &description) {
	if (!state_ || state_->faulted)
		return ice::unexpected(diagnostic{"3D world is unavailable/faulted"});
	auto &state = *state_;
	if (!valid(description.motion) || !finite(description.position) ||
	    !orientation(description.rotation) || !finite(description.linear_velocity, 10000) ||
	    !finite(description.angular_velocity, 10000) || !std::isfinite(description.mass) ||
	    description.mass < .001f || description.mass > 1.0e9f ||
	    !std::isfinite(description.friction) || description.friction < 0 ||
	    description.friction > 10 || !std::isfinite(description.restitution) ||
	    description.restitution < 0 || description.restitution > 1)
		return ice::unexpected(diagnostic{"Invalid body pose, velocity, mass or material"});
	if (state.free.empty())
		return ice::unexpected(diagnostic{"3D body budget exhausted"});
	auto shape = prepare(description.shape);
	if (!shape)
		return ice::unexpected(shape.error());
	JPH::BodyCreationSettings settings(shape->GetPtr(), native(description.position),
	                                   native(description.rotation), native(description.motion),
	                                   description.motion == motion_type::static_body ? 0 : 1);
	settings.mLinearVelocity               = native(description.linear_velocity);
	settings.mAngularVelocity              = native(description.angular_velocity);
	settings.mFriction                     = description.friction;
	settings.mRestitution                  = description.restitution;
	settings.mIsSensor                     = description.sensor;
	settings.mOverrideMassProperties       = JPH::EOverrideMassProperties::CalculateInertia;
	settings.mMassPropertiesOverride.mMass = description.mass;
	settings.mMotionQuality =
	    description.continuous ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;
	// Preserve authored velocities inside the accepted range, rather than Jolt's default speed cap.
	settings.mMaxLinearVelocity  = 20000;
	settings.mMaxAngularVelocity = 20000;
	auto index                   = state.free.back();
	settings.mUserData           = index;
	auto &bodies                 = state.system.GetBodyInterface();
	auto *body                   = bodies.CreateBody(settings);
	if (!body)
		return ice::unexpected(diagnostic{"Jolt could not allocate a native body"});
	state.free.pop_back();
	auto &entry   = state.slots[index];
	entry.native  = body->GetID();
	entry.alive   = true;
	entry.stepped = false;
	entry.motion  = description.motion;
	bodies.AddBody(entry.native, description.motion == motion_type::static_body
	                                 ? JPH::EActivation::DontActivate
	                                 : JPH::EActivation::Activate);
	++state.count;
	return body_id{state.owner, index + 1, entry.generation};
}
ice::expected<void, diagnostic> world3d::erase(body_id body) {
	if (!state_)
		return ice::unexpected(diagnostic{"3D world is unavailable"});
	auto resolved = state_->resolve(body);
	if (!resolved)
		return ice::unexpected(resolved.error());
	auto &entry  = state_->slots[*resolved];
	auto &bodies = state_->system.GetBodyInterface();
	bodies.RemoveBody(entry.native);
	if (entry.stepped)
		state_->retired_bodies.push_back(entry.native);
	else
		bodies.DestroyBody(entry.native);
	entry.alive = false;
	--state_->count;
	if (entry.generation != std::numeric_limits<std::uint64_t>::max()) {
		++entry.generation;
		state_->free.push_back(std::uint32_t(*resolved));
	}
	return {};
}
ice::expected<body3d_snapshot, diagnostic> world3d::inspect(body_id body) const {
	if (!state_)
		return ice::unexpected(diagnostic{"3D world is unavailable"});
	auto resolved = state_->resolve(body);
	if (!resolved)
		return ice::unexpected(resolved.error());
	const auto &entry  = state_->slots[*resolved];
	const auto &bodies = state_->system.GetBodyInterface();
	JPH::RVec3  position;
	JPH::Quat   rotation;
	bodies.GetPositionAndRotation(entry.native, position, rotation);
	JPH::Vec3 linear, angular;
	bodies.GetLinearAndAngularVelocity(entry.native, linear, angular);
	return body3d_snapshot{body,
	                       entry.motion,
	                       portable(position),
	                       portable(rotation),
	                       portable(linear),
	                       portable(angular),
	                       entry.motion == motion_type::dynamic && !bodies.IsActive(entry.native)};
}
ice::expected<void, diagnostic> world3d::set_pose(body_id body, const math::vec3 &position,
                                                  const math::quat &rotation, bool reset_velocity) {
	if (!state_ || state_->faulted)
		return ice::unexpected(diagnostic{"3D world is unavailable/faulted"});
	auto resolved = state_->resolve(body);
	if (!resolved)
		return ice::unexpected(resolved.error());
	if (!finite(position) || !orientation(rotation))
		return ice::unexpected(diagnostic{"Invalid 3D placement pose"});
	const auto &entry  = state_->slots[*resolved];
	auto       &bodies = state_->system.GetBodyInterface();
	bodies.SetPositionAndRotation(entry.native, native(position), native(rotation),
	                              entry.motion == motion_type::static_body
	                                  ? JPH::EActivation::DontActivate
	                                  : JPH::EActivation::Activate);
	if (reset_velocity && entry.motion != motion_type::static_body)
		bodies.SetLinearAndAngularVelocity(entry.native, JPH::Vec3::sZero(), JPH::Vec3::sZero());
	return {};
}
ice::expected<void, diagnostic> world3d::apply_impulse(body_id body, const math::vec3 &impulse) {
	if (!state_ || state_->faulted)
		return ice::unexpected(diagnostic{"3D world is unavailable/faulted"});
	auto resolved = state_->resolve(body);
	if (!resolved)
		return ice::unexpected(resolved.error());
	if (state_->slots[*resolved].motion != motion_type::dynamic || !finite(impulse, 1.0e6f))
		return ice::unexpected(
		    diagnostic{"Impulse requires a dynamic body and finite bounded vector"});
	state_->system.GetBodyInterface().AddImpulse(state_->slots[*resolved].native, native(impulse));
	return {};
}
ice::expected<step_report, diagnostic> world3d::step() {
	if (!state_ || state_->faulted)
		return ice::unexpected(diagnostic{"3D world is unavailable/faulted"});
	auto &state = *state_;
	if (state.ticks == std::numeric_limits<std::uint64_t>::max())
		return ice::unexpected(diagnostic{"Simulation tick exhausted"});
	state.events.clear();
	state.events.swap(state.pending);
	state.truncated         = state.pending_truncated;
	state.pending_truncated = false;
	state.cache.compact();
	for (auto &entry : state.slots)
		if (entry.alive)
			entry.stepped = true;
	state.stepping = true;
	auto error = state.system.Update(state.description.time_step, int(state.description.substeps),
	                                 &state.temporary, &state.jobs);
	state.stepping = false;
	if (error != JPH::EPhysicsUpdateError::None) {
		state.faulted = true;
		return ice::unexpected(
		    diagnostic{"Jolt step exhausted native constraints; world requires recovery"});
	}
	for (auto body : state.retired_bodies)
		state.system.GetBodyInterface().DestroyBody(body);
	state.retired_bodies.clear();
	const auto &bodies = state.system.GetBodyInterface();
	for (const auto &entry : state.slots) {
		if (!entry.alive)
			continue;
		JPH::RVec3 position;
		JPH::Quat  rotation;
		JPH::Vec3  linear, angular;
		bodies.GetPositionAndRotation(entry.native, position, rotation);
		bodies.GetLinearAndAngularVelocity(entry.native, linear, angular);
		const auto maximum = std::numeric_limits<float>::max();
		if (!finite(portable(position), maximum) || !orientation(portable(rotation)) ||
		    !finite(portable(linear), maximum) || !finite(portable(angular), maximum)) {
			state.faulted = true;
			return ice::unexpected(
			    diagnostic{"Jolt produced nonfinite body state; world requires recovery"});
		}
	}
	++state.ticks;
	return step_report{state.ticks, state.truncated};
}
std::span<const contact_event> world3d::contacts() const {
	return state_ ? std::span<const contact_event>(state_->events)
	              : std::span<const contact_event>{};
}
ice::expected<std::optional<ray_hit3d>, diagnostic>
world3d::ray_cast(const math::vec3 &origin, const math::vec3 &displacement) const {
	if (!state_)
		return ice::unexpected(diagnostic{"3D world is unavailable"});
	if (!finite(origin) || !finite(displacement) || glm::dot(displacement, displacement) < 1.0e-12f)
		return ice::unexpected(diagnostic{"Ray requires a finite origin and nonzero displacement"});
	JPH::RRayCast      ray(native(origin), native(displacement));
	JPH::RayCastResult hit;
	if (!state_->system.GetNarrowPhaseQuery().CastRay(ray, hit, {}, {}, nonsensors{}))
		return std::optional<ray_hit3d>{};
	JPH::BodyLockRead lock(state_->system.GetBodyLockInterface(), hit.mBodyID);
	if (!lock.Succeeded())
		return ice::unexpected(diagnostic{"Ray target retired during query"});
	const auto &body   = lock.GetBody();
	auto        point  = ray.GetPointOnRay(hit.mFraction);
	auto        normal = body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, point);
	return std::optional<ray_hit3d>{
	    ray_hit3d{state_->identify(body), portable(point), portable(normal), hit.mFraction}};
}
std::uint64_t world3d::identity() const { return state_ ? state_->owner : 0; }
std::uint64_t world3d::tick() const { return state_ ? state_->ticks : 0; }
float         world3d::time_step() const { return state_ ? state_->description.time_step : 0; }
std::uint32_t world3d::body_count() const { return state_ ? state_->count : 0; }
} // namespace ice::physics
