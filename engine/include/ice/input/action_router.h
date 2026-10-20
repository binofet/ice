#pragma once
#include "ice/input/actions.h"
#include <memory>
#include <optional>
#include <span>

namespace ice::input {
/// Author-assigned semantic channel identity; zero is invalid. Names/labels grant no authority.
using action_channel_id = std::uint32_t;
/// The clock under which the upstream reducer integrated this frame's rate values. Routing
/// never reintegrates or resamples. Simulation and presentation require explicitly matched
/// frames; a render eye or another producer is not another clock tick.
enum class action_clock { simulation, presentation };
/// Host-owned controller/participant lifetime. The router compares both fields exactly but
/// cannot authenticate labels or validate an external controller registry. {0,0} is unowned;
/// every owned lifetime requires nonzero identity and generation.
struct action_owner {
	std::uint64_t identity = 0, generation = 0;
	bool          operator==(const action_owner &) const = default;
};
/// Immutable semantic action-to-channel mapping. Every declared action has exactly one route;
/// channels are derived from these bindings and all start unowned. No event/context bindings.
struct action_channel_binding {
	action_id         action  = 0;
	action_channel_id channel = 0;
};
/// Exact configured storage profile, derived from validated declarations. The upstream reducer
/// permits at most 65536 actions; channels <= actions. Each publication has that many regular
/// values and at most that many retiring-owner cancellation values, with one internal cache.
/// Callers independently own any older snapshots they retain; there is no internal history.
struct action_router_profile {
	action_clock clock   = action_clock::simulation;
	std::size_t  actions = 0, channels = 0;
};
/// Owned immutable channel sample. Unowned/newly handed-off channels are neutral. A retiring
/// owner receives one neutral cancellation sample; released is set only for previously
/// delivered down buttons. No queued press or delta is forwarded to its replacement.
struct routed_action_channel {
	action_channel_id channel = 0;
	action_owner      owner;
	/// Router-local channel lease epoch, initially zero; increases on an actual owner change.
	std::uint64_t             lease     = 0;
	bool                      cancelled = false;
	std::vector<action_value> values;
	const action_value       *find(action_id id) const;
};
/// One shared immutable publication for all consumers of one clock tick. Reading this packet
/// does not consume it. The host dispatches each producer once by this clock/tick identity;
/// repeated per-producer/per-eye reads are observation, not another edge delivery.
struct routed_action_frame {
	action_clock                       clock = action_clock::simulation;
	std::uint64_t                      tick = 0, generation = 0;
	input_context                      context = input_context::terminal;
	double                             seconds = 0;
	std::vector<routed_action_channel> channels, retirements;
	/// Return only the exact currently owning lifetime, in the matching clock domain. Unowned,
	/// wrong-generation or wrong-clock reads return null. Retirements are explicitly separate.
	const routed_action_channel *find(action_channel_id channel, action_owner owner,
	                                  action_clock clock) const;
};
/// Compare-and-stage one ownership change. The epoch guards the active channel lease rather
/// than a scene revision. desired={0,0} explicitly revokes a lease.
struct action_channel_handoff {
	action_channel_id channel        = 0;
	std::uint64_t     expected_lease = 0;
	action_owner      desired;
};
struct action_channel_lease {
	action_owner  owner;
	std::uint64_t epoch = 0;
};
struct action_route_result {
	std::shared_ptr<const routed_action_frame> frame;
	/// False for an exact repeated publication: same pointer/identity, no handoff, cancellation
	/// or edge is dispatched again. Consumers must not execute again when advanced is false.
	bool advanced = false;
};
/// Portable owner-thread routing of one already reduced immutable action_frame. No native
/// events, global context changes, controller execution, presentation or security authority.
/// Allocation may throw; failed create/stage/publish leaves existing ownership/cache intact.
class action_router {
public:
	/// Validate a complete closed profile, retaining descriptor order. Clock choice is required.
	/// The limits match the upstream published action profile; storage follows actual counts.
	static ice::expected<action_router, action_diagnostic>
	create(std::vector<action_descriptor> declarations, std::vector<action_channel_binding> routes,
	       action_clock clock);
	~action_router();
	action_router(action_router &&other) noexcept;
	action_router &operator=(action_router &&other) noexcept;
	action_router(const action_router &)                   = delete;
	action_router        &operator=(const action_router &) = delete;
	action_router_profile profile() const noexcept;
	/// Observe the active lease; pending staging never changes this sample. Unknown channels
	/// return nullopt. This contains no action values or execution/presentation policy.
	std::optional<action_channel_lease> lease(action_channel_id channel) const;
	/// Atomically replace the entire pending transaction. Validate all active epochs, known
	/// channels, distinct changes and owner spellings before replacing any prior pending batch.
	/// Empty clears staging. Nothing takes effect until the next accepted strictly newer frame.
	ice::expected<void, action_diagnostic>
	stage_handoffs(std::span<const action_channel_handoff> changes);
	/// seconds must be the same finite [0,1] duration used by the upstream reducer. Rate values
	/// are already integrated. Wrong clock, malformed/incomplete frame, backwards tick or a
	/// conflicting repeated tick rejects without consuming staging or the previous cache.
	/// Exact repeats return the same cache with advanced=false, even with a pending handoff.
	/// On a changed channel, discard its boundary-frame input, cancel only its retiring owner,
	/// and publish neutral to the new owner. Held state/rate actions require observed neutral
	/// before rearming; a zero-duration rate sample cannot establish neutral. Delta actions may
	/// deliver fresh values on the next frame. Unchanged channels retain their actual source
	/// values, including fresh input after a source cancellation; they are not globally reset.
	ice::expected<action_route_result, action_diagnostic>
	publish(const action_frame &source, action_clock clock, double seconds);

private:
	struct implementation;
	explicit action_router(std::unique_ptr<implementation> state);
	std::unique_ptr<implementation> state_;
};
} // namespace ice::input
