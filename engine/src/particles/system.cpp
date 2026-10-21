#include "ice/particles/system.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <new>
#include <utility>

namespace ice::particles {
namespace {
constexpr float world_limit = 1000000;
constexpr float tau         = 6.2831853071795864769f;
diagnostic      invalid() { return {error::invalid_argument, "Invalid particle argument"}; }
diagnostic      stale() { return {error::stale_emitter, "Retired particle emitter"}; }
diagnostic      full() { return {error::capacity, "Particle budget exhausted"}; }
bool            finite(float value, float minimum, float maximum) {
    return std::isfinite(value) && value >= minimum && value <= maximum;
}
bool vector_valid(ice::math::vec3 value, float minimum, float maximum) {
	return finite(value.x, minimum, maximum) && finite(value.y, minimum, maximum) &&
	       finite(value.z, minimum, maximum);
}
bool color_valid(ice::math::vec4 value) {
	return vector_valid(ice::math::vec3(value), 0, 1) && finite(value.a, 0, 1);
}
bool owner_valid(owner_key value) {
	return value.session != 0 && ((value.object == 0 && value.generation == 0) ||
	                              (value.object != 0 && value.generation != 0));
}
bool config_valid(const emitter_config &c, limits budget) {
	if ((c.appearance != preset::smoke && c.appearance != preset::boost_fire &&
	     c.appearance != preset::confetti) ||
	    (c.blending != blend::alpha && c.blending != blend::additive) || c.capacity == 0 ||
	    c.capacity > budget.particles || c.palette_size > c.palette.size() ||
	    !finite(c.rate, 0, 4096) || !finite(c.lifetime_min, .001f, 60) ||
	    !finite(c.lifetime_max, c.lifetime_min, 60) || !finite(c.size_begin, .001f, 100) ||
	    !finite(c.size_end, 0, 100) || !vector_valid(c.origin, -world_limit, world_limit) ||
	    !vector_valid(c.position_spread, 0, 100) || !vector_valid(c.velocity_min, -1000, 1000) ||
	    !vector_valid(c.velocity_max, -1000, 1000) || !vector_valid(c.acceleration, -1000, 1000) ||
	    !vector_valid(c.spin_min, -100, 100) || !vector_valid(c.spin_max, -100, 100) ||
	    !color_valid(c.color_begin) || !color_valid(c.color_end))
		return false;
	for (int axis = 0; axis != 3; ++axis)
		if (c.velocity_min[axis] > c.velocity_max[axis] || c.spin_min[axis] > c.spin_max[axis])
			return false;
	for (std::uint32_t i = 0; i < c.palette_size; ++i)
		if (!vector_valid(c.palette[i], 0, 1))
			return false;
	return true;
}
bool transform_valid(const ice::math::mat4 &world, const emitter_config &c) {
	for (int column = 0; column != 4; ++column)
		for (int row = 0; row != 4; ++row)
			if (!finite(world[column][row], -world_limit, world_limit))
				return false;
	if (world[0][3] != 0 || world[1][3] != 0 || world[2][3] != 0 || world[3][3] != 1)
		return false;
	const auto center = ice::math::transform_point(world, c.origin);
	for (int row = 0; row != 3; ++row) {
		float radius = 0;
		float speed  = 0;
		for (int column = 0; column != 3; ++column) {
			if (std::abs(world[column][row]) > 100)
				return false;
			radius += std::abs(world[column][row]) * c.position_spread[column];
			speed += std::abs(world[column][row]) *
			         std::max(std::abs(c.velocity_min[column]), std::abs(c.velocity_max[column]));
		}
		if (!finite(center[row], -world_limit + radius, world_limit - radius) || speed > 100000)
			return false;
	}
	return true;
}
std::uint64_t next_identity() {
	static std::atomic<std::uint64_t> next{1};
	auto                              current = next.load(std::memory_order_relaxed);
	while (current != std::numeric_limits<std::uint64_t>::max())
		if (next.compare_exchange_weak(current, current + 1, std::memory_order_relaxed))
			return current;
	return 0;
}
float random_unit(std::uint32_t &state) {
	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;
	return static_cast<float>(state >> 8) * (1.0f / 16777216.0f);
}
float random_range(std::uint32_t &state, float minimum, float maximum) {
	return minimum + random_unit(state) * (maximum - minimum);
}
ice::math::vec3 random_vector(std::uint32_t &state, ice::math::vec3 minimum,
                              ice::math::vec3 maximum) {
	// Explicit evaluation order is part of seeded reproducibility.
	ice::math::vec3 result;
	for (int axis = 0; axis != 3; ++axis)
		result[axis] = random_range(state, minimum[axis], maximum[axis]);
	return result;
}
} // namespace

emitter_config recipe(preset appearance) noexcept {
	emitter_config c;
	c.appearance = appearance;
	if (appearance == preset::boost_fire) {
		c.blending     = blend::additive;
		c.rate         = 96;
		c.lifetime_min = .12f;
		c.lifetime_max = .35f;
		c.size_begin   = .12f;
		c.size_end     = .025f;
		c.velocity_min = {-.15f, -.1f, -2};
		c.velocity_max = {.15f, .1f, -1};
		c.color_begin  = {1, .8f, .15f, .9f};
		c.color_end    = {1, .05f, 0, 0};
	} else if (appearance == preset::confetti) {
		c.rate         = 0;
		c.capacity     = 128;
		c.lifetime_min = 1.8f;
		c.lifetime_max = 3;
		c.size_begin   = .05f;
		c.size_end     = .04f;
		c.velocity_min = {-1.5f, 2, -1.5f};
		c.velocity_max = {1.5f, 4, 1.5f};
		c.acceleration = {0, -3, 0};
		c.spin_min     = {-6, -6, -6};
		c.spin_max     = {6, 6, 6};
		c.color_begin  = {1, 1, 1, 1};
		c.color_end    = {1, 1, 1, 0};
		c.palette_size = 5;
		c.palette[0]   = {1, .1f, .1f};
		c.palette[1]   = {1, .85f, .05f};
		c.palette[2]   = {.1f, .85f, .25f};
		c.palette[3]   = {.1f, .35f, 1};
		c.palette[4]   = {1, .1f, .7f};
	}
	return c;
}

struct system::implementation {
	struct slot {
		bool                     active     = false;
		bool                     emitting   = false;
		std::uint64_t            generation = 0;
		owner_key                owner;
		std::optional<owner_key> attachment;
		emitter_config           config;
		ice::math::mat4          world{1};
		std::uint32_t            random = 1;
		std::uint32_t            alive  = 0;
		double                   carry  = 0;
	};
	struct particle {
		std::uint32_t   slot_index;
		float           age = 0;
		float           lifetime;
		ice::math::vec3 position, velocity, acceleration, rotation, spin;
		ice::math::vec4 color_begin, color_end;
		float           size_begin, size_end;
		preset          appearance;
		blend           blending;
	};
	limits                budget;
	std::uint64_t         identity = 0;
	std::uint64_t         tick     = 0;
	std::vector<slot>     slots;
	std::vector<particle> particles;
	slot                 *find(emitter_id id) noexcept {
        if (id.owner != identity || id.slot == 0 || id.slot > slots.size())
            return nullptr;
        auto &s = slots[id.slot - 1];
        return s.active && s.generation == id.generation ? &s : nullptr;
	}
	void erase_particles(std::uint32_t index) noexcept {
		std::erase_if(particles, [index](const particle &p) { return p.slot_index == index; });
		slots[index].alive = 0;
	}
	void retire(std::uint32_t index) noexcept {
		erase_particles(index);
		auto &s  = slots[index];
		s.active = s.emitting = false;
		s.attachment.reset();
		s.carry = 0;
	}
	std::uint32_t available(const slot &s) const noexcept {
		return std::min(s.config.capacity - s.alive,
		                budget.particles - static_cast<std::uint32_t>(particles.size()));
	}
	void emit(std::uint32_t index, std::uint32_t count) noexcept {
		auto       &s = slots[index];
		const auto &c = s.config;
		for (std::uint32_t i = 0; i < count; ++i) {
			particle p;
			p.slot_index = index;
			p.lifetime   = random_range(s.random, c.lifetime_min, c.lifetime_max);
			const auto local =
			    c.origin + random_vector(s.random, -c.position_spread, c.position_spread);
			p.position = ice::math::transform_point(s.world, local);
			p.velocity = ice::math::transform_direction(
			    s.world, random_vector(s.random, c.velocity_min, c.velocity_max));
			p.acceleration = c.acceleration;
			p.rotation     = random_vector(s.random, ice::math::vec3(0), ice::math::vec3(tau));
			p.spin         = random_vector(s.random, c.spin_min, c.spin_max);
			p.color_begin  = c.color_begin;
			p.color_end    = c.color_end;
			if (c.palette_size) {
				const auto choice = static_cast<std::uint32_t>(random_unit(s.random) *
				                                               static_cast<float>(c.palette_size));
				p.color_begin *= ice::math::vec4(c.palette[choice], 1);
				p.color_end *= ice::math::vec4(c.palette[choice], 1);
			}
			p.size_begin = c.size_begin;
			p.size_end   = c.size_end;
			p.appearance = c.appearance;
			p.blending   = c.blending;
			particles.push_back(p); // Reserved up front; every caller checks both capacities.
		}
		s.alive += count;
	}
};

system::system(std::unique_ptr<implementation> state) noexcept : state_(std::move(state)) {}
system::~system()                                                            = default;
system::system(system &&other) noexcept                                      = default;
system                           &system::operator=(system &&other) noexcept = default;
ice::expected<system, diagnostic> system::create(limits budget) noexcept {
	if (budget.emitters == 0 || budget.emitters > 256 || budget.particles == 0 ||
	    budget.particles > 65536)
		return ice::unexpected(invalid());
	try {
		auto state    = std::make_unique<implementation>();
		state->budget = budget;
		state->slots.resize(budget.emitters);
		state->particles.reserve(budget.particles);
		state->identity = next_identity();
		if (!state->identity)
			return ice::unexpected(
			    diagnostic{error::exhausted_identity, "Particle identity exhausted"});
		return system(std::move(state));
	} catch (const std::bad_alloc &) {
		return ice::unexpected(diagnostic{error::allocation, "Particle allocation failed"});
	}
}
ice::expected<emitter_id, diagnostic>
system::create_emitter(owner_key owner, const emitter_config &config) noexcept {
	if (!state_ || !owner_valid(owner) || !config_valid(config, state_->budget) ||
	    !transform_valid(ice::math::mat4(1), config))
		return ice::unexpected(invalid());
	for (std::uint32_t index = 0; index < state_->slots.size(); ++index) {
		auto &s = state_->slots[index];
		if (s.active || s.generation == std::numeric_limits<std::uint64_t>::max())
			continue;
		const auto generation = s.generation + 1;
		s                     = implementation::slot{};
		s.generation          = generation;
		s.owner               = owner;
		s.config              = config;
		s.active              = true;
		s.random              = config.seed ? config.seed : 0x9e3779b9u;
		return emitter_id{state_->identity, index + 1, s.generation};
	}
	return ice::unexpected(full());
}
ice::expected<void, diagnostic> system::configure(emitter_id            id,
                                                  const emitter_config &config) noexcept {
	auto *s = state_ ? state_->find(id) : nullptr;
	if (!s)
		return ice::unexpected(stale());
	if (!config_valid(config, state_->budget) || !transform_valid(s->world, config))
		return ice::unexpected(invalid());
	state_->erase_particles(id.slot - 1);
	s->config   = config;
	s->emitting = false;
	s->carry    = 0;
	s->random   = config.seed ? config.seed : 0x9e3779b9u;
	return {};
}
ice::expected<void, diagnostic> system::start(emitter_id id) noexcept {
	auto *s = state_ ? state_->find(id) : nullptr;
	if (!s)
		return ice::unexpected(stale());
	s->emitting = true;
	return {};
}
ice::expected<void, diagnostic> system::stop(emitter_id id, stop_mode mode) noexcept {
	auto *s = state_ ? state_->find(id) : nullptr;
	if (!s)
		return ice::unexpected(stale());
	if (mode != stop_mode::drain && mode != stop_mode::clear)
		return ice::unexpected(invalid());
	s->emitting = false;
	s->carry    = 0;
	if (mode == stop_mode::clear)
		state_->erase_particles(id.slot - 1);
	return {};
}
ice::expected<void, diagnostic> system::burst(emitter_id id, std::uint32_t count) noexcept {
	auto *s = state_ ? state_->find(id) : nullptr;
	if (!s)
		return ice::unexpected(stale());
	if (count > state_->available(*s))
		return ice::unexpected(full());
	state_->emit(id.slot - 1, count);
	return {};
}
ice::expected<void, diagnostic> system::attach(emitter_id id, owner_key object,
                                               const ice::math::mat4 &world) noexcept {
	auto *s = state_ ? state_->find(id) : nullptr;
	if (!s)
		return ice::unexpected(stale());
	if (!owner_valid(object) || object.object == 0 || object.session != s->owner.session ||
	    !transform_valid(world, s->config))
		return ice::unexpected(invalid());
	s->attachment = object;
	s->world      = world;
	return {};
}
ice::expected<void, diagnostic> system::detach(emitter_id id) noexcept {
	auto *s = state_ ? state_->find(id) : nullptr;
	if (!s)
		return ice::unexpected(stale());
	s->attachment.reset();
	return {};
}
ice::expected<std::uint32_t, diagnostic>
system::update_attachment(owner_key object, const ice::math::mat4 &world) noexcept {
	if (!state_ || !owner_valid(object) || object.object == 0)
		return ice::unexpected(invalid());
	for (const auto &s : state_->slots)
		if (s.active && s.attachment == object && !transform_valid(world, s.config))
			return ice::unexpected(invalid());
	std::uint32_t count = 0;
	for (auto &s : state_->slots)
		if (s.active && s.attachment == object) {
			s.world = world;
			++count;
		}
	return count;
}
ice::expected<void, diagnostic> system::destroy(emitter_id id) noexcept {
	if (!state_ || !state_->find(id))
		return ice::unexpected(stale());
	state_->retire(id.slot - 1);
	return {};
}
std::uint32_t system::destroy_owner(owner_key owner) noexcept {
	if (!state_ || !owner_valid(owner))
		return 0;
	std::uint32_t count = 0;
	for (std::uint32_t i = 0; i < state_->slots.size(); ++i)
		if (const auto &s = state_->slots[i];
		    s.active && (s.owner == owner || s.attachment == owner)) {
			state_->retire(i);
			++count;
		}
	return count;
}
std::uint32_t system::destroy_session(std::uint64_t session) noexcept {
	if (!state_ || session == 0)
		return 0;
	std::uint32_t count = 0;
	for (std::uint32_t i = 0; i < state_->slots.size(); ++i)
		if (state_->slots[i].active && state_->slots[i].owner.session == session) {
			state_->retire(i);
			++count;
		}
	return count;
}
void system::clear() noexcept {
	if (!state_)
		return;
	state_->particles.clear();
	for (auto &s : state_->slots) {
		s.active = s.emitting = false;
		s.alive               = 0;
		s.attachment.reset();
		s.carry = 0;
	}
}
ice::expected<step_report, diagnostic> system::advance(float delta_seconds) noexcept {
	if (!state_ || !finite(delta_seconds, 0, .25f))
		return ice::unexpected(invalid());
	step_report report{.tick  = state_->tick,
	                   .alive = static_cast<std::uint32_t>(state_->particles.size())};
	if (delta_seconds == 0)
		return report;
	if (state_->tick == std::numeric_limits<std::uint64_t>::max())
		return ice::unexpected(diagnostic{error::exhausted_identity, "Particle tick exhausted"});
	report.tick      = ++state_->tick;
	std::size_t kept = 0;
	for (std::size_t index = 0; index < state_->particles.size(); ++index) {
		auto &p = state_->particles[index];
		p.age += delta_seconds;
		p.position +=
		    p.velocity * delta_seconds + .5f * p.acceleration * delta_seconds * delta_seconds;
		p.velocity += p.acceleration * delta_seconds;
		for (int axis = 0; axis != 3; ++axis)
			p.rotation[axis] = std::fmod(p.rotation[axis] + p.spin[axis] * delta_seconds, tau);
		if (p.age < p.lifetime && vector_valid(p.position, -world_limit, world_limit)) {
			if (kept != index)
				state_->particles[kept] = p;
			++kept;
		} else {
			--state_->slots[p.slot_index].alive;
			++report.expired;
		}
	}
	state_->particles.resize(kept);
	for (std::uint32_t index = 0; index < state_->slots.size(); ++index) {
		auto &s = state_->slots[index];
		if (!s.active || !s.emitting)
			continue;
		const double desired   = s.carry + static_cast<double>(s.config.rate) * delta_seconds;
		const auto   requested = static_cast<std::uint32_t>(std::floor(desired));
		s.carry                = desired - requested;
		const auto count       = std::min(requested, state_->available(s));
		state_->emit(index, count);
		report.spawned += count;
		report.dropped += requested - count;
	}
	report.alive = static_cast<std::uint32_t>(state_->particles.size());
	return report;
}
ice::expected<emitter_snapshot, diagnostic> system::inspect(emitter_id id) const noexcept {
	const auto *s = state_ ? state_->find(id) : nullptr;
	if (!s)
		return ice::unexpected(stale());
	return emitter_snapshot{id, s->owner, s->attachment, s->config, s->emitting, s->alive};
}
ice::expected<frame_snapshot, diagnostic> system::capture() const noexcept {
	if (!state_)
		return ice::unexpected(stale());
	try {
		frame_snapshot frame;
		frame.tick = state_->tick;
		frame.particles.reserve(state_->particles.size());
		frame.batches.reserve(2);
		for (auto blending : {blend::alpha, blend::additive}) {
			const auto first = static_cast<std::uint32_t>(frame.particles.size());
			for (const auto &p : state_->particles) {
				if (p.blending != blending)
					continue;
				const float age = std::clamp(p.age / p.lifetime, 0.0f, 1.0f);
				frame.particles.push_back({p.position,
				                           p.size_begin + (p.size_end - p.size_begin) * age,
				                           p.color_begin + (p.color_end - p.color_begin) * age,
				                           p.rotation, age, p.appearance});
			}
			const auto count = static_cast<std::uint32_t>(frame.particles.size()) - first;
			if (count)
				frame.batches.push_back({blending, first, count});
		}
		return frame;
	} catch (const std::bad_alloc &) {
		return ice::unexpected(diagnostic{error::allocation, "Particle frame allocation failed"});
	}
}
limits system::budget() const noexcept { return state_ ? state_->budget : limits{0, 0}; }

} // namespace ice::particles
