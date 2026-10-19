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
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/PlaneShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/StateRecorder.h>
#include <Jolt/RegisterTypes.h>
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace ice::physics {
namespace {
constexpr JPH::ObjectLayer inactive_character_layer = 2;
// Analytic planes need a conservative finite broadphase bound. This covers the ±1e6 input
// domain even with translated/rotated body frames and bounded ray endpoints, with margin.
constexpr float plane_half_extent = 8000000.f;
bool            finite(const math::vec3 &value, float bound = 1.0e6f) {
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
bool valid_static_pose(const JPH::Shape &shape, const math::vec3 &position,
                       const math::quat &rotation) {
	const auto       bounds = shape.GetLocalBounds();
	const glm::dquat unit(glm::normalize(rotation));
	for (unsigned corner = 0; corner < 8; ++corner) {
		const glm::dvec3 local((corner & 1) ? bounds.mMax.GetX() : bounds.mMin.GetX(),
		                       (corner & 2) ? bounds.mMax.GetY() : bounds.mMin.GetY(),
		                       (corner & 4) ? bounds.mMax.GetZ() : bounds.mMin.GetZ());
		const auto world = glm::dvec3(position) + unit * local;
		if (glm::any(glm::greaterThan(glm::abs(world), glm::dvec3(1.0e6))))
			return false;
	}
	return true;
}
bool equivalent_rotation(const JPH::Quat &first, const JPH::Quat &second) {
	const auto a = portable(first), b = portable(second);
	return std::min(glm::length(a - b), glm::length(a + b)) <=
	       4 * std::numeric_limits<float>::epsilon();
}
bool static_pose_profile(const JPH::Body &body) {
	if (!body.IsStatic() || body.IsSensor() || body.GetMotionPropertiesUnchecked())
		return false;
	// Only actor primitives/rest-bounds meshes in this first proof. Terrain/planes, compounds,
	// dynamic/kinematic motion and character proxies do not enter the fast publication seam.
	const auto type = body.GetShape()->GetSubType();
	return type == JPH::EShapeSubType::Box || type == JPH::EShapeSubType::Sphere ||
	       type == JPH::EShapeSubType::Capsule || type == JPH::EShapeSubType::Mesh;
}
// Pinned Jolt static SaveBodyState writes active, COM and rotation only. A fixed recorder
// avoids stringstream allocation and preserves exact COM bytes during prefix rollback.
struct static_pose_record final : JPH::StateRecorder {
	static constexpr std::size_t    capacity = 128;
	std::array<std::byte, capacity> bytes{};
	std::size_t                     size = 0, cursor = 0;
	bool                            failed = false;
	void                            WriteBytes(const void *data, std::size_t count) override {
        if (count > capacity - size) {
            failed = true;
            return;
        }
        std::memcpy(bytes.data() + size, data, count);
        size += count;
	}
	void ReadBytes(void *data, std::size_t count) override {
		if (count > size - cursor) {
			failed = true;
			return;
		}
		std::memcpy(data, bytes.data() + cursor, count);
		cursor += count;
	}
	bool IsEOF() const override { return cursor == size; }
	bool IsFailed() const override { return failed; }
	void rewind() noexcept { cursor = 0; }
	bool validate_static_payload() {
		bool       active = true;
		JPH::RVec3 position;
		JPH::Quat  rotation;
		Read(active);
		Read(position);
		Read(rotation);
		const bool valid = !failed && IsEOF() && !active;
		rewind();
		return valid;
	}
};
struct backend_runtime {
	JPH::RefConst<JPH::PhysicsMaterial> default_material;
	backend_runtime() {
		// One module-owned registration lifetime, shared by all independent ICE worlds.
		if (JPH::Factory::sInstance)
			throw std::runtime_error("Jolt was initialized outside the ICE physics module");
		JPH::RegisterDefaultAllocator();
		JPH::Factory::sInstance = new JPH::Factory();
		JPH::RegisterTypes();
		default_material = JPH::PhysicsMaterial::sDefault;
	}
	~backend_runtime() {
		// Global cooked owners can retire after Jolt's sDefault RefConst destructor.
		// UnregisterTypes assigns into that global, so split its two cleanup actions:
		// clear the registry here, and let each material reference release naturally.
		// Our independent reference keeps material storage alive in either exit order.
		JPH::Factory::sInstance->Clear();
		delete JPH::Factory::sInstance;
		JPH::Factory::sInstance = nullptr;
	}
};
std::shared_ptr<backend_runtime> initialize_backend() {
	// A cooked resource or world may outlive the function-static registration owner during
	// process teardown. Their explicit references keep the allocator/registry alive until
	// their last native shape, material and physics system have been destroyed.
	static const auto runtime = std::make_shared<backend_runtime>();
	return runtime;
}
struct broad_layers final : JPH::BroadPhaseLayerInterface {
	JPH::uint            GetNumBroadPhaseLayers() const override { return 2; }
	JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
		return JPH::BroadPhaseLayer(
		    static_cast<JPH::BroadPhaseLayer::Type>(layer == inactive_character_layer ? 0 : layer));
	}
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
		return layer.GetValue() ? "moving" : "static";
	}
#endif
};
struct layer_pairs final : JPH::ObjectLayerPairFilter {
	bool ShouldCollide(JPH::ObjectLayer first, JPH::ObjectLayer second) const override {
		return first != inactive_character_layer && second != inactive_character_layer &&
		       (first != 0 || second != 0);
	}
};
struct broad_pairs final : JPH::ObjectVsBroadPhaseLayerFilter {
	bool ShouldCollide(JPH::ObjectLayer first, JPH::BroadPhaseLayer second) const override {
		return first != inactive_character_layer && (first != 0 || second.GetValue() != 0);
	}
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
// Keep Jolt's original Ref owner in its movable Result until native settings retain it.
// Avoid an extra RefConst copy whose atomic reference count Clang 18's analyzer cannot track.
ice::expected<JPH::ShapeSettings::ShapeResult, diagnostic>
prepare(const shape3d_description &shape) {
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
	case shape3d_kind::plane:
		result =
		    JPH::PlaneShapeSettings(JPH::Plane(JPH::Vec3::sAxisY(), 0), nullptr, plane_half_extent)
		        .Create();
		break;
	default:
		return ice::unexpected(diagnostic{"Unknown 3D collision shape"});
	}
	if (result.HasError())
		return ice::unexpected(diagnostic{result.GetError().c_str()});
	if (!result.IsValid())
		return ice::unexpected(diagnostic{"Jolt did not prepare a collision shape"});
	return result;
}
bool valid(const character3d_description &description) {
	auto range = [](float value, float minimum, float maximum) {
		return std::isfinite(value) && value >= minimum && value <= maximum;
	};
	return finite(description.position) && finite(description.linear_velocity, 10000) &&
	       range(description.radius, .01f, 10000) &&
	       range(description.height, description.radius * 2, 20000) &&
	       range(description.max_slope_angle, glm::radians(1.f), glm::radians(89.f)) &&
	       range(description.step_height, 0, description.height) &&
	       range(description.support_distance, 0, description.height) &&
	       range(description.jump_speed, 0, 10000) && range(description.mass, .001f, 1.0e6f) &&
	       range(description.max_push_force, 0, 1.0e9f) && description.max_contacts &&
	       description.max_contacts <= 65536;
}
ice::expected<JPH::ShapeSettings::ShapeResult, diagnostic>
prepare_character_shape(const character3d_description &description, float scale) {
	shape3d_description geometry;
	geometry.kind        = shape3d_kind::capsule;
	geometry.radius      = description.radius * scale;
	geometry.half_height = (description.height * .5f - description.radius) * scale;
	auto shape           = prepare(geometry);
	if (!shape)
		return ice::unexpected(shape.error());
	auto result = JPH::RotatedTranslatedShapeSettings(JPH::Vec3(0, description.height * .5f, 0),
	                                                  JPH::Quat::sIdentity(), shape->Get().GetPtr())
	                  .Create();
	if (result.HasError())
		return ice::unexpected(diagnostic{result.GetError().c_str()});
	if (!result.IsValid())
		return ice::unexpected(diagnostic{"Jolt did not prepare a character shape"});
	return result;
}
character_ground_state portable(JPH::CharacterBase::EGroundState state) {
	switch (state) {
	case JPH::CharacterBase::EGroundState::OnGround:
		return character_ground_state::on_ground;
	case JPH::CharacterBase::EGroundState::OnSteepGround:
		return character_ground_state::on_steep_ground;
	case JPH::CharacterBase::EGroundState::NotSupported:
		return character_ground_state::not_supported;
	case JPH::CharacterBase::EGroundState::InAir:
		return character_ground_state::in_air;
	}
	return character_ground_state::in_air;
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

struct prepared_pose_batch::implementation {
	struct entry {
		body_id            body;
		JPH::BodyID        native_id;
		JPH::RVec3         requested_origin = JPH::RVec3::sZero();
		JPH::RVec3         accepted_com     = JPH::RVec3::sZero();
		JPH::Quat          rotation         = JPH::Quat::sIdentity();
		JPH::AABox         old_bounds, new_bounds;
		static_pose_record before;
		std::size_t        lock_index = 0;
		bool               changed    = false;
	};
	std::uint64_t                   owner = 0, epoch = 0;
	std::size_t                     changed_count = 0;
	std::vector<entry>              entries;
	std::vector<body3d_pose_update> previews;
	// Immutable lock/journal indices never go through Jolt's mutable broadphase ID sort.
	std::vector<JPH::BodyID> lock_ids, wake_ids;
	std::vector<std::size_t> wake_lock_indices, dirty_characters;
	diagnostic               rollback_error{"Static pose batch was rolled back"};
};
prepared_pose_batch::prepared_pose_batch()                                           = default;
prepared_pose_batch::~prepared_pose_batch()                                          = default;
prepared_pose_batch::prepared_pose_batch(prepared_pose_batch &&) noexcept            = default;
prepared_pose_batch &prepared_pose_batch::operator=(prepared_pose_batch &&) noexcept = default;
std::span<const body3d_pose_update> prepared_pose_batch::poses() const noexcept {
	return state_ ? std::span<const body3d_pose_update>(state_->previews)
	              : std::span<const body3d_pose_update>{};
}

struct cooked_static_shape::implementation {
	// Declaration order matters: registration is released after the native shape.
	std::shared_ptr<backend_runtime> runtime;
	JPH::RefConst<JPH::Shape>        shape;
	static_shape3d_statistics        statistics;
	std::uint64_t                    identity = 0;
};
cooked_static_shape::cooked_static_shape(std::shared_ptr<const implementation> state)
    : state_(std::move(state)) {}
cooked_static_shape::operator bool() const { return bool(state_); }
std::uint64_t cooked_static_shape::identity() const { return state_ ? state_->identity : 0; }
static_shape3d_statistics cooked_static_shape::statistics() const {
	return state_ ? state_->statistics : static_shape3d_statistics{};
}
ice::expected<cooked_static_shape, diagnostic>
cooked_static_shape::cook_mesh(const triangle_mesh3d_description &description) {
	// Jolt uses signed indices in mesh edge construction and signed triangle loops. Keep
	// intermediate triangle/edge counts representable before copying or indexing source data.
	const auto triangles = description.indices.size() / 3;
	if (description.vertices.empty() || description.indices.empty() ||
	    description.indices.size() % 3 ||
	    description.vertices.size() > std::size_t(std::numeric_limits<std::int32_t>::max()) ||
	    triangles > std::size_t(std::numeric_limits<std::int32_t>::max()) / 2)
		return ice::unexpected(diagnostic{"Invalid static mesh vertex/index counts"});
	for (const auto &vertex : description.vertices)
		if (!finite(vertex))
			return ice::unexpected(
			    diagnostic{"Static mesh vertices must be finite and within +/-1e6 meters"});
	for (auto index : description.indices)
		if (index >= description.vertices.size())
			return ice::unexpected(diagnostic{"Static mesh index is outside the vertex span"});
	for (std::size_t index = 0; index < description.indices.size(); index += 3) {
		const glm::dvec3 a(description.vertices[description.indices[index]]);
		const glm::dvec3 b(description.vertices[description.indices[index + 1]]);
		const glm::dvec3 c(description.vertices[description.indices[index + 2]]);
		const auto       normal = glm::cross(b - a, c - a);
		if (glm::dot(normal, normal) == 0)
			return ice::unexpected(diagnostic{"Static mesh contains a degenerate triangle"});
	}
	std::shared_ptr<backend_runtime> runtime;
	try {
		runtime = initialize_backend();
	} catch (const std::exception &error) {
		return ice::unexpected(diagnostic{error.what()});
	}
	const auto             started = std::chrono::steady_clock::now();
	JPH::MeshShapeSettings settings;
	settings.mTriangleVertices.reserve(description.vertices.size());
	for (const auto &vertex : description.vertices)
		settings.mTriangleVertices.emplace_back(vertex.x, vertex.y, vertex.z);
	settings.mIndexedTriangles.reserve(triangles);
	for (std::size_t index = 0; index < description.indices.size(); index += 3)
		settings.mIndexedTriangles.emplace_back(description.indices[index],
		                                        description.indices[index + 1],
		                                        description.indices[index + 2]);
	// The default constructor leaves source topology intact. Jolt still validates triangles
	// after its vertex quantization, and computes shared-edge activity for smooth contact.
	auto result = settings.Create();
	if (result.HasError())
		return ice::unexpected(diagnostic{result.GetError().c_str()});
	if (!result.IsValid())
		return ice::unexpected(diagnostic{"Jolt did not cook a static mesh"});
	auto state      = std::make_shared<implementation>();
	state->runtime  = std::move(runtime);
	state->shape    = result.Get();
	state->identity = detail::next_world_identity();
	if (!state->identity)
		return ice::unexpected(diagnostic{"Static collision lifetime identity exhausted"});
	state->statistics = {description.vertices.size(), triangles,
	                     state->shape->GetStats().mSizeBytes,
	                     std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
	                                       std::chrono::steady_clock::now() - started)
	                                       .count())};
	return cooked_static_shape(std::move(state));
}
ice::expected<cooked_static_shape, diagnostic>
cooked_static_shape::cook_heightfield(const heightfield3d_description &description) {
	const auto count  = std::uint64_t(description.sample_count);
	const auto padded = (count + 1) & ~std::uint64_t{1};
	// Jolt's sixteen-bit sample buffer uses 32-bit bit offsets, including seven rounding bits.
	// Check that arithmetic (and the two-dimensional source size) before touching its arrays.
	if (count < 4 || padded > 16382 || count * count != description.heights.size() ||
	    !finite(description.offset) || !finite(description.scale) ||
	    glm::any(glm::lessThan(description.scale, math::vec3(.001f))))
		return ice::unexpected(
		    diagnostic{"Invalid static heightfield dimensions, offset or scale"});
	const auto maximum_x =
	    double(description.offset.x) + double(description.scale.x) * double(count - 1);
	const auto maximum_z =
	    double(description.offset.z) + double(description.scale.z) * double(count - 1);
	if (maximum_x > 1.0e6 || maximum_z > 1.0e6)
		return ice::unexpected(
		    diagnostic{"Static heightfield extends outside +/-1e6 local meters"});
	for (float height : description.heights) {
		const auto y = double(description.offset.y) + double(description.scale.y) * double(height);
		if (!std::isfinite(height) || !std::isfinite(y) || std::abs(y) > 1.0e6)
			return ice::unexpected(diagnostic{
			    "Static heightfield heights must be finite and bounded; holes are unsupported"});
	}
	std::shared_ptr<backend_runtime> runtime;
	try {
		runtime = initialize_backend();
	} catch (const std::exception &error) {
		return ice::unexpected(diagnostic{error.what()});
	}
	const auto                    started = std::chrono::steady_clock::now();
	JPH::HeightFieldShapeSettings settings(description.heights.data(), native(description.offset),
	                                       native(description.scale), description.sample_count);
	settings.mBitsPerSample = 16;
	auto result             = settings.Create();
	if (result.HasError())
		return ice::unexpected(diagnostic{result.GetError().c_str()});
	if (!result.IsValid())
		return ice::unexpected(diagnostic{"Jolt did not cook a static heightfield"});
	auto state      = std::make_shared<implementation>();
	state->runtime  = std::move(runtime);
	state->shape    = result.Get();
	state->identity = detail::next_world_identity();
	if (!state->identity)
		return ice::unexpected(diagnostic{"Static collision lifetime identity exhausted"});
	state->statistics = {count * count, (count - 1) * (count - 1) * 2,
	                     state->shape->GetStats().mSizeBytes,
	                     std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
	                                       std::chrono::steady_clock::now() - started)
	                                       .count())};
	return cooked_static_shape(std::move(state));
}

struct world3d::implementation final : JPH::ContactListener {
	struct slot {
		JPH::BodyID   native;
		std::uint64_t generation            = 1;
		motion_type   motion                = motion_type::static_body;
		bool          alive                 = false;
		bool          stepped               = false;
		bool          character_proxy       = false;
		std::uint64_t static_shape_identity = 0;
	};
	struct character_slot {
		std::unique_ptr<JPH::CharacterVirtual> native;
		character3d_description                description;
		character3d_intent                     intent;
		body_id                                proxy;
		std::uint64_t                          generation   = 1;
		bool                                   jump_pending = false, ground_dirty = true;
	};
	std::shared_ptr<backend_runtime> runtime;
	world3d_description              description;
	std::uint64_t                    owner = 0, ticks = 0, epoch = 0;
	std::uint32_t                    count = 0, characters_count = 0;
	bool faulted = false, stepping = false, truncated = false, pending_truncated = false;
	bool applying_poses = false;
#ifdef ICE_PHYSICS_POSE_BATCH_TESTING
	std::size_t fail_after_writes = std::numeric_limits<std::size_t>::max();
#endif
	std::vector<slot>                                   slots;
	std::vector<std::uint32_t>                          free;
	std::vector<contact_event>                          events, pending;
	std::vector<JPH::BodyID>                            retired_bodies;
	std::vector<character_slot>                         characters;
	std::vector<std::uint32_t>                          free_characters;
	std::vector<std::unique_ptr<JPH::CharacterVirtual>> retired_characters;
	JPH::CharacterVsCharacterCollisionSimple            character_collisions;
	contact_cache                                       cache;
	broad_layers                                        layers;
	layer_pairs                                         pairs;
	broad_pairs                                         broad;
	JPH::TempAllocatorImplWithMallocFallback            temporary;
	JPH::JobSystemSingleThreaded                        jobs;
	JPH::PhysicsSystem                                  system;
	explicit implementation(world3d_description settings, std::shared_ptr<backend_runtime> backend)
	    : runtime(std::move(backend)), description(settings), cache(settings.max_contact_events),
	      temporary(4 * 1024 * 1024), jobs(2048) {
		owner = detail::next_world_identity();
		slots.resize(settings.max_bodies);
		free.reserve(settings.max_bodies);
		for (auto i = settings.max_bodies; i > 0; --i)
			free.push_back(i - 1);
		events.reserve(settings.max_contact_events);
		pending.reserve(settings.max_contact_events);
		retired_bodies.reserve(settings.max_bodies * 2);
		const auto character_capacity = std::min(settings.max_characters, settings.max_bodies);
		characters.resize(character_capacity);
		free_characters.reserve(character_capacity);
		retired_characters.reserve(character_capacity);
		character_collisions.mCharacters.reserve(character_capacity);
		for (auto index = character_capacity; index > 0; --index)
			free_characters.push_back(index - 1);
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
		character_collisions.mCharacters.clear();
		// CharacterVirtual owns its inner native body. Retire every such owner before the
		// physics system, and do not independently destroy a proxy through the body registry.
		for (auto &entry : characters)
			if (entry.native) {
				slots[entry.proxy.slot - 1].alive = false;
				entry.native.reset();
			}
		retired_characters.clear();
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
	ice::expected<std::size_t, diagnostic> resolve(character_id id) const {
		if (id.owner != owner || !id.slot || id.slot > characters.size())
			return ice::unexpected(diagnostic{"Foreign or invalid character identity"});
		const auto index = std::size_t(id.slot - 1);
		if (!characters[index].native || characters[index].generation != id.generation)
			return ice::unexpected(diagnostic{"Stale character identity"});
		return index;
	}
	body_id identify(const JPH::Body &body) const {
		auto index = body.GetUserData();
		if (index >= slots.size() || !slots[index].alive || slots[index].native != body.GetID())
			return {};
		return {owner, std::uint32_t(index + 1), slots[index].generation};
	}
	struct query_filter final : JPH::BodyFilter {
		const implementation    &state;
		std::span<const body_id> ignored;
		bool                     ignore_characters = false;
		explicit query_filter(const implementation &owner, std::span<const body_id> excluded = {},
		                      bool exclude_characters = false)
		    : state(owner), ignored(excluded), ignore_characters(exclude_characters) {}
		bool ShouldCollide(const JPH::BodyID &body) const override {
			for (auto id : ignored)
				if (state.slots[id.slot - 1].native == body)
					return false;
			return true;
		}
		bool ShouldCollideLocked(const JPH::Body &body) const override {
			auto id = state.identify(body);
			return id.owner && !body.IsSensor() &&
			       (!ignore_characters || !state.slots[id.slot - 1].character_proxy);
		}
	};
	void update_characters() {
		const query_filter filter(*this, {}, true);
		const auto         broad_filter = system.GetDefaultBroadPhaseLayerFilter(1);
		const auto         layer_filter = system.GetDefaultLayerFilter(1);
		const auto         gravity      = native(description.gravity);
		const auto         dt           = description.time_step;
		for (auto &entry : characters) {
			if (!entry.native)
				continue;
			auto &character = *entry.native;
			// Refresh at the tick boundary, after every live body has entered ID quarantine.
			// A create/teleport never leaves native contacts referring to unstepped bodies.
			character.RefreshContacts(broad_filter, layer_filter, filter, {}, temporary);
			character.UpdateGroundVelocity();
			const auto previous_velocity = character.GetLinearVelocity();
			const auto ground_velocity   = character.GetGroundVelocity();
			const bool grounded =
			    character.GetGroundState() == JPH::CharacterBase::EGroundState::OnGround &&
			    !character.IsSlopeTooSteep(character.GetGroundNormal()) &&
			    previous_velocity.GetY() - ground_velocity.GetY() < .1f;
			auto velocity = grounded ? ground_velocity : JPH::Vec3(0, previous_velocity.GetY(), 0);
			if (grounded && entry.jump_pending)
				velocity += JPH::Vec3(0, entry.description.jump_speed, 0);
			velocity += native(entry.intent.horizontal_velocity) + gravity * dt;
			character.SetLinearVelocity(velocity);
			JPH::CharacterVirtual::ExtendedUpdateSettings settings;
			settings.mStickToFloorStepDown = JPH::Vec3(0, -entry.description.support_distance, 0);
			settings.mWalkStairsStepUp     = JPH::Vec3(0, entry.description.step_height, 0);
			character.ExtendedUpdate(dt, gravity, settings, broad_filter, layer_filter, filter, {},
			                         temporary);
			// Jolt retains requested upward velocity when a sweep hits the ceiling. Cancel
			// that momentum rather than keeping the player pressed upward for the jump arc.
			if (character.GetLinearVelocity().GetY() > 0)
				for (const auto &contact : character.GetActiveContacts())
					if (contact.mHadCollision && !contact.mWasDiscarded &&
					    contact.mContactNormal.GetY() < -.01f) {
						auto stopped = character.GetLinearVelocity();
						stopped.SetY(0);
						character.SetLinearVelocity(stopped);
						break;
					}
			entry.jump_pending = false;
			entry.ground_dirty = false;
		}
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
	    description.max_contact_events > 65536 || description.max_characters > 65536)
		return ice::unexpected(
		    diagnostic{"Invalid fixed-step 3D world settings or resource budgets"});
	std::shared_ptr<backend_runtime> runtime;
	try {
		runtime = initialize_backend();
	} catch (const std::exception &error) {
		return ice::unexpected(diagnostic{error.what()});
	}
	auto state = std::make_unique<implementation>(description, std::move(runtime));
	if (!state->owner)
		return ice::unexpected(diagnostic{"World lifetime identity exhausted"});
	return world3d(std::move(state));
}
ice::expected<body_id, diagnostic> world3d::create_body(const body3d_description &description) {
	if (!state_ || state_->faulted || state_->stepping || state_->applying_poses)
		return ice::unexpected(diagnostic{"3D world is unavailable/faulted"});
	auto &state = *state_;
	if (state_->epoch == std::numeric_limits<std::uint64_t>::max())
		return ice::unexpected(diagnostic{"Native mutation epoch exhausted"});
	if (!valid(description.motion) || !finite(description.position) ||
	    !orientation(description.rotation) || !finite(description.linear_velocity, 10000) ||
	    !finite(description.angular_velocity, 10000) || !std::isfinite(description.mass) ||
	    description.mass < .001f || description.mass > 1.0e9f ||
	    !std::isfinite(description.friction) || description.friction < 0 ||
	    description.friction > 10 || !std::isfinite(description.restitution) ||
	    description.restitution < 0 || description.restitution > 1)
		return ice::unexpected(diagnostic{"Invalid body pose, velocity, mass or material"});
	if (description.shape.kind == shape3d_kind::plane && !description.static_shape &&
	    description.motion != motion_type::static_body)
		return ice::unexpected(diagnostic{"Plane collision shapes require a static body"});
	if (description.static_shape &&
	    (description.motion != motion_type::static_body || description.sensor))
		return ice::unexpected(
		    diagnostic{"Cooked surface collision requires a static nonsensor body"});
	if (state.free.empty())
		return ice::unexpected(diagnostic{"3D body budget exhausted"});
	ice::expected<JPH::ShapeSettings::ShapeResult, diagnostic> prepared;
	if (!description.static_shape) {
		prepared = prepare(description.shape);
		if (!prepared)
			return ice::unexpected(prepared.error());
	}
	const auto *shape = description.static_shape ? description.static_shape.state_->shape.GetPtr()
	                                             : prepared->Get().GetPtr();
	if (description.static_shape &&
	    !valid_static_pose(*shape, description.position, description.rotation))
		return ice::unexpected(diagnostic{"Cooked surface body bounds exceed +/-1e6 world meters"});
	JPH::BodyCreationSettings settings(shape, native(description.position),
	                                   native(description.rotation), native(description.motion),
	                                   description.motion == motion_type::static_body ? 0 : 1);
	settings.mLinearVelocity               = native(description.linear_velocity);
	settings.mAngularVelocity              = native(description.angular_velocity);
	settings.mFriction                     = description.friction;
	settings.mRestitution                  = description.restitution;
	settings.mIsSensor                     = description.sensor;
	settings.mEnhancedInternalEdgeRemoval  = true;
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
	auto &entry                 = state.slots[index];
	entry.native                = body->GetID();
	entry.alive                 = true;
	entry.stepped               = false;
	entry.motion                = description.motion;
	entry.character_proxy       = false;
	entry.static_shape_identity = description.static_shape.identity();
	bodies.AddBody(entry.native, description.motion == motion_type::static_body
	                                 ? JPH::EActivation::DontActivate
	                                 : JPH::EActivation::Activate);
	++state.count;
	++state.epoch;
	return body_id{state.owner, index + 1, entry.generation};
}
ice::expected<void, diagnostic> world3d::erase(body_id body) {
	if (!state_ || state_->stepping || state_->applying_poses)
		return ice::unexpected(diagnostic{"3D world is unavailable"});
	auto resolved = state_->resolve(body);
	if (!resolved)
		return ice::unexpected(resolved.error());
	auto &entry = state_->slots[*resolved];
	if (entry.character_proxy)
		return ice::unexpected(
		    diagnostic{"Character proxies must be retired through erase_character"});
	if (state_->epoch == std::numeric_limits<std::uint64_t>::max())
		return ice::unexpected(diagnostic{"Native mutation epoch exhausted"});
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
	++state_->epoch;
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
	                       entry.motion == motion_type::dynamic && !bodies.IsActive(entry.native),
	                       entry.static_shape_identity};
}
ice::expected<collider_geometry3d, diagnostic>
world3d::capture_collider(body_id body, collider_geometry3d_limits limits) const {
	if (!state_ || state_->stepping || state_->applying_poses)
		return ice::unexpected(diagnostic{"Collider readback is unavailable/busy"});
	auto resolved = state_->resolve(body);
	if (!resolved)
		return ice::unexpected(resolved.error());
	const auto &entry = state_->slots[*resolved];
	const auto &bodies = state_->system.GetBodyInterface();
	JPH::RVec3 position;
	JPH::Quat rotation;
	bodies.GetPositionAndRotation(entry.native, position, rotation);
	collider_geometry3d result;
	result.position = portable(position);
	result.rotation = portable(rotation);
	result.motion = entry.motion;
	result.sensor = bodies.IsSensor(entry.native);
	if (entry.character_proxy) {
		result.shape = collider_unavailable3d{collider_unsupported3d::character_proxy};
		return result;
	}
	const auto shape = bodies.GetShape(entry.native);
	if (!shape)
		return ice::unexpected(diagnostic{"Collider shape is unavailable"});
	switch (shape->GetSubType()) {
	case JPH::EShapeSubType::Box: {
		const auto &box = static_cast<const JPH::BoxShape &>(*shape);
		result.shape = collider_box3d{portable(box.GetHalfExtent()), box.GetConvexRadius()};
		return result;
	}
	case JPH::EShapeSubType::Sphere:
		result.shape = collider_sphere3d{static_cast<const JPH::SphereShape &>(*shape).GetRadius()};
		return result;
	case JPH::EShapeSubType::Capsule: {
		const auto &capsule = static_cast<const JPH::CapsuleShape &>(*shape);
		result.shape = collider_capsule3d{capsule.GetRadius(), capsule.GetHalfHeightOfCylinder()};
		return result;
	}
	case JPH::EShapeSubType::Plane:
		result.shape = collider_unavailable3d{collider_unsupported3d::plane};
		return result;
	case JPH::EShapeSubType::HeightField:
		result.shape = collider_unavailable3d{collider_unsupported3d::heightfield};
		return result;
	case JPH::EShapeSubType::Mesh:
		break;
	default:
		return result; // Unknown/decorated shapes stay explicitly unsupported.
	}
	// Two bounded passes avoid an unbounded stats walk, speculative maximum-size allocation,
	// vector growth past the payload budget, or an incomplete surface reported as complete.
	constexpr int chunk = JPH::Shape::cGetTrianglesMinTrianglesRequested;
	std::array<JPH::Float3, chunk * 3> vertices;
	const auto maximum = std::min(limits.triangles, limits.bytes / (3 * sizeof(math::vec3)));
	JPH::Shape::GetTrianglesContext context;
	auto start = [&] {
		shape->GetTrianglesStart(context, JPH::AABox::sBiggest(), shape->GetCenterOfMass(),
		                         JPH::Quat::sIdentity(), JPH::Vec3::sReplicate(1));
	};
	start();
	std::size_t count = 0;
	for (;;) {
		const auto found = shape->GetTrianglesNext(context, chunk, vertices.data());
		if (!found)
			break;
		if (found < 0 || found > chunk)
			return ice::unexpected(diagnostic{"Invalid native collider triangle readback"});
		if (std::size_t(found) > maximum - count) {
			result.shape = collider_unavailable3d{collider_unsupported3d::budget};
			return result;
		}
		count += std::size_t(found);
	}
	try {
		collider_triangles3d surface;
		surface.vertices.resize(count * 3);
		start();
		std::size_t written = 0;
		for (;;) {
			const auto found = shape->GetTrianglesNext(context, chunk, vertices.data());
			if (!found)
				break;
			if (found < 0 || found > chunk || std::size_t(found) > count - written)
				return ice::unexpected(diagnostic{"Native collider changed during readback"});
			for (int i = 0; i < found * 3; ++i) {
				const auto &v = vertices[std::size_t(i)];
				const math::vec3 point{v.x, v.y, v.z};
				if (!finite(point))
					return ice::unexpected(diagnostic{"Invalid cooked collider vertex"});
				surface.vertices[written * 3 + std::size_t(i)] = point;
			}
			written += std::size_t(found);
		}
		if (written != count)
			return ice::unexpected(diagnostic{"Native collider changed during readback"});
		result.shape = std::move(surface);
		return result;
	} catch (const std::bad_alloc &) {
		return ice::unexpected(diagnostic{"Collider readback allocation failed"});
	}
}
ice::expected<void, diagnostic> world3d::set_pose(body_id body, const math::vec3 &position,
                                                  const math::quat &rotation, bool reset_velocity) {
	if (!state_ || state_->faulted || state_->stepping || state_->applying_poses)
		return ice::unexpected(diagnostic{"3D world is unavailable/faulted/busy"});
	auto resolved = state_->resolve(body);
	if (!resolved)
		return ice::unexpected(resolved.error());
	if (!finite(position) || !orientation(rotation))
		return ice::unexpected(diagnostic{"Invalid 3D placement pose"});
	const auto &entry = state_->slots[*resolved];
	if (entry.character_proxy)
		return ice::unexpected(
		    diagnostic{"Character proxies must be moved through teleport_character"});
	auto      &bodies = state_->system.GetBodyInterface();
	JPH::RVec3 previous;
	JPH::Quat  prior_rotation;
	JPH::Vec3  linear, angular;
	bodies.GetPositionAndRotation(entry.native, previous, prior_rotation);
	bodies.GetLinearAndAngularVelocity(entry.native, linear, angular);
	auto accepted_rotation = native(rotation);
	if (equivalent_rotation(accepted_rotation, prior_rotation))
		accepted_rotation = prior_rotation;
	const bool pose_changed = position != portable(previous) || accepted_rotation != prior_rotation;
	const bool velocity_changed = reset_velocity && entry.motion != motion_type::static_body &&
	                              (linear != JPH::Vec3::sZero() || angular != JPH::Vec3::sZero());
	if (!pose_changed && !velocity_changed)
		return {};
	if (state_->epoch == std::numeric_limits<std::uint64_t>::max())
		return ice::unexpected(diagnostic{"Native mutation epoch exhausted"});
	if (entry.static_shape_identity &&
	    !valid_static_pose(*bodies.GetShape(entry.native), position, portable(accepted_rotation)))
		return ice::unexpected(diagnostic{"Cooked surface body bounds exceed +/-1e6 world meters"});
	if (pose_changed)
		bodies.SetPositionAndRotation(entry.native, native(position), accepted_rotation,
		                              entry.motion == motion_type::static_body
		                                  ? JPH::EActivation::DontActivate
		                                  : JPH::EActivation::Activate);
	if (reset_velocity && entry.motion != motion_type::static_body)
		bodies.SetLinearAndAngularVelocity(entry.native, JPH::Vec3::sZero(), JPH::Vec3::sZero());
	++state_->epoch;
	return {};
}
ice::expected<prepared_pose_batch, diagnostic>
world3d::prepare_poses(std::span<const body3d_pose_update> updates) const {
	if (!state_ || state_->faulted || state_->stepping || state_->applying_poses)
		return ice::unexpected(diagnostic{"3D world is unavailable/faulted/busy"});
	const auto &state = *state_;
	if (updates.size() > state.count)
		return ice::unexpected(diagnostic{"Pose span exceeds live body capacity"});
	try {
		prepared_pose_batch result;
		result.state_ = std::make_unique<prepared_pose_batch::implementation>();
		auto &plan    = *result.state_;
		plan.owner    = state.owner;
		plan.epoch    = state.epoch;
		plan.entries.reserve(updates.size());
		plan.previews.reserve(updates.size());
		plan.lock_ids.reserve(state.count);
		plan.wake_ids.reserve(state.count);
		plan.wake_lock_indices.reserve(state.count);
		plan.dirty_characters.reserve(state.characters_count);
		std::vector<bool> seen(state.slots.size(), false);
		for (const auto &update : updates) {
			auto index = state.resolve(update.body);
			if (!index)
				return ice::unexpected(index.error());
			if (seen[*index])
				return ice::unexpected(diagnostic{"Duplicate pose body"});
			seen[*index]           = true;
			const auto       &slot = state.slots[*index];
			JPH::BodyLockRead lock(state.system.GetBodyLockInterface(), slot.native);
			if (slot.character_proxy || !lock.SucceededAndIsInBroadPhase() ||
			    !static_pose_profile(lock.GetBody()))
				return ice::unexpected(
				    diagnostic{"Pose batches require static nonsensor actor bodies"});
			if (!finite(update.position) || !orientation(update.rotation))
				return ice::unexpected(diagnostic{"Invalid static pose"});
			const auto &body     = lock.GetBody();
			auto        rotation = native(update.rotation);
			if (equivalent_rotation(rotation, body.GetRotation()))
				rotation = body.GetRotation();
			if (!valid_static_pose(*body.GetShape(), update.position, portable(rotation)))
				return ice::unexpected(diagnostic{"Static pose bounds exceed world domain"});
			prepared_pose_batch::implementation::entry entry;
			entry.body             = update.body;
			entry.native_id        = slot.native;
			entry.requested_origin = native(update.position);
			entry.rotation         = rotation;
			entry.accepted_com =
			    entry.requested_origin + rotation * body.GetShape()->GetCenterOfMass();
			// Reusing an exact readback with an equivalent rotation must not round through COM
			// again, wake neighbors or invalidate another prepared plan.
			if (update.position == portable(body.GetPosition()) && rotation == body.GetRotation())
				entry.accepted_com = body.GetCenterOfMassPosition();
			entry.changed = entry.accepted_com != body.GetCenterOfMassPosition() ||
			                rotation != body.GetRotation();
			const auto accepted =
			    entry.changed ? entry.accepted_com - rotation * body.GetShape()->GetCenterOfMass()
			                  : body.GetPosition();
			plan.previews.push_back({update.body, portable(accepted), portable(rotation)});
			entry.old_bounds = body.GetWorldSpaceBounds();
			entry.new_bounds = body.GetShape()->GetWorldSpaceBounds(
			    JPH::RMat44::sRotationTranslation(rotation, entry.accepted_com), JPH::Vec3::sOne());
			// Include native contact skin for resting neighbors, without enlarging collision.
			const auto &settings = state.system.GetPhysicsSettings();
			const auto  skin     = JPH::Vec3::sReplicate(settings.mSpeculativeContactDistance +
			                                             settings.mPenetrationSlop);
			entry.old_bounds.ExpandBy(skin);
			entry.new_bounds.ExpandBy(skin);
			if (entry.changed) {
				state.system.SaveBodyState(body, entry.before);
				if (!entry.before.validate_static_payload())
					return ice::unexpected(
					    diagnostic{"Static rollback journal profile/capacity mismatch"});
				++plan.changed_count;
			}
			plan.lock_ids.push_back(slot.native);
			plan.entries.push_back(std::move(entry));
		}
		if (plan.changed_count && state.epoch == std::numeric_limits<std::uint64_t>::max())
			return ice::unexpected(diagnostic{"Native mutation epoch exhausted"});
		if (plan.changed_count) {
			// Preparation may scan the bounded slot table. No allocating AABB collector or
			// already-active body reaches finalize (activation resets its sleep timer).
			for (const auto &slot : state.slots) {
				if (!slot.alive || slot.character_proxy || slot.motion != motion_type::dynamic)
					continue;
				JPH::BodyLockRead lock(state.system.GetBodyLockInterface(), slot.native);
				if (!lock.SucceededAndIsInBroadPhase())
					return ice::unexpected(diagnostic{"Wake body disappeared during preparation"});
				const auto &body = lock.GetBody();
				if (body.IsActive() || body.IsSensor())
					continue;
				const auto &bounds = body.GetWorldSpaceBounds();
				if (std::any_of(plan.entries.begin(), plan.entries.end(), [&](const auto &entry) {
					    return entry.changed && (entry.old_bounds.Overlaps(bounds) ||
					                             entry.new_bounds.Overlaps(bounds));
				    })) {
					plan.wake_ids.push_back(slot.native);
					plan.lock_ids.push_back(slot.native);
				}
			}
			for (std::size_t index = 0; index < state.characters.size(); ++index) {
				const auto &character = state.characters[index];
				if (!character.native || character.ground_dirty)
					continue;
				const auto ground = character.native->GetGroundBodyID();
				if (std::any_of(plan.entries.begin(), plan.entries.end(), [&](const auto &entry) {
					    return entry.changed && entry.native_id == ground;
				    }))
					plan.dirty_characters.push_back(index);
			}
		}
		const auto less = [](JPH::BodyID a, JPH::BodyID b) {
			return a.GetIndexAndSequenceNumber() < b.GetIndexAndSequenceNumber();
		};
		std::sort(plan.lock_ids.begin(), plan.lock_ids.end(), less);
		for (auto &entry : plan.entries)
			entry.lock_index =
			    std::size_t(std::lower_bound(plan.lock_ids.begin(), plan.lock_ids.end(),
			                                 entry.native_id, less) -
			                plan.lock_ids.begin());
		for (auto id : plan.wake_ids)
			plan.wake_lock_indices.push_back(
			    std::size_t(std::lower_bound(plan.lock_ids.begin(), plan.lock_ids.end(), id, less) -
			                plan.lock_ids.begin()));
		return result;
	} catch (const std::bad_alloc &) {
		return ice::unexpected(diagnostic{"Static pose preparation allocation failed"});
	}
}
ice::expected<bool, diagnostic> world3d::apply_poses(prepared_pose_batch &&batch) {
	auto plan = std::move(batch.state_); // Every attempt consumes the caller's plan.
	if (!plan)
		return ice::unexpected(diagnostic{"Pose plan is empty/consumed/moved"});
	if (!state_ || state_->faulted || state_->stepping || state_->applying_poses)
		return ice::unexpected(diagnostic{"3D world is unavailable/faulted/busy"});
	auto &state = *state_;
	if (plan->owner != state.owner || plan->epoch != state.epoch)
		return ice::unexpected(diagnostic{"Pose plan belongs to another world or stale epoch"});
	if (!plan->changed_count)
		return false;
	if (state.epoch == std::numeric_limits<std::uint64_t>::max())
		return ice::unexpected(diagnostic{"Native mutation epoch exhausted"});
	JPH::BodyLockMultiWrite locks(state.system.GetBodyLockInterface(), plan->lock_ids.data(),
	                              int(plan->lock_ids.size()));
	for (const auto &entry : plan->entries) {
		auto        index = state.resolve(entry.body);
		const auto *body  = locks.GetBody(int(entry.lock_index));
		if (!index || state.slots[*index].native != entry.native_id || !body ||
		    !body->IsInBroadPhase() || !static_pose_profile(*body))
			return ice::unexpected(diagnostic{"Pose body binding changed before apply"});
	}
	for (auto index : plan->wake_lock_indices) {
		const auto *body = locks.GetBody(int(index));
		if (!body || !body->IsInBroadPhase() || !body->IsDynamic() || body->IsActive() ||
		    body->IsSensor())
			return ice::unexpected(diagnostic{"Sleeping wake set changed before apply"});
	}
	state.applying_poses = true;
	struct apply_scope {
		bool &flag;
		~apply_scope() { flag = false; }
	} scope{state.applying_poses};
	auto rollback = [&]() noexcept {
		for (auto &entry : plan->entries) {
			if (!entry.changed)
				continue;
			entry.before.rewind();
			// Public PhysicsSystem wrapper restores exact native state AND refreshes broadphase.
			// Its single local BodyID scratch never mutates our journal/lock index mapping.
			state.system.RestoreBodyState(*locks.GetBody(int(entry.lock_index)), entry.before);
			JPH_ASSERT(!entry.before.IsFailed() && entry.before.IsEOF());
		}
	};
#ifdef ICE_PHYSICS_POSE_BATCH_TESTING
	const auto fail_after =
	    std::exchange(state.fail_after_writes, std::numeric_limits<std::size_t>::max());
#endif
	std::size_t written = 0;
	auto       &bodies  = state.system.GetBodyInterfaceNoLock();
	for (const auto &entry : plan->entries) {
		if (!entry.changed)
			continue;
#ifdef ICE_PHYSICS_POSE_BATCH_TESTING
		if (written == fail_after) {
			rollback();
			return ice::unexpected(std::move(plan->rollback_error));
		}
#endif
		bodies.SetPositionAndRotation(entry.native_id, entry.requested_origin, entry.rotation,
		                              JPH::EActivation::DontActivate);
		++written;
		const auto &body = *locks.GetBody(int(entry.lock_index));
		if (body.GetCenterOfMassPosition() != entry.accepted_com ||
		    body.GetRotation() != entry.rotation) {
			rollback();
			return ice::unexpected(std::move(plan->rollback_error));
		}
	}
#ifdef ICE_PHYSICS_POSE_BATCH_TESTING
	if (written == fail_after) {
		rollback();
		return ice::unexpected(std::move(plan->rollback_error));
	}
#else
	(void)written;
#endif
	// No fallible operation or callback after this point. Capacity was allocated at world init;
	// no activation listener is installed. Only sleeping dynamics are activated, once each.
	if (!plan->wake_ids.empty())
		bodies.ActivateBodies(plan->wake_ids.data(), int(plan->wake_ids.size()));
	for (auto index : plan->dirty_characters)
		state.characters[index].ground_dirty = true;
	++state.epoch;
	return true;
}
std::uint64_t world3d::mutation_epoch() const noexcept { return state_ ? state_->epoch : 0; }
#ifdef ICE_PHYSICS_POSE_BATCH_TESTING
void detail::pose_batch_test_access::fail_after_writes(world3d &world, std::size_t count) {
	world.state_->fail_after_writes = count;
}
void detail::pose_batch_test_access::set_epoch(world3d &world, std::uint64_t epoch) {
	world.state_->epoch = epoch;
}
void detail::pose_batch_test_access::set_faulted(world3d &world, bool faulted) {
	world.state_->faulted = faulted;
}
namespace {
struct state_hash_record final : JPH::StateRecorder {
	std::uint64_t hash = 14695981039346656037ull;
	void          WriteBytes(const void *data, std::size_t count) override {
        const auto *bytes = static_cast<const unsigned char *>(data);
        for (std::size_t i = 0; i < count; ++i)
            hash = (hash ^ bytes[i]) * 1099511628211ull;
	}
	void ReadBytes(void *, std::size_t) override {}
	bool IsEOF() const override { return true; }
	bool IsFailed() const override { return false; }
};
} // namespace
std::uint64_t detail::pose_batch_test_access::body_state_hash(const world3d &world, body_id id) {
	auto index = world.state_->resolve(id);
	if (!index)
		return 0;
	JPH::BodyLockRead lock(world.state_->system.GetBodyLockInterface(),
	                       world.state_->slots[*index].native);
	if (!lock.Succeeded())
		return 0;
	state_hash_record record;
	world.state_->system.SaveBodyState(lock.GetBody(), record);
	return record.hash;
}
std::uint64_t detail::pose_batch_test_access::character_state_hash(const world3d &world,
                                                                   character_id   id) {
	auto index = world.state_->resolve(id);
	if (!index)
		return 0;
	const auto       &entry = world.state_->characters[*index];
	state_hash_record record;
	entry.native->SaveState(record);
	record.Write(entry.intent.horizontal_velocity.x);
	record.Write(entry.intent.horizontal_velocity.y);
	record.Write(entry.intent.horizontal_velocity.z);
	record.Write(entry.intent.jump);
	record.Write(entry.jump_pending);
	record.Write(entry.ground_dirty);
	return record.hash;
}
#endif
ice::expected<void, diagnostic> world3d::apply_impulse(body_id body, const math::vec3 &impulse) {
	if (!state_ || state_->faulted || state_->stepping || state_->applying_poses)
		return ice::unexpected(diagnostic{"3D world is unavailable/faulted"});
	auto resolved = state_->resolve(body);
	if (!resolved)
		return ice::unexpected(resolved.error());
	if (state_->slots[*resolved].character_proxy)
		return ice::unexpected(diagnostic{"Character proxies do not accept independent impulses"});
	if (state_->slots[*resolved].motion != motion_type::dynamic || !finite(impulse, 1.0e6f))
		return ice::unexpected(
		    diagnostic{"Impulse requires a dynamic body and finite bounded vector"});
	if (impulse == math::vec3(0))
		return {};
	if (state_->epoch == std::numeric_limits<std::uint64_t>::max())
		return ice::unexpected(diagnostic{"Native mutation epoch exhausted"});
	state_->system.GetBodyInterface().AddImpulse(state_->slots[*resolved].native, native(impulse));
	++state_->epoch;
	return {};
}
ice::expected<character_id, diagnostic>
world3d::create_character(const character3d_description &description) {
	if (!state_ || state_->faulted || state_->stepping || state_->applying_poses)
		return ice::unexpected(diagnostic{"3D world is unavailable/faulted"});
	auto &state = *state_;
	if (state_->epoch == std::numeric_limits<std::uint64_t>::max())
		return ice::unexpected(diagnostic{"Native mutation epoch exhausted"});
	if (!valid(description))
		return ice::unexpected(
		    diagnostic{"Invalid character pose, capsule, motor settings or budget"});
	if (state.description.gravity.x != 0 || state.description.gravity.z != 0 ||
	    state.description.gravity.y > 0)
		return ice::unexpected(diagnostic{"Character motors require zero or downward Y gravity"});
	if (state.free.empty() || state.free_characters.empty())
		return ice::unexpected(diagnostic{"Character or proxy body budget exhausted"});
	auto shape       = prepare_character_shape(description, 1);
	auto proxy_shape = prepare_character_shape(description, .95f);
	if (!shape)
		return ice::unexpected(shape.error());
	if (!proxy_shape)
		return ice::unexpected(proxy_shape.error());
	JPH::CharacterVirtualSettings settings;
	settings.mShape                       = shape->Get();
	settings.mInnerBodyShape              = proxy_shape->Get();
	settings.mInnerBodyLayer              = 1;
	settings.mMass                        = description.mass;
	settings.mMaxStrength                 = description.max_push_force;
	settings.mMaxSlopeAngle               = description.max_slope_angle;
	settings.mCharacterPadding            = std::min(.02f, description.radius * .1f);
	settings.mCollisionTolerance          = std::min(.001f, description.radius * .01f);
	settings.mPredictiveContactDistance   = std::min(.1f, description.radius * .5f);
	settings.mSupportingVolume            = JPH::Plane(JPH::Vec3::sAxisY(), -description.radius);
	settings.mMaxNumHits                  = description.max_contacts;
	settings.mEnhancedInternalEdgeRemoval = true;
	settings.mBackFaceMode                = JPH::EBackFaceMode::IgnoreBackFaces;
	const auto body_index                 = state.free.back();
	const auto character_index            = state.free_characters.back();
	auto       prepared                   = std::make_unique<JPH::CharacterVirtual>(
        &settings, native(description.position), JPH::Quat::sIdentity(), body_index, &state.system);
	if (prepared->GetInnerBodyID().IsInvalid())
		return ice::unexpected(diagnostic{"Jolt could not allocate a character collision proxy"});
	prepared->SetLinearVelocity(native(description.linear_velocity));
	prepared->SetCharacterVsCharacterCollision(&state.character_collisions);
	// These containers were reserved at world construction. All fallible preparation is done
	// before publishing slots or exposing a character to another motor's collision queries.
	state.character_collisions.Add(prepared.get());
	state.free.pop_back();
	state.free_characters.pop_back();
	auto &proxy                 = state.slots[body_index];
	proxy.native                = prepared->GetInnerBodyID();
	proxy.motion                = motion_type::kinematic;
	proxy.alive                 = true;
	proxy.stepped               = false;
	proxy.character_proxy       = true;
	proxy.static_shape_identity = 0;
	auto &entry                 = state.characters[character_index];
	entry.native                = std::move(prepared);
	entry.description           = description;
	entry.intent                = {};
	entry.proxy                 = {state.owner, body_index + 1, proxy.generation};
	entry.jump_pending          = false;
	entry.ground_dirty          = true;
	++state.count;
	++state.characters_count;
	++state.epoch;
	return character_id{state.owner, character_index + 1, entry.generation};
}
ice::expected<void, diagnostic> world3d::erase_character(character_id character) {
	if (!state_ || state_->stepping || state_->applying_poses)
		return ice::unexpected(diagnostic{"3D world is unavailable"});
	auto resolved = state_->resolve(character);
	if (!resolved)
		return ice::unexpected(resolved.error());
	auto &state = *state_;
	if (state_->epoch == std::numeric_limits<std::uint64_t>::max())
		return ice::unexpected(diagnostic{"Native mutation epoch exhausted"});
	auto &entry = state.characters[*resolved];
	auto &proxy = state.slots[entry.proxy.slot - 1];
	state.character_collisions.Remove(entry.native.get());
	if (proxy.stepped) {
		// CharacterVirtual owns and immediately destroys its inner body. Keep that owner
		// alive but noncolliding until cached manifolds/other virtual contacts are refreshed.
		state.system.GetBodyInterface().SetObjectLayer(proxy.native, inactive_character_layer);
		state.retired_characters.push_back(std::move(entry.native));
	} else
		entry.native.reset();
	proxy.alive = false;
	--state.count;
	--state.characters_count;
	if (proxy.generation != std::numeric_limits<std::uint64_t>::max()) {
		++proxy.generation;
		state.free.push_back(entry.proxy.slot - 1);
	}
	entry.proxy        = {};
	entry.intent       = {};
	entry.jump_pending = false;
	entry.ground_dirty = true;
	if (entry.generation != std::numeric_limits<std::uint64_t>::max()) {
		++entry.generation;
		state.free_characters.push_back(std::uint32_t(*resolved));
	}
	++state.epoch;
	return {};
}
ice::expected<character3d_snapshot, diagnostic>
world3d::inspect_character(character_id character) const {
	if (!state_)
		return ice::unexpected(diagnostic{"3D world is unavailable"});
	auto resolved = state_->resolve(character);
	if (!resolved)
		return ice::unexpected(resolved.error());
	const auto          &entry            = state_->characters[*resolved];
	const auto          &native_character = *entry.native;
	character3d_snapshot result;
	result.id                 = character;
	result.proxy              = entry.proxy;
	result.position           = portable(native_character.GetPosition());
	result.linear_velocity    = portable(native_character.GetLinearVelocity());
	result.jump_pending       = entry.jump_pending;
	result.contacts_truncated = native_character.GetMaxHitsExceeded();
	if (!entry.ground_dirty) {
		result.ground_state    = portable(native_character.GetGroundState());
		result.ground_position = portable(native_character.GetGroundPosition());
		result.ground_normal   = portable(native_character.GetGroundNormal());
		result.ground_velocity = portable(native_character.GetGroundVelocity());
		const auto ground      = native_character.GetGroundBodyID();
		if (!ground.IsInvalid()) {
			JPH::BodyLockRead lock(state_->system.GetBodyLockInterface(), ground);
			if (lock.SucceededAndIsInBroadPhase())
				result.ground_body = state_->identify(lock.GetBody());
		}
	}
	return result;
}
ice::expected<void, diagnostic> world3d::set_character_intent(character_id              character,
                                                              const character3d_intent &intent) {
	if (!state_ || state_->faulted || state_->stepping || state_->applying_poses)
		return ice::unexpected(diagnostic{"3D world is unavailable/faulted"});
	auto resolved = state_->resolve(character);
	if (!resolved)
		return ice::unexpected(resolved.error());
	if (!finite(intent.horizontal_velocity, 10000) || intent.horizontal_velocity.y != 0)
		return ice::unexpected(
		    diagnostic{"Character intent requires finite bounded X/Z velocity and zero Y"});
	auto &entry = state_->characters[*resolved];
	if (entry.intent.horizontal_velocity == intent.horizontal_velocity &&
	    entry.intent.jump == intent.jump && (!intent.jump || entry.jump_pending))
		return {};
	if (state_->epoch == std::numeric_limits<std::uint64_t>::max())
		return ice::unexpected(diagnostic{"Native mutation epoch exhausted"});

	entry.intent       = intent;
	entry.jump_pending = entry.jump_pending || intent.jump;
	++state_->epoch;
	return {};
}
ice::expected<void, diagnostic> world3d::cancel_character_intent(character_id character) {
	if (!state_ || state_->stepping || state_->applying_poses)
		return ice::unexpected(diagnostic{"3D world is unavailable"});
	auto resolved = state_->resolve(character);
	if (!resolved)
		return ice::unexpected(resolved.error());
	auto &entry = state_->characters[*resolved];
	if (entry.intent.horizontal_velocity == math::vec3(0) && !entry.intent.jump &&
	    !entry.jump_pending)
		return {};
	if (state_->epoch == std::numeric_limits<std::uint64_t>::max())
		return ice::unexpected(diagnostic{"Native mutation epoch exhausted"});

	entry.intent       = {};
	entry.jump_pending = false;
	++state_->epoch;
	return {};
}
ice::expected<void, diagnostic> world3d::teleport_character(character_id      character,
                                                            const math::vec3 &position,
                                                            const math::vec3 &linear_velocity) {
	if (!state_ || state_->faulted || state_->stepping || state_->applying_poses)
		return ice::unexpected(diagnostic{"3D world is unavailable/faulted"});
	auto resolved = state_->resolve(character);
	if (!resolved)
		return ice::unexpected(resolved.error());
	if (!finite(position) || !finite(linear_velocity, 10000))
		return ice::unexpected(diagnostic{"Invalid character teleport position or velocity"});
	auto &entry = state_->characters[*resolved];
	if (portable(entry.native->GetPosition()) == position &&
	    portable(entry.native->GetLinearVelocity()) == linear_velocity && entry.ground_dirty &&
	    entry.intent.horizontal_velocity == math::vec3(0) && !entry.intent.jump &&
	    !entry.jump_pending)
		return {};
	if (state_->epoch == std::numeric_limits<std::uint64_t>::max())
		return ice::unexpected(diagnostic{"Native mutation epoch exhausted"});

	entry.native->SetPosition(native(position));
	entry.native->SetLinearVelocity(native(linear_velocity));
	entry.intent       = {};
	entry.jump_pending = false;
	entry.ground_dirty = true;
	++state_->epoch;
	return {};
}
ice::expected<step_report, diagnostic> world3d::step() {
	if (!state_ || state_->faulted || state_->stepping || state_->applying_poses)
		return ice::unexpected(diagnostic{"3D world is unavailable/faulted"});
	auto &state = *state_;
	if (state.ticks == std::numeric_limits<std::uint64_t>::max())
		return ice::unexpected(diagnostic{"Simulation tick exhausted"});
	if (state_->epoch == std::numeric_limits<std::uint64_t>::max())
		return ice::unexpected(diagnostic{"Native mutation epoch exhausted"});
	++state.epoch; // A step can partially mutate native state even if it later faults.
	state.events.clear();
	state.events.swap(state.pending);
	state.truncated         = state.pending_truncated;
	state.pending_truncated = false;
	state.cache.compact();
	for (auto &entry : state.slots)
		if (entry.alive)
			entry.stepped = true;
	state.stepping = true;
	state.update_characters();
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
	state.retired_characters.clear();
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
	for (const auto &entry : state.characters)
		if (entry.native &&
		    (!finite(portable(entry.native->GetPosition()), std::numeric_limits<float>::max()) ||
		     !finite(portable(entry.native->GetLinearVelocity()),
		             std::numeric_limits<float>::max()))) {
			state.faulted = true;
			return ice::unexpected(
			    diagnostic{"Jolt produced nonfinite character state; world requires recovery"});
		}
	++state.ticks;
	return step_report{state.ticks, state.truncated};
}
std::span<const contact_event> world3d::contacts() const {
	return state_ ? std::span<const contact_event>(state_->events)
	              : std::span<const contact_event>{};
}
ice::expected<std::optional<ray_hit3d>, diagnostic>
world3d::ray_cast(const math::vec3 &origin, const math::vec3 &displacement,
                  std::span<const body_id> ignored) const {
	if (!state_)
		return ice::unexpected(diagnostic{"3D world is unavailable"});
	if (!finite(origin) || !finite(displacement) || glm::dot(displacement, displacement) < 1.0e-12f)
		return ice::unexpected(diagnostic{"Ray requires a finite origin and nonzero displacement"});
	if (ignored.size() > state_->slots.size())
		return ice::unexpected(diagnostic{"Ray ignore list exceeds the world body budget"});
	for (auto id : ignored) {
		auto checked = state_->resolve(id);
		if (!checked)
			return ice::unexpected(checked.error());
	}
	JPH::RRayCast      ray(native(origin), native(displacement));
	JPH::RayCastResult hit;
	if (!state_->system.GetNarrowPhaseQuery().CastRay(
	        ray, hit, {}, {}, implementation::query_filter(*state_, ignored)))
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
std::uint32_t world3d::character_count() const { return state_ ? state_->characters_count : 0; }
} // namespace ice::physics
