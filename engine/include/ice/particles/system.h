#pragma once

#include "ice/particles/control.h"
#include <memory>
#include <vector>

namespace ice::particles {

/// Immutable frame value: interpolated world position/size/linear straight-alpha color.
/// Rotation is XYZ radians (billboards may use Z); normalized_age is in [0,1).
struct render_particle {
	/// World-space center, detached from later parent motion.
	ice::math::vec3 position{0};
	/// Interpolated world-space diameter.
	float size = 0;
	/// Interpolated linear straight-alpha RGBA.
	ice::math::vec4 color{0};
	/// XYZ radians; chip renderers can use all axes, billboards may use Z.
	ice::math::vec3 rotation{0};
	/// Lifetime fraction in [0,1), without backend state references.
	float normalized_age = 0;
	/// Smoke/fire/chip appearance label for the initial renderer.
	preset appearance = preset::smoke;
};
/// Contiguous particle range; at most two blend buckets, no per-emitter draw requirement.
struct render_batch {
	/// Required blend bucket for this contiguous range.
	blend blending;
	/// First zero-based index in the owned particle vector.
	std::uint32_t first;
	/// Number of particles in this range.
	std::uint32_t count;
};
/// Owns all frame bytes. Remains valid after simulation, replacement or system destruction.
struct frame_snapshot {
	/// CPU simulation tick when these owned values were copied.
	std::uint64_t tick = 0;
	/// Owned samples; later simulation calls never overwrite them.
	std::vector<render_particle> particles;
	/// At most two nonempty blend ranges covering all samples.
	std::vector<render_batch> batches;
};
/// Actual work at one explicit step; full pools drop continuous births instead of growing.
struct step_report {
	/// Completed CPU step, unchanged by a zero-duration call.
	std::uint64_t tick = 0;
	/// Actual retained particles after expiry and emission.
	std::uint32_t alive = 0;
	/// Births actually admitted this step.
	std::uint32_t spawned = 0;
	/// Particles retired by lifetime or leaving the world envelope.
	std::uint32_t expired = 0;
	/// Continuous births rejected by global or emitter capacity this step.
	std::uint32_t dropped = 0;
};

/// Single-host-thread bounded CPU simulation. No callbacks, physics bodies, scene objects or GPU.
/// Expected validation/capacity/allocation failures are returned; they do not pause a world.
class system final : public emitter_control {
public:
	/// Preallocate all simulation storage; invalid budgets/allocation fail without a partial
	/// system.
	static ice::expected<system, diagnostic> create(limits budget = {}) noexcept;
	/// Retire CPU state; all previously captured frames remain independently owned.
	~system() override;
	/// Transfer the complete backend lifetime and handles; the source becomes unavailable.
	system(system &&) noexcept;
	/// Retire this backend and transfer the source lifetime; previous handles become stale.
	system &operator=(system &&) noexcept;
	/// A mutable simulation has one owner.
	system(const system &) = delete;
	/// Do not duplicate a backend lifetime or its handles.
	system &operator=(const system &) = delete;
	/// Create a stopped emitter owned by an exact host lifetime. No allocation after create().
	ice::expected<emitter_id, diagnostic>
	create_emitter(owner_key owner, const emitter_config &config) noexcept override;
	/// Replace recipe, clear all old particles and stop births. Rejection changes nothing.
	ice::expected<void, diagnostic> configure(emitter_id            id,
	                                          const emitter_config &config) noexcept override;
	/// Idempotently enable continuous births. A zero-rate emitter still supports burst().
	ice::expected<void, diagnostic> start(emitter_id id) noexcept override;
	/// Stop births and either drain naturally or clear immediately; repeated stop is safe.
	ice::expected<void, diagnostic> stop(emitter_id id,
	                                     stop_mode  mode = stop_mode::drain) noexcept override;
	/// Atomically emit exactly count particles or reject capacity without consuming RNG/state.
	ice::expected<void, diagnostic> burst(emitter_id id, std::uint32_t count) noexcept override;
	/// Bind a same-session object generation to a validated affine transform, applied at birth.
	ice::expected<void, diagnostic> attach(emitter_id id, owner_key object,
	                                       const ice::math::mat4 &world) noexcept override;
	/// Remove tracking, retaining the last transform for future births; existing particles stay
	/// put.
	ice::expected<void, diagnostic> detach(emitter_id id) noexcept override;
	/// Host resolver supplies the current transform; no user callback is executed by the engine.
	ice::expected<std::uint32_t, diagnostic>
	update_attachment(owner_key object, const ice::math::mat4 &world) noexcept override;
	/// Retire the handle and all its particles immediately.
	ice::expected<void, diagnostic> destroy(emitter_id id) noexcept override;
	/// Remove emitters created by or attached to this exact lifetime, including draining particles.
	std::uint32_t destroy_owner(owner_key owner) noexcept override;
	/// Remove the session's emitters/particles; unrelated sessions remain unchanged.
	std::uint32_t destroy_session(std::uint64_t session) noexcept override;
	/// Remove every emitter/particle; old IDs remain invalid if slots are reused.
	void clear() noexcept override;
	/// Step by finite [0,.25] seconds; zero is a no-op. No allocation or callbacks occur.
	/// Repeated identical seeds/configuration/step sequences produce identical particles.
	ice::expected<step_report, diagnostic> advance(float delta_seconds) noexcept;
	/// Read one exact emitter generation; no allocation or mutation.
	ice::expected<emitter_snapshot, diagnostic> inspect(emitter_id id) const noexcept override;
	/// Copy bounded data into owned frame storage. Failure leaves simulation unchanged.
	ice::expected<frame_snapshot, diagnostic> capture() const noexcept;
	/// Fixed capacity chosen at creation; moved-from systems return zero limits.
	limits budget() const noexcept override;

private:
	struct implementation;
	explicit system(std::unique_ptr<implementation> state) noexcept;
	std::unique_ptr<implementation> state_;
};

} // namespace ice::particles
