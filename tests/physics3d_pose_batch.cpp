#include "ice/physics/world3d.h"
// Jolt's umbrella must precede every other Jolt header.
#include <Jolt/Jolt.h>

#include <Jolt/Core/Memory.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <vector>

namespace allocation_probe {
bool           tracking  = false;
std::size_t    cpp_calls = 0, native_calls = 0;
std::ptrdiff_t fail_after = -1;
void           cpp_allocation() {
    if (tracking)
        ++cpp_calls;
    if (fail_after == 0) {
        fail_after = -1;
        throw std::bad_alloc();
    }
    if (fail_after > 0)
        --fail_after;
}
JPH::AllocateFunction        original_allocate;
JPH::ReallocateFunction      original_reallocate;
JPH::AlignedAllocateFunction original_aligned;
void                        *native_allocate(std::size_t size) {
    if (tracking)
        ++native_calls;
    return original_allocate(size);
}
void *native_reallocate(void *pointer, std::size_t old_size, std::size_t size) {
	if (tracking)
		++native_calls;
	return original_reallocate(pointer, old_size, size);
}
void *native_aligned(std::size_t size, std::size_t alignment) {
	if (tracking)
		++native_calls;
	return original_aligned(size, alignment);
}
void install() {
	original_allocate    = JPH::Allocate;
	original_reallocate  = JPH::Reallocate;
	original_aligned     = JPH::AlignedAllocate;
	JPH::Allocate        = native_allocate;
	JPH::Reallocate      = native_reallocate;
	JPH::AlignedAllocate = native_aligned;
}
void begin() {
	cpp_calls = native_calls = 0;
	tracking                 = true;
}
struct counts {
	std::size_t cpp, native;
};
counts end() {
	tracking   = false;
	fail_after = -1;
	return {cpp_calls, native_calls};
}
} // namespace allocation_probe
void *operator new(std::size_t size) {
	allocation_probe::cpp_allocation();
	if (void *pointer = std::malloc(size ? size : 1))
		return pointer;
	throw std::bad_alloc();
}
void *operator new[](std::size_t size) { return ::operator new(size); }
void  operator delete(void *pointer) noexcept { std::free(pointer); }
void  operator delete[](void *pointer) noexcept { std::free(pointer); }
void  operator delete(void *pointer, std::size_t) noexcept { std::free(pointer); }
void  operator delete[](void *pointer, std::size_t) noexcept { std::free(pointer); }
void *operator new(std::size_t size, std::align_val_t alignment) {
	allocation_probe::cpp_allocation();
	void *pointer = nullptr;
	if (posix_memalign(&pointer, std::size_t(alignment), size ? size : 1) != 0)
		throw std::bad_alloc();
	return pointer;
}
void *operator new[](std::size_t size, std::align_val_t alignment) {
	return ::operator new(size, alignment);
}
void operator delete(void *pointer, std::align_val_t) noexcept { std::free(pointer); }
void operator delete[](void *pointer, std::align_val_t) noexcept { std::free(pointer); }
void operator delete(void *pointer, std::size_t, std::align_val_t) noexcept { std::free(pointer); }
void operator delete[](void *pointer, std::size_t, std::align_val_t) noexcept {
	std::free(pointer);
}

namespace {
using namespace ice::physics;
using ice::math::vec3;
using access = detail::pose_batch_test_access;
void check(bool condition, const char *expression, int line) {
	if (!condition)
		throw std::runtime_error("Pose batch check " + std::to_string(line) + ": " + expression);
}
#define CHECK(expression) check(bool(expression), #expression, __LINE__)
template <class T> T take(ice::expected<T, diagnostic> value) {
	if (!value)
		throw std::runtime_error(value.error().message);
	return std::move(*value);
}
bool    near(vec3 a, vec3 b, float epsilon = .0001f) { return glm::length(a - b) < epsilon; }
world3d make_world(vec3 gravity = {0, -9.81f, 0}) {
	world3d_description settings;
	settings.max_bodies         = 32;
	settings.max_characters     = 2;
	settings.max_contact_events = 128;
	settings.gravity            = gravity;
	return take(world3d::create(settings));
}
body_id box(world3d &world, vec3 position, motion_type motion = motion_type::static_body,
            vec3 half_extent = {.5f, .5f, .5f}) {
	body3d_description description;
	description.motion            = motion;
	description.position          = position;
	description.shape.half_extent = half_extent;
	description.friction          = 1;
	description.restitution       = 0;
	return take(world.create_body(description));
}
cooked_static_shape offset_shape() {
	std::array<vec3, 8> vertices;
	for (unsigned i = 0; i < 8; ++i)
		vertices[i] = {i & 1 ? 3.7f : 1.1f, i & 2 ? 4.8f : 2.2f, i & 4 ? -1.3f : -5.9f};
	const std::array<std::uint32_t, 36> indices{0, 4, 6, 0, 6, 2, 1, 3, 7, 1, 7, 5,
	                                            0, 1, 5, 0, 5, 4, 2, 6, 7, 2, 7, 3,
	                                            0, 2, 3, 0, 3, 1, 4, 5, 7, 4, 7, 6};
	return take(cooked_static_shape::cook_mesh({vertices, indices}));
}
body_id offset_body(world3d &world, vec3 position = {10000, 3, -10000}) {
	body3d_description description;
	description.motion       = motion_type::static_body;
	description.static_shape = offset_shape();
	description.position     = position;
	description.rotation     = glm::angleAxis(.17f, vec3(0, 1, 0));
	return take(world.create_body(description));
}
prepared_pose_batch plan(world3d &world, body_id body, vec3 position,
                         ice::math::quat rotation = {1, 0, 0, 0}) {
	const std::array updates{body3d_pose_update{body, position, rotation}};
	return take(world.prepare_poses(updates));
}
bool apply_without_allocations(world3d &world, prepared_pose_batch &&batch, bool expected = true) {
	allocation_probe::begin();
	auto       result = world.apply_poses(std::move(batch));
	const auto counts = allocation_probe::end();
	CHECK(result && *result == expected && counts.cpp == 0 && counts.native == 0);
	return *result;
}
void success() {
	auto       world  = make_world();
	const auto first  = box(world, {0, 2, 0});
	const auto second = offset_body(world);
	const auto epoch = world.mutation_epoch(), identity = world.identity(), tick = world.tick();
	const auto cook = take(world.inspect(second)).static_shape_identity;
	const std::array updates{
	    body3d_pose_update{second,
	                       {10001.37f, 4.21f, -10003.81f},
	                       glm::angleAxis(.71f, glm::normalize(vec3(1, 2, 3)))},
	    body3d_pose_update{first, {5, 3, -4}, {1, 0, 0, 0}}};
	auto              batch = take(world.prepare_poses(updates));
	const std::vector preview(batch.poses().begin(), batch.poses().end());
	CHECK(world.mutation_epoch() == epoch);
	apply_without_allocations(world, std::move(batch));
	// Applying deliberately consumes the plan; querying its defined empty state is legal.
	// NOLINTNEXTLINE(bugprone-use-after-move)
	CHECK(batch.poses().empty());
	CHECK(world.identity() == identity && world.tick() == tick &&
	      world.mutation_epoch() == epoch + 1);
	for (const auto &expected : preview) {
		const auto actual = take(world.inspect(expected.body));
		CHECK(actual.id == expected.body && actual.position == expected.position &&
		      actual.rotation == expected.rotation);
	}
	CHECK(take(world.inspect(second)).static_shape_identity == cook && world.body_count() == 2);
	CHECK(!take(world.ray_cast({0, 5, 0}, {0, -5, 0})));
	const auto hit = take(world.ray_cast({5, 6, -4}, {0, -6, 0}));
	CHECK(hit && hit->body == first);
	std::puts("2 static poses: exact accepted readback; apply C++/Jolt allocations=0/0");
}
void validation() {
	auto       world    = make_world();
	const auto first    = box(world, {0, 2, 0});
	const auto second   = box(world, {4, 2, 0});
	const auto original = access::body_state_hash(world, first);
	const auto epoch    = world.mutation_epoch();
	const auto nan      = std::numeric_limits<float>::quiet_NaN();
	std::array updates{body3d_pose_update{first, {3, 2, 0}, {1, 0, 0, 0}},
	                   body3d_pose_update{second, {nan, 2, 0}, {1, 0, 0, 0}}};
	CHECK(!world.prepare_poses(updates));
	CHECK(
	    !world.prepare_poses(std::array{body3d_pose_update{first, {1000000, 2, 0}, {1, 0, 0, 0}}}));
	CHECK(!world.prepare_poses(std::array{body3d_pose_update{first, {0, 2, 0}, {0, 0, 0, 0}}}));
	updates[1] = updates[0];
	CHECK(!world.prepare_poses(updates));
	auto other = make_world();
	updates[1] = {box(other, {0, 2, 0}), {0, 3, 0}, {1, 0, 0, 0}};
	CHECK(!world.prepare_poses(updates));
	for (auto motion : {motion_type::dynamic, motion_type::kinematic}) {
		const auto       id = box(world, {10, 3, 0}, motion);
		const std::array invalid{body3d_pose_update{id, {10, 4, 0}, {1, 0, 0, 0}}};
		CHECK(!world.prepare_poses(invalid));
	}
	body3d_description sensor;
	sensor.motion        = motion_type::static_body;
	sensor.sensor        = true;
	const auto sensor_id = take(world.create_body(sensor));
	CHECK(!world.prepare_poses(std::array{body3d_pose_update{sensor_id, {0, 3, 0}, {1, 0, 0, 0}}}));
	body3d_description plane;
	plane.motion     = motion_type::static_body;
	plane.shape.kind = shape3d_kind::plane;
	const auto floor = take(world.create_body(plane));
	CHECK(!world.prepare_poses(std::array{body3d_pose_update{floor, {0, 3, 0}, {1, 0, 0, 0}}}));
	std::array<float, 16> heights{};
	body3d_description    terrain;
	terrain.motion        = motion_type::static_body;
	terrain.static_shape  = take(cooked_static_shape::cook_heightfield({heights, 4}));
	const auto terrain_id = take(world.create_body(terrain));
	CHECK(
	    !world.prepare_poses(std::array{body3d_pose_update{terrain_id, {0, 3, 0}, {1, 0, 0, 0}}}));
	const std::vector<body3d_pose_update> oversized(world.body_count() + 1,
	                                                {first, {0, 3, 0}, {1, 0, 0, 0}});
	CHECK(!world.prepare_poses(oversized));
	const auto character = take(world.create_character());
	const auto proxy     = take(world.inspect_character(character)).proxy;
	CHECK(!world.prepare_poses(std::array{body3d_pose_update{proxy, {0, 3, 0}, {1, 0, 0, 0}}}));
	const auto stale = box(world, {20, 3, 0});
	CHECK(world.erase(stale));
	CHECK(!world.prepare_poses(std::array{body3d_pose_update{stale, {0, 3, 0}, {1, 0, 0, 0}}}));
	CHECK(access::body_state_hash(world, first) == original && world.mutation_epoch() > epoch);
	// A fresh valid plan survives invalid inputs and failed legacy setters.
	auto       valid  = plan(world, first, {0, 4, 0});
	const auto before = world.mutation_epoch();
	CHECK(!world.set_pose(second, {nan, 0, 0}, {1, 0, 0, 0}) && world.mutation_epoch() == before);
	apply_without_allocations(world, std::move(valid));
}
void rollback() {
	auto             world = make_world({0, 0, 0});
	const std::array ids{box(world, {0, 2, 0}), offset_body(world, {20, 3, -20}),
	                     box(world, {-10, 3, 0})};
	const std::array updates{
	    body3d_pose_update{ids[2], {-100, 13, 0}, {1, 0, 0, 0}},
	    body3d_pose_update{ids[0], {100, 12, 0}, {1, 0, 0, 0}},
	    body3d_pose_update{ids[1], {200, 13, -20}, glm::angleAxis(.9f, vec3(0, 1, 0))}};
	std::array<std::uint64_t, 3> hashes;
	for (unsigned i = 0; i < 3; ++i)
		hashes[i] = access::body_state_hash(world, ids[i]);
	const auto epoch    = world.mutation_epoch();
	auto       accepted = take(world.prepare_poses(updates));
	for (std::size_t prefix = 0; prefix <= updates.size(); ++prefix) {
		auto batch = take(world.prepare_poses(updates));
		access::fail_after_writes(world, prefix);
		allocation_probe::begin();
		auto       rejected = world.apply_poses(std::move(batch));
		const auto counts   = allocation_probe::end();
		CHECK(!rejected && counts.cpp == 0 && counts.native == 0 &&
		      world.mutation_epoch() == epoch && world.tick() == 0);
		for (unsigned i = 0; i < 3; ++i)
			CHECK(access::body_state_hash(world, ids[i]) == hashes[i]);
		const auto old_hit = take(world.ray_cast({0, 6, 0}, {0, -6, 0}));
		CHECK(old_hit && old_hit->body == ids[0]);
		CHECK(!take(world.ray_cast({100, 16, 0}, {0, -16, 0})));
	}
	// Rollback leaves the other same-epoch plan valid; broadphase may only be conservative.
	apply_without_allocations(world, std::move(accepted));
	std::puts(
	    "rollback prefixes 0..3: exact native journals, old/new ray equivalence, allocations=0/0");
	// Compare subsequent physics to an untouched control, without a private-cache bit promise.
	auto candidate = make_world(), control = make_world();
	struct control_handles {
		body_id      support, dynamic;
		character_id character;
	};
	auto setup = [](world3d &owner) {
		const auto support = box(owner, {0, 0, 0}, motion_type::static_body, {3, .5f, 3});
		const auto dynamic = box(owner, {0, 1.01f, 0}, motion_type::dynamic);
		character3d_description settings;
		settings.position = {2, .5f, 0};
		return control_handles{support, dynamic, take(owner.create_character(settings))};
	};
	const auto candidate_handles = setup(candidate), control_handles_ = setup(control);
	const auto candidate_character = candidate_handles.character,
	           control_character   = control_handles_.character;
	for (int i = 0; i < 60; ++i) {
		take(candidate.step());
		take(control.step());
	}
	const auto candidate_support = candidate_handles.support;
	const auto candidate_dynamic = candidate_handles.dynamic,
	           control_dynamic   = control_handles_.dynamic;
	CHECK(candidate.set_character_intent(candidate_character, {{.25f, 0, 0}, true}));
	CHECK(control.set_character_intent(control_character, {{.25f, 0, 0}, true}));
	auto abort = plan(candidate, candidate_support, {20, 0, 0});
	access::fail_after_writes(candidate, 1);
	CHECK(!candidate.apply_poses(std::move(abort)));
	for (int i = 0; i < 20; ++i) {
		take(candidate.step());
		take(control.step());
		const auto a  = take(candidate.inspect(candidate_dynamic)),
		           b  = take(control.inspect(control_dynamic));
		const auto ca = take(candidate.inspect_character(candidate_character)),
		           cb = take(control.inspect_character(control_character));
		CHECK(near(a.position, b.position) && near(a.linear_velocity, b.linear_velocity) &&
		      a.sleeping == b.sleeping);
		CHECK(near(ca.position, cb.position) && near(ca.linear_velocity, cb.linear_velocity) &&
		      ca.ground_state == cb.ground_state);
		const auto contact_key = [](const contact_event &event) {
			auto first  = std::pair(event.first.slot, event.first.generation);
			auto second = std::pair(event.second.slot, event.second.generation);
			if (second < first)
				std::swap(first, second);
			return std::tuple(event.phase, event.sensor, first, second);
		};
		CHECK(candidate.contacts().size() == control.contacts().size());
		for (const auto &event : candidate.contacts()) {
			const auto matches = [&](const contact_event &other) {
				return contact_key(event) == contact_key(other);
			};
			CHECK(
			    std::count_if(candidate.contacts().begin(), candidate.contacts().end(), matches) ==
			    std::count_if(control.contacts().begin(), control.contacts().end(), matches));
		}
		const auto candidate_hit = take(candidate.ray_cast({2, 6, 0}, {0, -8, 0}));
		const auto control_hit   = take(control.ray_cast({2, 6, 0}, {0, -8, 0}));
		CHECK(candidate_hit.has_value() == control_hit.has_value());
		if (candidate_hit)
			CHECK(candidate_hit->body.slot == control_hit->body.slot &&
			      candidate_hit->body.generation == control_hit->body.generation &&
			      near(candidate_hit->point, control_hit->point) &&
			      near(candidate_hit->normal, control_hit->normal) &&
			      std::abs(candidate_hit->fraction - control_hit->fraction) < .0001f);
	}
	std::puts("20 subsequent steps after rollback match untouched body/character/contact control");
}
// These checks deliberately exercise the documented moved-from and consumed states.
// NOLINTBEGIN(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
void lifetimes() {
	auto                original = make_world();
	const auto          id       = box(original, {0, 2, 0});
	auto                first    = plan(original, id, {0, 3, 0});
	auto                second   = plan(original, id, {0, 3, 0});
	prepared_pose_batch moved(std::move(first));
	CHECK(first.poses().empty());
	CHECK(!original.apply_poses(std::move(first)));
	const auto identity = original.identity();
	auto       owner    = std::move(original);
	CHECK(owner.identity() == identity && original.identity() == 0);
	CHECK(!original.apply_poses(std::move(second)) && second.poses().empty());
	apply_without_allocations(owner, std::move(moved));
	CHECK(!owner.apply_poses(std::move(moved)));
	std::optional<prepared_pose_batch> orphan;
	{
		auto       temporary = make_world();
		const auto body      = box(temporary, {0, 3, 0});
		orphan               = plan(temporary, body, {0, 4, 0});
	}
	CHECK(!owner.apply_poses(std::move(*orphan)) && orphan->poses().empty());
	auto faulted = plan(owner, id, {0, 4, 0});
	access::set_faulted(owner, true);
	CHECK(!owner.apply_poses(std::move(faulted)) && !owner.prepare_poses({}));
	access::set_faulted(owner, false);
	auto invalidated = plan(owner, id, {0, 4, 0});
	take(owner.step());
	CHECK(!owner.apply_poses(std::move(invalidated)));
}
// NOLINTEND(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
void epochs() {
	auto       world   = make_world({0, 0, 0});
	const auto id      = box(world, {0, 2, 0});
	const auto dynamic = box(world, {4, 2, 0}, motion_type::dynamic);
	auto       future  = plan(world, id, {0, 3, 0});
	const auto epoch   = world.mutation_epoch();
	CHECK(world.set_pose(id, {0, 2, 0}, {-7, 0, 0, 0}) && world.apply_impulse(dynamic, {0, 0, 0}));
	auto noop = plan(world, id, {0, 2, 0}, {-3, 0, 0, 0});
	apply_without_allocations(world, std::move(noop), false);
	CHECK(world.mutation_epoch() == epoch);
	apply_without_allocations(world, std::move(future));
	auto stale = plan(world, id, {0, 4, 0});
	box(world, {8, 2, 0});
	CHECK(!world.apply_poses(std::move(stale)));
	// Character and impulse mutations also participate in freshness, without becoming batch
	// targets.
	const auto character    = take(world.create_character());
	auto       intent_stale = plan(world, id, {0, 4, 0});
	CHECK(world.set_character_intent(character, {{1, 0, 0}, true}));
	CHECK(!world.apply_poses(std::move(intent_stale)));
	auto cancel_stale = plan(world, id, {0, 4, 0});
	CHECK(world.cancel_character_intent(character));
	CHECK(!world.apply_poses(std::move(cancel_stale)));
	auto teleport_stale = plan(world, id, {0, 4, 0});
	CHECK(world.teleport_character(character, {2, 0, 0}));
	CHECK(!world.apply_poses(std::move(teleport_stale)));
	auto retire_stale = plan(world, id, {0, 4, 0});
	CHECK(world.erase_character(character));
	CHECK(!world.apply_poses(std::move(retire_stale)));
	auto impulse_stale = plan(world, id, {0, 4, 0});
	CHECK(world.apply_impulse(dynamic, {1, 0, 0}));
	CHECK(!world.apply_poses(std::move(impulse_stale)));
	access::set_epoch(world, std::numeric_limits<std::uint64_t>::max() - 1);
	auto last  = plan(world, id, {0, 4, 0});
	auto loser = plan(world, id, {0, 5, 0});
	apply_without_allocations(world, std::move(last));
	CHECK(world.mutation_epoch() == std::numeric_limits<std::uint64_t>::max());
	CHECK(!world.apply_poses(std::move(loser)));
	CHECK(!world.prepare_poses(std::array{body3d_pose_update{id, {0, 5, 0}, {1, 0, 0, 0}}}));
	const auto before = take(world.inspect(id));
	CHECK(!world.set_pose(id, {0, 5, 0}, {1, 0, 0, 0}) && !world.create_body({}) &&
	      !world.erase(id) && !world.step());
	CHECK(world.set_pose(id, before.position, -before.rotation));
	auto empty = take(world.prepare_poses({}));
	apply_without_allocations(world, std::move(empty), false);
	auto equal = plan(world, id, before.position, -before.rotation);
	apply_without_allocations(world, std::move(equal), false);
	CHECK(take(world.inspect(id)).position == before.position);
}
void wake() {
	// An active but stationary neighbor has accumulated native sleep time. Its complete public
	// Jolt serialized state, including the private timer, must stay byte-for-byte unchanged.
	auto               active_world = make_world({0, 0, 0});
	const auto         actor        = box(active_world, {0, 0, 0});
	body3d_description sphere;
	sphere.position   = {1.03f, 0, 0};
	sphere.shape.kind = shape3d_kind::sphere;
	const auto active = take(active_world.create_body(sphere));
	for (int i = 0; i < 12; ++i)
		take(active_world.step());
	CHECK(!take(active_world.inspect(active)).sleeping);
	const auto active_before = access::body_state_hash(active_world, active);
	auto       moved         = plan(active_world, actor, {-10, 0, 0});
	apply_without_allocations(active_world, std::move(moved));
	CHECK(access::body_state_hash(active_world, active) == active_before);
	// Remove support beneath a settled stack. Wake the direct sleeper now; native contact
	// propagation handles the rest of the stack at the next ordinary steps.
	auto               world = make_world();
	body3d_description floor;
	floor.motion     = motion_type::static_body;
	floor.shape.kind = shape3d_kind::plane;
	floor.position   = {0, -5, 0};
	take(world.create_body(floor));
	const auto platform  = box(world, {0, 0, 0}, motion_type::static_body, {3, .5f, 3});
	const auto lower     = box(world, {0, 1.01f, 0}, motion_type::dynamic);
	const auto upper     = box(world, {0, 2.02f, 0}, motion_type::dynamic);
	const auto unrelated = box(world, {12, -4.49f, 0}, motion_type::dynamic);
	for (int i = 0; i < 360; ++i)
		take(world.step());
	CHECK(take(world.inspect(lower)).sleeping && take(world.inspect(upper)).sleeping &&
	      take(world.inspect(unrelated)).sleeping);
	const auto lower_before    = access::body_state_hash(world, lower);
	const auto upper_before    = access::body_state_hash(world, upper);
	const auto unchanged_epoch = world.mutation_epoch();
	auto       equal           = plan(world, platform, {0, 0, 0}, {-7, 0, 0, 0});
	apply_without_allocations(world, std::move(equal), false);
	CHECK(world.set_pose(platform, {0, 0, 0}, {-3, 0, 0, 0}) &&
	      world.mutation_epoch() == unchanged_epoch &&
	      access::body_state_hash(world, lower) == lower_before &&
	      access::body_state_hash(world, upper) == upper_before);
	const auto untouched = access::body_state_hash(world, unrelated);
	const auto tick      = world.tick();
	auto       remove    = plan(world, platform, {6, 0, 0});
	apply_without_allocations(world, std::move(remove));
	CHECK(!take(world.inspect(lower)).sleeping &&
	      access::body_state_hash(world, unrelated) == untouched && world.tick() == tick);
	for (int i = 0; i < 120; ++i)
		take(world.step());
	CHECK(take(world.inspect(lower)).position.y < -.5f &&
	      take(world.inspect(upper)).position.y < .5f);
	std::puts("sleeping stack wake, active-neighbor sleep timer and unrelated-body state retained; "
	          "finalize allocations=0/0");
}
bool same_contacts(std::span<const contact_event> a, const std::vector<contact_event> &b) {
	if (a.size() != b.size())
		return false;
	for (std::size_t i = 0; i < a.size(); ++i)
		if (a[i].phase != b[i].phase || a[i].first != b[i].first || a[i].second != b[i].second ||
		    a[i].sensor != b[i].sensor)
			return false;
	return true;
}
void characters() {
	auto       world     = make_world();
	const auto platform  = box(world, {0, -.5f, 0}, motion_type::static_body, {3, .5f, 3});
	const auto character = take(world.create_character());
	const auto unrelated = box(world, {12, 1, 0}, motion_type::dynamic);
	for (int i = 0; i < 30; ++i)
		take(world.step());
	auto before = take(world.inspect_character(character));
	CHECK(before.ground_body == platform &&
	      before.ground_state == character_ground_state::on_ground);
	CHECK(world.set_character_intent(character, {{1, 0, 0}, true}));
	const auto        character_hash = access::character_state_hash(world, character);
	const auto        body_hash      = access::body_state_hash(world, unrelated);
	const std::vector contacts(world.contacts().begin(), world.contacts().end());
	const auto        tick     = world.tick();
	auto              rejected = plan(world, platform, {8, -.5f, 0});
	access::fail_after_writes(world, 1);
	allocation_probe::begin();
	auto       failure = world.apply_poses(std::move(rejected));
	const auto counts  = allocation_probe::end();
	CHECK(!failure && counts.cpp == 0 && counts.native == 0 &&
	      access::character_state_hash(world, character) == character_hash);
	CHECK(same_contacts(world.contacts(), contacts) &&
	      access::body_state_hash(world, unrelated) == body_hash && world.tick() == tick);
	auto accepted = plan(world, platform, {8, -.5f, 0});
	apply_without_allocations(world, std::move(accepted));
	auto dirty = take(world.inspect_character(character));
	CHECK(dirty.id == before.id && dirty.proxy == before.proxy &&
	      dirty.position == before.position && dirty.linear_velocity == before.linear_velocity &&
	      dirty.jump_pending);
	CHECK(dirty.ground_state == character_ground_state::in_air && !dirty.ground_body.owner &&
	      same_contacts(world.contacts(), contacts));
	take(world.step());
	auto after = take(world.inspect_character(character));
	CHECK(!after.jump_pending && after.position.x > before.position.x &&
	      after.linear_velocity.y < 0);
	CHECK(world.cancel_character_intent(character));
	// A completed in-air tick leaves clean cached support that is not the moving target.
	// Insert the only nearby support without teleport or equal-normal support ambiguity.
	auto       landing_world = make_world();
	const auto new_support =
	    box(landing_world, {8, -.08f, 0}, motion_type::static_body, {3, .1f, 3});
	character3d_description landing_settings;
	landing_settings.position    = {0, 2, 0};
	const auto landing_character = take(landing_world.create_character(landing_settings));
	take(landing_world.step());
	CHECK(landing_world.set_character_intent(landing_character, {{.25f, 0, 0}, false}));
	const auto landing_before = take(landing_world.inspect_character(landing_character));
	const auto landing_hash   = access::character_state_hash(landing_world, landing_character);
	CHECK(!landing_before.ground_body.owner &&
	      landing_before.ground_state == character_ground_state::in_air);
	auto insert_support =
	    plan(landing_world, new_support, landing_before.position + vec3(0, -.1001f, 0));
	apply_without_allocations(landing_world, std::move(insert_support));
	const auto landing_immediate = take(landing_world.inspect_character(landing_character));
	CHECK(landing_immediate.id == landing_before.id &&
	      landing_immediate.proxy == landing_before.proxy &&
	      landing_immediate.position == landing_before.position &&
	      landing_immediate.linear_velocity == landing_before.linear_velocity &&
	      landing_immediate.ground_body == landing_before.ground_body &&
	      access::character_state_hash(landing_world, landing_character) == landing_hash);
	take(landing_world.step());
	const auto landing_after = take(landing_world.inspect_character(landing_character));
	std::printf("new support refresh: old/new/actual slots=%u/%u/%u, ground=%u, feet=(%.6f, "
	            "%.6f, %.6f), previous=(%.6f, %.6f, %.6f)\n",
	            landing_before.ground_body.slot, new_support.slot, landing_after.ground_body.slot,
	            unsigned(landing_after.ground_state), landing_after.position.x,
	            landing_after.position.y, landing_after.position.z, landing_before.position.x,
	            landing_before.position.y, landing_before.position.z);
	CHECK(landing_after.id == landing_before.id && landing_after.proxy == landing_before.proxy &&
	      landing_after.ground_body == new_support &&
	      landing_after.ground_state == character_ground_state::on_ground &&
	      landing_after.position.x > landing_before.position.x);
	std::puts("new support moved beneath unchanged cached character: next-tick refresh and held "
	          "intent preserved without teleport");
}
void prepare_failures() {
	auto             world  = make_world({0, 0, 0});
	const auto       id     = offset_body(world);
	const auto       before = access::body_state_hash(world, id);
	const auto       epoch  = world.mutation_epoch();
	const std::array updates{body3d_pose_update{id, {13, 5, -17}, {1, 0, 0, 0}}};
	allocation_probe::begin();
	auto       prepared = world.prepare_poses(updates);
	const auto baseline = allocation_probe::end();
	CHECK(prepared && baseline.cpp > 0 && baseline.cpp <= 64 && baseline.native == 0);
	for (std::size_t failure = 0; failure < baseline.cpp; ++failure) {
		allocation_probe::fail_after = std::ptrdiff_t(failure);
		auto rejected                = world.prepare_poses(updates);
		allocation_probe::fail_after = -1;
		CHECK(!rejected && access::body_state_hash(world, id) == before &&
		      world.mutation_epoch() == epoch);
	}
	apply_without_allocations(world, std::move(*prepared));
	std::printf("finite preparation failures: %zu/%zu; native prepare allocations=0\n",
	            baseline.cpp, baseline.cpp);
}
} // namespace
int main(int argc, char **argv) {
	try {
		CHECK(argc == 2);
		{ auto warm = make_world(); }
		allocation_probe::install();
		const std::string_view name(argv[1]);
		if (name == "success")
			success();
		else if (name == "validation")
			validation();
		else if (name == "rollback")
			rollback();
		else if (name == "lifetimes")
			lifetimes();
		else if (name == "epochs")
			epochs();
		else if (name == "wake")
			wake();
		else if (name == "characters")
			characters();
		else if (name == "prepare_failures")
			prepare_failures();
		else
			throw std::runtime_error("Unknown pose fixture");
		std::printf("PASS %s\n", argv[1]);
		return 0;
	} catch (const std::exception &error) {
		allocation_probe::tracking   = false;
		allocation_probe::fail_after = -1;
		std::fprintf(stderr, "%s\n", error.what());
		return 1;
	}
}
