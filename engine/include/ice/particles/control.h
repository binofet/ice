#pragma once

#include "ice/core/expected.h"
#include "ice/math/types.h"
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace ice::particles {

/// Process-local system, one-based slot and non-wrapping generation; never a native pointer.
struct emitter_id {
	/// Unique backend-instance lifetime; zero is invalid.
	std::uint64_t owner = 0;
	/// One-based bounded emitter slot; zero is invalid.
	std::uint32_t slot = 0;
	/// Non-wrapping slot lifetime checked by every operation.
	std::uint64_t generation = 0;
	/// Compare only the complete opaque identity, without a state lookup.
	bool operator==(const emitter_id &) const = default;
};

/// Host-supplied lifetime. A session-only owner has object=generation=0.
/// Hosts must retire exact object generations and sessions; numeric object IDs alone are unsafe.
struct owner_key {
	/// Opaque host session lifetime, nonzero.
	std::uint64_t session = 0;
	/// Host object identity; zero denotes a session-only effect.
	std::uint64_t object = 0;
	/// Exact object lifetime; zero only when object is zero.
	std::uint64_t generation = 0;
	/// Compare only the complete opaque identity, without a state lookup.
	bool operator==(const owner_key &) const = default;
};

/// Expected rejection, including exhausted capacity; it does not invalidate a driving controller.
enum class error { invalid_argument, stale_emitter, capacity, allocation, exhausted_identity };
/// Static-lifetime explanation; reporting a failure requires no heap allocation.
struct diagnostic {
	/// Machine-readable reason for rejection.
	error code;
	/// Non-owning view of a static engine literal.
	std::string_view message;
};
/// Artist-facing built-in recipe and render appearance; the core creates no render resources.
enum class preset { smoke, boost_fire, confetti };
/// Linear, straight-alpha input; the renderer owns sorting, depth and actual blend implementation.
enum class blend { alpha, additive };
/// Drain stops births and lets existing particles expire; clear also removes them immediately.
enum class stop_mode { drain, clear };

/// Fixed storage budgets allocated at system creation; hard ceilings are 256/65536.
struct limits {
	/// Maximum simultaneously retained emitter handles, in [1,256].
	std::uint32_t emitters = 64;
	/// Maximum live CPU particles, in [1,65536]; hosts may choose lower mobile limits.
	std::uint32_t particles = 4096;
};

/// Owned, allocation-free recipe. Distances/sizes are world units; time is seconds.
/// Initial positions/velocities are transformed at birth; existing particles stay in world space.
struct emitter_config {
	/// Builtin render shape/preset label; it does not select or promise a compute backend.
	preset appearance = preset::smoke;
	/// Blend bucket; values are straight-alpha linear color.
	blend blending = blend::alpha;
	/// Per-emitter cap, at most the system particle budget.
	std::uint32_t capacity = 256;
	/// Deterministic seed; zero normalizes to 0x9e3779b9.
	std::uint32_t seed = 1;
	/// Continuous births per second, in [0,4096]; zero is burst-only.
	float rate = 16;
	/// Minimum sampled lifetime, in [.001,60] seconds.
	float lifetime_min = .8f;
	/// Maximum sampled lifetime, at least lifetime_min and at most 60 seconds.
	float lifetime_max = 1.6f;
	/// Initial world-space diameter, in [.001,100].
	float size_begin = .1f;
	/// Diameter at expiry, in [0,100].
	float size_end = .45f;
	/// Local emitter origin, transformed once at birth.
	ice::math::vec3 origin{0};
	/// Nonnegative local half-extents of sampled birth jitter, at most 100.
	ice::math::vec3 position_spread{.02f};
	/// Minimum local birth velocity, each component in [-1000,1000].
	ice::math::vec3 velocity_min{-.1f, .2f, -.1f};
	/// Maximum local birth velocity; each component is at least velocity_min.
	ice::math::vec3 velocity_max{.1f, .4f, .1f};
	/// World-space acceleration, each component in [-1000,1000].
	ice::math::vec3 acceleration{0};
	/// Minimum XYZ angular velocity, radians/second, in [-100,100].
	ice::math::vec3 spin_min{0};
	/// Maximum XYZ angular velocity, each component at least spin_min.
	ice::math::vec3 spin_max{0};
	/// Initial linear straight-alpha RGBA, each component in [0,1].
	ice::math::vec4 color_begin{.4f, .4f, .4f, .5f};
	/// Expiry linear straight-alpha RGBA, each component in [0,1].
	ice::math::vec4 color_end{.4f, .4f, .4f, 0};
	/// Optional multiplicative RGB palette, chosen once at birth; alpha is still interpolated.
	std::array<ice::math::vec3, 8> palette{};
	/// Number of active palette entries, in [0,8]; zero disables palette tinting.
	std::uint32_t palette_size = 0;
};

/// Return a finite smoke, short-lived boost fire or one-shot colored confetti recipe.
emitter_config recipe(preset appearance) noexcept;

/// Copied inspection data, not a mutable provider view or an engine/scene pointer.
struct emitter_snapshot {
	/// Exact emitter generation copied from the control handle.
	emitter_id id;
	/// Creation owner, retained until explicit retirement.
	owner_key owner;
	/// Optional same-session object lifetime followed for new births.
	std::optional<owner_key> attachment;
	/// Owned copy of the current validated recipe.
	emitter_config config;
	/// Whether continuous births are enabled.
	bool emitting = false;
	/// Exact CPU count; other backends may omit it instead of reading GPU state back.
	std::optional<std::uint32_t> alive;
};
/// Backend-neutral host-thread control plane, with no CPU vector or GPU readback requirement.
/// Implementations own effect resources; retirement preserves already submitted GPU lifetimes.
class emitter_control {
public:
	/// Release implementation ownership; submitted resources follow backend completion lifetimes.
	virtual ~emitter_control() = default;
	/// Accept creation of a stopped bounded effect for an exact host lifetime.
	virtual ice::expected<emitter_id, diagnostic>
	create_emitter(owner_key owner, const emitter_config &config) noexcept = 0;
	/// Replace recipe, clear all old particles and stop births. Rejection changes nothing.
	virtual ice::expected<void, diagnostic> configure(emitter_id            id,
	                                                  const emitter_config &config) noexcept = 0;
	/// Idempotently enable continuous births. A zero-rate emitter still supports burst().
	virtual ice::expected<void, diagnostic> start(emitter_id id) noexcept = 0;
	/// Stop births and either drain naturally or clear immediately; repeated stop is safe.
	virtual ice::expected<void, diagnostic> stop(emitter_id id,
	                                             stop_mode  mode = stop_mode::drain) noexcept = 0;
	/// Accept one finite burst control or reject without partially committing it.
	/// GPU backends need not synchronously read back the resulting live particle count.
	virtual ice::expected<void, diagnostic> burst(emitter_id id, std::uint32_t count) noexcept = 0;
	/// Bind a same-session object generation to a validated affine transform, applied at birth.
	virtual ice::expected<void, diagnostic> attach(emitter_id id, owner_key object,
	                                               const ice::math::mat4 &world) noexcept = 0;
	/// Remove tracking, retaining the last transform for future births; existing particles stay
	/// put.
	virtual ice::expected<void, diagnostic> detach(emitter_id id) noexcept = 0;
	/// Host resolver supplies the current transform; no user callback is executed by the engine.
	virtual ice::expected<std::uint32_t, diagnostic>
	update_attachment(owner_key object, const ice::math::mat4 &world) noexcept = 0;
	/// Retire the handle and all its particles immediately.
	virtual ice::expected<void, diagnostic> destroy(emitter_id id) noexcept = 0;
	/// Remove emitters created by or attached to this exact lifetime, including draining particles.
	virtual std::uint32_t destroy_owner(owner_key owner) noexcept = 0;
	/// Remove the session's emitters/particles; unrelated sessions remain unchanged.
	virtual std::uint32_t destroy_session(std::uint64_t session) noexcept = 0;
	/// Remove every emitter/particle; old IDs remain invalid if slots are reused.
	virtual void clear() noexcept = 0;
	/// Read one exact emitter generation; no allocation or mutation.
	virtual ice::expected<emitter_snapshot, diagnostic> inspect(emitter_id id) const noexcept = 0;
	/// Fixed capacity chosen at creation; moved-from systems return zero limits.
	virtual limits budget() const noexcept = 0;
};

} // namespace ice::particles
