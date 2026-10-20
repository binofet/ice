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
	/// Host registry identity of the participant/controller; zero denotes no owner.
	std::uint64_t identity = 0;
	/// Lifetime generation of that identity; a reused identity must not inherit an old lease.
	std::uint64_t generation                             = 0;
	bool          operator==(const action_owner &) const = default;
};
/// Immutable semantic action-to-channel mapping. Every declared action has exactly one route;
/// channels are derived from these bindings and all start unowned. No event/context bindings.
struct action_channel_binding {
	/// Declared semantic action routed by this binding; each action appears exactly once.
	action_id action = 0;
	/// Nonzero destination channel shared by actions belonging to the same consumer.
	action_channel_id channel = 0;
};
/// Exact configured storage profile, derived from validated declarations. The upstream reducer
/// permits at most 65536 actions; channels <= actions. Each publication has that many regular
/// values and at most that many retiring-owner cancellation values, with one internal cache.
/// Callers independently own any older snapshots they retain; there is no internal history.
struct action_router_profile {
	/// Clock domain accepted by this router; publications must name the same domain.
	action_clock clock = action_clock::simulation;
	/// Number of retained semantic action declarations.
	std::size_t actions = 0;
	/// Number of distinct channels derived from the complete routing declarations.
	std::size_t channels = 0;
};
/// Owned immutable channel sample. Unowned/newly handed-off channels are neutral. A retiring
/// owner receives one neutral cancellation sample; released is set only for previously
/// delivered down buttons. No queued press or delta is forwarded to its replacement.
struct routed_action_channel {
	/// Semantic destination shared by these routed values.
	action_channel_id channel = 0;
	/// Lifetime receiving this sample; the zero owner receives only neutral values.
	action_owner owner;
	/// Router-local channel lease epoch, initially zero; increases on an actual owner change.
	std::uint64_t lease = 0;
	/// Source cancellation, generation change or handoff occurred at this publication boundary.
	bool cancelled = false;
	/// Values for this channel in the retained action-declaration order.
	std::vector<action_value> values;
	/// Borrow an action's value from this immutable sample, or null if absent from this channel.
	const action_value *find(action_id id) const;
};
/// One shared immutable publication for all consumers of one clock tick. Reading this packet
/// does not consume it. The host dispatches each producer once by this clock/tick identity;
/// repeated per-producer/per-eye reads are observation, not another edge delivery.
struct routed_action_frame {
	/// Clock domain of this publication, independent of render-eye or consumer count.
	action_clock clock = action_clock::simulation;
	/// Accepted upstream tick; repeated reads do not deliver another action edge.
	std::uint64_t tick = 0;
	/// Upstream reducer generation used to identify context/lifecycle changes.
	std::uint64_t generation = 0;
	/// Input context of the accepted upstream frame.
	input_context context = input_context::terminal;
	/// Accepted upstream integration duration; the router does not integrate rates again.
	double seconds = 0;
	/// Current channel samples, including neutral samples for channels without an owner.
	std::vector<routed_action_channel> channels;
	/// One cancellation sample for each replaced owned lease, separate from its successor.
	std::vector<routed_action_channel> retirements;
	/// Return only the exact currently owning lifetime, in the matching clock domain. Unowned,
	/// wrong-generation or wrong-clock reads return null. Retirements are explicitly separate.
	const routed_action_channel *find(action_channel_id channel, action_owner owner,
	                                  action_clock clock) const;
};
/// Compare-and-stage one ownership change. The epoch guards the active channel lease rather
/// than a scene revision. desired={0,0} explicitly revokes a lease.
struct action_channel_handoff {
	/// Known destination channel whose owner is to change at the next accepted new tick.
	action_channel_id channel = 0;
	/// Active lease epoch observed before staging; a mismatch rejects the transaction.
	std::uint64_t expected_lease = 0;
	/// New owning lifetime, or the zero owner to revoke the channel.
	action_owner desired;
};
/// Snapshot of one active ownership lease; this conveys no external registry authority.
struct action_channel_lease {
	/// Current owning lifetime; zero means that the channel is unowned.
	action_owner owner;
	/// Router-local epoch incremented only when this channel's owner actually changes.
	std::uint64_t epoch = 0;
};
/// Accepted publication and whether the host must dispatch it as a new clock tick.
struct action_route_result {
	/// Shared immutable packet; an exact repeat returns the previously published packet.
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
	/// Release router-owned staging/cache; independently retained immutable frames remain valid.
	~action_router();
	/// Transfer routing state and leave the source without an active router implementation.
	action_router(action_router &&other) noexcept;
	/// Replace this router's state by moving ownership from another router.
	action_router &operator=(action_router &&other) noexcept;
	action_router(const action_router &)            = delete;
	action_router &operator=(const action_router &) = delete;
	/// Return the validated storage/clock profile, or an empty profile after a move.
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
