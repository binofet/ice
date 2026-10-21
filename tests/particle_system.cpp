#include "ice/particles/system.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>

namespace {
bool deny_allocations = false;
struct deny_heap {
	deny_heap() { deny_allocations = true; }
	~deny_heap() { deny_allocations = false; }
};
} // namespace

// Exercise real allocation failures and prove that steady simulation uses its reserved storage.
void *operator new(std::size_t size) {
	if (deny_allocations)
		throw std::bad_alloc();
	if (void *result = std::malloc(size ? size : 1))
		return result;
	throw std::bad_alloc();
}
void  operator delete(void *memory) noexcept { std::free(memory); }
void *operator new[](std::size_t size) { return ::operator new(size); }
void  operator delete[](void *memory) noexcept { ::operator delete(memory); }
void  operator delete(void *memory, std::size_t) noexcept { ::operator delete(memory); }
void  operator delete[](void *memory, std::size_t) noexcept { ::operator delete(memory); }

#define CHECK(expression)                                                                          \
	do {                                                                                           \
		if (!(expression)) {                                                                       \
			std::fprintf(stderr, "Particles failed %d: %s\n", __LINE__, #expression);              \
			return false;                                                                          \
		}                                                                                          \
	} while (0)

namespace {
using namespace ice::particles;
using particle_system = ice::particles::system;
using ice::math::vec3;
bool           near(float left, float right) { return std::abs(left - right) < .00001f; }
emitter_config fixed_config() {
	auto c         = recipe(preset::smoke);
	c.capacity     = 8;
	c.rate         = 0;
	c.lifetime_min = c.lifetime_max = 1;
	c.position_spread = c.velocity_min = c.velocity_max = vec3(0);
	return c;
}
bool equal_frames(const frame_snapshot &a, const frame_snapshot &b) {
	if (a.tick != b.tick || a.particles.size() != b.particles.size() ||
	    a.batches.size() != b.batches.size())
		return false;
	for (std::size_t i = 0; i < a.particles.size(); ++i) {
		const auto &x = a.particles[i];
		const auto &y = b.particles[i];
		if (x.position != y.position || x.size != y.size || x.color != y.color ||
		    x.rotation != y.rotation || x.normalized_age != y.normalized_age ||
		    x.appearance != y.appearance)
			return false;
	}
	for (std::size_t i = 0; i < a.batches.size(); ++i)
		if (a.batches[i].blending != b.batches[i].blending ||
		    a.batches[i].first != b.batches[i].first || a.batches[i].count != b.batches[i].count)
			return false;
	return true;
}
bool validation_and_capacity() {
	CHECK(!particle_system::create({0, 1}));
	CHECK(!particle_system::create({257, 1}));
	CHECK(!particle_system::create({1, 65537}));
	auto ps = particle_system::create({2, 4});
	CHECK(ps);
	auto c     = fixed_config();
	c.capacity = 4;
	CHECK(!ps->create_emitter({0, 1, 1}, c));
	CHECK(!ps->create_emitter({1, 1, 0}, c));
	auto first = ps->create_emitter({1, 0, 0}, c);
	CHECK(first);
	auto second = ps->create_emitter({1, 2, 1}, c);
	CHECK(second);
	CHECK(!ps->create_emitter({1, 3, 1}, c));
	CHECK(ps->burst(*first, 4));
	CHECK(!ps->burst(*second, 1));
	CHECK(ps->inspect(*first)->alive == 4);
	CHECK(ps->inspect(*second)->alive == 0);
	CHECK(!ps->advance(std::numeric_limits<float>::quiet_NaN()));
	CHECK(!ps->advance(-.1f));
	CHECK(!ps->advance(.251f));
	CHECK(ps->advance(0)->tick == 0);
	for (auto invalid_value :
	     {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN(), -1.0f}) {
		auto bad         = c;
		bad.lifetime_min = invalid_value;
		CHECK(!ps->configure(*first, bad));
		CHECK(ps->inspect(*first)->alive == 4);
	}
	auto bad         = c;
	bad.palette_size = 9;
	CHECK(!ps->configure(*first, bad));
	bad          = c;
	bad.blending = static_cast<blend>(9);
	CHECK(!ps->configure(*first, bad));
	bad             = c;
	bad.color_end.a = 2;
	CHECK(!ps->configure(*first, bad));
	CHECK(!ps->stop(*first, static_cast<stop_mode>(9)));
	CHECK(ps->inspect(*first)->alive == 4);
	return true;
}
bool progression_and_expiry() {
	auto ps = particle_system::create();
	CHECK(ps);
	auto c         = fixed_config();
	c.size_begin   = 1;
	c.size_end     = 3;
	c.color_begin  = {1, .5f, 0, 1};
	c.color_end    = {0, .5f, 1, 0};
	c.velocity_min = c.velocity_max = {2, 0, 0};
	c.acceleration                  = {0, -4, 0};
	c.spin_min = c.spin_max = {1, 2, 3};
	auto id                 = ps->create_emitter({1, 1, 1}, c);
	CHECK(id);
	CHECK(ps->burst(*id, 1));
	auto before = ps->capture();
	CHECK(before);
	auto report = ps->advance(.25f);
	CHECK(report);
	CHECK(report->alive == 1 && report->spawned == 0 && report->expired == 0);
	auto after = ps->capture();
	CHECK(after);
	const auto &p = after->particles[0];
	CHECK(near(p.position.x, .5f) && near(p.position.y, -.125f));
	CHECK(near(p.size, 1.5f) && near(p.normalized_age, .25f));
	CHECK(near(p.color.r, .75f) && near(p.color.a, .75f));
	CHECK(p.rotation != before->particles[0].rotation);
	for (int i = 0; i < 3; ++i)
		CHECK(ps->advance(.25f));
	CHECK(ps->inspect(*id)->alive == 0);
	CHECK(ps->capture()->particles.empty());
	CHECK(before->particles[0].position == vec3(0));
	return true;
}
bool attachment_and_generations() {
	auto ps = particle_system::create();
	CHECK(ps);
	auto c   = fixed_config();
	c.origin = {1, 0, 0};
	auto id  = ps->create_emitter({1, 0, 0}, c);
	CHECK(id);
	ice::math::mat4 world(1);
	world[3].x = 10;
	CHECK(!ps->attach(*id, {2, 2, 1}, world));
	CHECK(ps->attach(*id, {1, 2, 1}, world));
	CHECK(ps->burst(*id, 1));
	world[3].x = 20;
	CHECK(ps->update_attachment({1, 2, 1}, world) == 1);
	CHECK(ps->burst(*id, 1));
	auto frame = ps->capture();
	CHECK(frame);
	CHECK(frame->particles[0].position.x == 11 && frame->particles[1].position.x == 21);
	CHECK(ps->detach(*id));
	world[3].x = 30;
	CHECK(ps->update_attachment({1, 2, 1}, world) == 0);
	CHECK(ps->burst(*id, 1));
	CHECK(ps->capture()->particles[2].position.x == 21);
	CHECK(ps->attach(*id, {1, 2, 1}, world));
	world[3][3] = 0;
	CHECK(!ps->update_attachment({1, 2, 1}, world));
	CHECK(ps->destroy_owner({1, 2, 2}) == 0);
	CHECK(ps->destroy_owner({1, 2, 1}) == 1);
	CHECK(!ps->inspect(*id));
	auto replacement = ps->create_emitter({1, 2, 2}, c);
	CHECK(replacement);
	CHECK(replacement->slot == id->slot && replacement->generation != id->generation);
	CHECK(!ps->start(*id));
	CHECK(!ps->destroy(*id));
	CHECK(ps->inspect(*replacement));
	CHECK(frame->particles[0].position.x == 11);
	return true;
}
bool continuous_budget_and_stop_modes() {
	auto ps = particle_system::create({4, 8});
	CHECK(ps);
	auto c     = fixed_config();
	c.rate     = 16;
	c.capacity = 4;
	auto a     = ps->create_emitter({1, 1, 1}, c);
	CHECK(a);
	auto b = ps->create_emitter({1, 2, 1}, c);
	CHECK(b);
	CHECK(ps->start(*a));
	CHECK(ps->start(*a));
	CHECK(ps->start(*b));
	auto first = ps->advance(.25f);
	CHECK(first && first->spawned == 8 && first->alive == 8 && first->dropped == 0);
	auto saturated = ps->advance(.25f);
	CHECK(saturated && saturated->alive == 8 && saturated->dropped == 8);
	CHECK(ps->stop(*a));
	CHECK(ps->stop(*b, stop_mode::clear));
	CHECK(ps->inspect(*a)->alive == 4 && ps->inspect(*b)->alive == 0);
	for (int i = 0; i < 3; ++i)
		CHECK(ps->advance(.25f));
	CHECK(ps->capture()->particles.empty());
	CHECK(ps->start(*b));
	CHECK(ps->advance(.25f)->spawned == 4);
	CHECK(ps->stop(*b, stop_mode::clear));
	CHECK(ps->stop(*b, stop_mode::clear));
	CHECK(ps->capture()->particles.empty());
	return true;
}
bool deterministic_failure_is_atomic() {
	auto a = particle_system::create({2, 128});
	auto b = particle_system::create({2, 128});
	CHECK(a && b);
	auto c = recipe(preset::confetti);
	c.seed = 37;
	auto x = a->create_emitter({1, 1, 1}, c);
	auto y = b->create_emitter({1, 1, 1}, c);
	CHECK(x && y);
	CHECK(!a->burst(*x, 129));
	CHECK(a->burst(*x, 96));
	CHECK(b->burst(*y, 96));
	for (int i = 0; i < 8; ++i) {
		CHECK(a->advance(.125f));
		CHECK(b->advance(.125f));
	}
	auto af = a->capture();
	auto bf = b->capture();
	CHECK(af && bf && equal_frames(*af, *bf));
	CHECK(af->particles.size() == 96);
	bool different_colors = false;
	for (const auto &p : af->particles)
		different_colors |= p.color != af->particles[0].color;
	CHECK(different_colors);
	return true;
}
bool replacement_session_and_moves() {
	auto ps = particle_system::create();
	CHECK(ps);
	auto c = fixed_config();
	auto a = ps->create_emitter({1, 1, 1}, c);
	auto b = ps->create_emitter({2, 1, 1}, c);
	CHECK(a && b);
	CHECK(ps->burst(*a, 3));
	CHECK(ps->burst(*b, 2));
	CHECK(ps->start(*a));
	CHECK(ps->configure(*a, recipe(preset::boost_fire)));
	CHECK(ps->inspect(*a)->alive == 0 && !ps->inspect(*a)->emitting);
	CHECK(ps->destroy_session(1) == 1 && !ps->inspect(*a));
	CHECK(ps->inspect(*b)->alive == 2);
	auto moved = std::move(*ps);
	CHECK(moved.inspect(*b));
	CHECK(!ps->inspect(*b));
	auto other = particle_system::create();
	CHECK(other && !other->inspect(*b));
	*other = std::move(moved);
	CHECK(other->inspect(*b));
	other->clear();
	CHECK(!other->inspect(*b));
	auto fresh = other->create_emitter({2, 1, 2}, c);
	CHECK(fresh && *fresh != *b);
	CHECK(other->destroy_session(0) == 0);
	return true;
}
bool owned_snapshot_and_optional_control_counts() {
	frame_snapshot retained;
	{
		auto ps = particle_system::create();
		CHECK(ps);
		emitter_control &control = *ps;
		auto             a       = control.create_emitter({1, 0, 0}, fixed_config());
		auto             b       = control.create_emitter({1, 2, 1}, recipe(preset::boost_fire));
		CHECK(a && b);
		CHECK(control.burst(*a, 2));
		CHECK(control.burst(*b, 3));
		CHECK(control.inspect(*a)->alive.has_value());
		auto frame = ps->capture();
		CHECK(frame);
		retained = std::move(*frame);
		CHECK(retained.batches.size() == 2 && retained.particles.size() == 5);
		CHECK(retained.batches[0].blending == blend::alpha && retained.batches[0].first == 0 &&
		      retained.batches[0].count == 2);
		CHECK(retained.batches[1].blending == blend::additive && retained.batches[1].first == 2 &&
		      retained.batches[1].count == 3);
		control.clear();
		CHECK(ps->capture()->particles.empty());
	}
	CHECK(retained.particles.size() == 5 && retained.particles[0].color.a > 0);
	return true;
}
bool allocation_failure_and_bounded_mobile_work() {
	{
		deny_heap guard;
		auto      failed = particle_system::create();
		CHECK(!failed && failed.error().code == error::allocation);
	}
	auto ps = particle_system::create({4, 512});
	CHECK(ps);
	auto c     = recipe(preset::boost_fire);
	c.capacity = 512;
	c.rate     = 4096;
	auto id    = ps->create_emitter({1, 1, 1}, c);
	CHECK(id);
	{
		deny_heap guard;
		CHECK(ps->start(*id));
		for (int i = 0; i < 120; ++i) {
			auto step = ps->advance(1.0f / 60);
			CHECK(step && step->alive <= 512);
		}
		auto failed = ps->capture();
		CHECK(!failed && failed.error().code == error::allocation);
		CHECK(ps->stop(*id, stop_mode::clear));
		CHECK(ps->configure(*id, c));
		CHECK(ps->burst(*id, 32));
		CHECK(ps->destroy_session(1) == 1);
	}
	CHECK(ps->capture()->particles.empty());
	return true;
}
} // namespace

int main() {
	static_assert(!std::is_copy_constructible_v<particle_system>);
	static_assert(std::is_nothrow_move_constructible_v<particle_system>);
	static_assert(std::is_abstract_v<ice::particles::emitter_control>);
	const struct {
		const char *name;
		bool (*run)();
	} scenarios[] = {
	    {"validation and capacity", validation_and_capacity},
	    {"lifetime size color gravity spin", progression_and_expiry},
	    {"birth attachment and exact generations", attachment_and_generations},
	    {"continuous drops and stop/drain/clear", continuous_budget_and_stop_modes},
	    {"deterministic confetti and atomic rejection", deterministic_failure_is_atomic},
	    {"replacement session cleanup and moves", replacement_session_and_moves},
	    {"owned blend snapshots and neutral control plane",
	     owned_snapshot_and_optional_control_counts},
	    {"allocation failures and bounded mobile simulation",
	     allocation_failure_and_bounded_mobile_work},
	};
	for (const auto &scenario : scenarios) {
		if (!scenario.run())
			return 1;
		std::printf("PASS %s\n", scenario.name);
	}
	return 0;
}
