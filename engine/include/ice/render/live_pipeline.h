#pragma once
#include "ice/render/device.h"
#include "ice/render/shader_snapshot.h"
#include <cstdint>
#include <memory>

namespace ice::render {
/// Synchronous, render/UI-thread-only pipeline publication owner. Preparation compiles
/// an immutable source and probes the caller's actual bindings without touching presentation.
/// Call activate/undo only at the caller's frame boundary; native submissions retain prior
/// resources. Owns one active, one staged and one undo pipeline, with no event loop or transport.
/// The first activation binds this owner to one logical device. A renderer replacement creates
/// a new holder from retained authored source; old-device staged work cannot migrate implicitly.
/// Validation failures preserve active/history. Allocation exceptions and driver hangs are not
/// contained by this abstraction.
class live_pipeline {
public:
	/// Start empty at revision zero; there is no current pipeline until successful activation.
	live_pipeline();
	/// Release prepared/active/history resources, which independently retain native devices.
	~live_pipeline();
	/// One publication owner cannot be copied.
	live_pipeline(const live_pipeline &other) = delete;
	/// Publication ownership cannot be reassigned by copy.
	live_pipeline &operator=(const live_pipeline &other) = delete;
	/// Transfer all revision/resource ownership; the source becomes an empty reusable holder.
	live_pipeline(live_pipeline &&other) noexcept;
	/// Replace this holder with transferred state; the source becomes empty and reusable.
	live_pipeline &operator=(live_pipeline &&other) noexcept;
	/// Compile exact snapshot bytes and synchronously render a detached 63x31 probe with
	/// current uniforms/image. Reject stale revision or a foreign logical device first.
	/// Indexed options require a same-device probe_mesh; procedural options reject one.
	/// Success replaces only the staged candidate; failure preserves all existing owners.
	/// The caller maintains compatible bindings through activation and later draws; probing
	/// validates supported binding counts/byte sizes, not semantic shader-buffer layout.
	ice::expected<void, diagnostic>
	prepare(device &gpu, const shader_snapshot &source, const std::string &vertex_entry,
	        const std::string &fragment_entry, std::uint64_t expected_revision,
	        std::span<const std::byte> uniforms = {}, const texture *sampled_image = nullptr,
	        pipeline_options options = {}, const mesh *probe_mesh = nullptr);
	/// Publish staged work at an explicit frame boundary, rejecting stale revision/device.
	/// Retain the prior active pipeline for one undo; success returns the new monotonic revision.
	ice::expected<std::uint64_t, diagnostic> activate(device &gpu, std::uint64_t expected_revision);
	/// Restore the retained prior pipeline once. Revision advances and staged work is discarded;
	/// undo does not provide redo. Reject stale revision or a foreign device unchanged.
	ice::expected<std::uint64_t, diagnostic> undo(device &gpu, std::uint64_t expected_revision);
	/// Discard detached preparation without changing the active revision or undo history.
	void discard_prepared();
	/// Borrow current pipeline until the next successful activation/undo or owner destruction.
	const pipeline *current() const;
	/// Borrow the active immutable source snapshot under the same lifetime as current().
	const shader_snapshot *source() const;
	/// Monotonic successful activation/undo count; zero means never activated.
	std::uint64_t revision() const;
	/// Logical device identity of the active pipeline, or zero before first activation.
	std::uint64_t device_identity() const;
	/// Whether detached work is ready for explicit activation.
	bool has_prepared() const;
	/// Whether one previous active pipeline remains available for undo.
	bool can_undo() const;

private:
	struct implementation;
	std::unique_ptr<implementation> implementation_;
};
} // namespace ice::render
