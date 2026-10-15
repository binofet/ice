#include "ice/render/live_pipeline.h"
#include <limits>
#include <utility>

namespace ice::render {
namespace {
ice::unexpected<diagnostic> failure(const char *message) {
	return ice::unexpected(diagnostic{message});
}
struct prepared_state {
	std::unique_ptr<pipeline> gpu_pipeline;
	shader_snapshot           source;
	std::uint64_t             device_identity, base_revision;
};
} // namespace
struct live_pipeline::implementation {
	std::unique_ptr<prepared_state> active, staged, previous;
	std::uint64_t                   revision = 0;
};
live_pipeline::live_pipeline() : implementation_(std::make_unique<implementation>()) {}
live_pipeline::~live_pipeline()                                         = default;
live_pipeline::live_pipeline(live_pipeline &&other) noexcept            = default;
live_pipeline &live_pipeline::operator=(live_pipeline &&other) noexcept = default;
ice::expected<void, diagnostic>
live_pipeline::prepare(device &gpu, const shader_snapshot &source, const std::string &vertex_entry,
                       const std::string &fragment_entry, std::uint64_t expected_revision,
                       std::span<const std::byte> uniforms, const texture *sampled_image,
                       pipeline_options options, const mesh *probe_mesh) {
	if (!implementation_)
		implementation_ = std::make_unique<implementation>();
	auto &state = *implementation_;
	if (expected_revision != state.revision)
		return failure("Stale pipeline revision during preparation");
	if (state.active && state.active->device_identity != gpu.identity())
		return failure("Pipeline holder belongs to another logical device");
	auto candidate = gpu.prepare_pipeline(source, vertex_entry, fragment_entry, options);
	if (!candidate)
		return ice::unexpected(candidate.error());
	const draw_item item{candidate->get(), probe_mesh, uniforms, sampled_image};
	auto            probe = gpu.render_offscreen(frame_batch{std::span(&item, 1)}, 63, 31);
	if (!probe)
		return ice::unexpected(probe.error());
	auto prepared = std::make_unique<prepared_state>(
	    prepared_state{std::move(*candidate), source, gpu.identity(), state.revision});
	state.staged = std::move(prepared);
	return {};
}
ice::expected<std::uint64_t, diagnostic> live_pipeline::activate(device       &gpu,
                                                                 std::uint64_t expected_revision) {
	if (!implementation_)
		return failure("No prepared pipeline to activate");
	auto &state = *implementation_;
	if (expected_revision != state.revision ||
	    (state.staged && state.staged->base_revision != state.revision))
		return failure("Stale pipeline revision during activation");
	if (!state.staged)
		return failure("No prepared pipeline to activate");
	if (state.staged->device_identity != gpu.identity() ||
	    (state.active && state.active->device_identity != gpu.identity()))
		return failure("Prepared pipeline belongs to another logical device");
	if (state.revision == std::numeric_limits<std::uint64_t>::max())
		return failure("Pipeline revision exhausted");
	state.previous = std::move(state.active);
	state.active   = std::move(state.staged);
	return ++state.revision;
}
ice::expected<std::uint64_t, diagnostic> live_pipeline::undo(device       &gpu,
                                                             std::uint64_t expected_revision) {
	if (!implementation_)
		return failure("No prior pipeline to restore");
	auto &state = *implementation_;
	if (expected_revision != state.revision)
		return failure("Stale pipeline revision during undo");
	if (!state.previous)
		return failure("No prior pipeline to restore");
	if (state.previous->device_identity != gpu.identity())
		return failure("Undo pipeline belongs to another logical device");
	if (state.revision == std::numeric_limits<std::uint64_t>::max())
		return failure("Pipeline revision exhausted");
	state.active = std::move(state.previous);
	state.staged.reset();
	return ++state.revision;
}
void live_pipeline::discard_prepared() {
	if (implementation_)
		implementation_->staged.reset();
}
const pipeline *live_pipeline::current() const {
	return implementation_ && implementation_->active ? implementation_->active->gpu_pipeline.get()
	                                                  : nullptr;
}
const shader_snapshot *live_pipeline::source() const {
	return implementation_ && implementation_->active ? &implementation_->active->source : nullptr;
}
std::uint64_t live_pipeline::revision() const {
	return implementation_ ? implementation_->revision : 0;
}
std::uint64_t live_pipeline::device_identity() const {
	return implementation_ && implementation_->active ? implementation_->active->device_identity
	                                                  : 0;
}
bool live_pipeline::has_prepared() const {
	return implementation_ && bool(implementation_->staged);
}
bool live_pipeline::can_undo() const { return implementation_ && bool(implementation_->previous); }
} // namespace ice::render
