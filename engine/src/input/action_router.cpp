#include "ice/input/action_router.h"
#include "ice/core/utf8.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <type_traits>
#include <utility>

namespace ice::input {
namespace {
constexpr std::size_t maximum_actions   = 65536; // Published action_reducer ceiling.
constexpr double      maximum_component = 1e9;
action_diagnostic     reject(std::string message) { return {std::move(message)}; }
bool                  valid(action_clock value) {
    return value == action_clock::simulation || value == action_clock::presentation;
}
bool valid(action_owner value) { return (value.identity == 0) == (value.generation == 0); }
bool owned(action_owner value) { return value.identity != 0; }
bool same(const action_value &a, const action_value &b) {
	return a.id == b.id && a.kind == b.kind && a.behavior == b.behavior && a.x == b.x &&
	       a.y == b.y && a.down == b.down && a.pressed == b.pressed && a.released == b.released;
}
bool same(const action_frame &a, const action_frame &b) {
	return a.tick == b.tick && a.context == b.context && a.generation == b.generation &&
	       a.cancelled == b.cancelled && a.values.size() == b.values.size() &&
	       std::equal(a.values.begin(), a.values.end(), b.values.begin(),
	                  [](const auto &x, const auto &y) { return same(x, y); });
}
action_value neutral(action_value value, bool release = false) {
	value.x = value.y = 0;
	value.down = value.pressed = false;
	value.released             = release;
	return value;
}
bool neutral_level(const action_value &value, double seconds) {
	if (value.behavior == action_behavior::rate && seconds == 0)
		return false;
	return !value.down && value.x == 0 && value.y == 0;
}
} // namespace

struct action_router::implementation {
	struct channel_state {
		action_channel_id id = 0;
		action_owner      owner;
		std::uint64_t     lease = 0;
		std::size_t       count = 0;
	};
	action_router_profile                      profile;
	std::vector<action_descriptor>             declarations;
	std::vector<std::size_t>                   routes;
	std::vector<channel_state>                 channels;
	std::vector<bool>                          armed;
	std::vector<action_channel_handoff>        pending;
	std::optional<action_frame>                previous;
	double                                     seconds = 0;
	std::shared_ptr<const routed_action_frame> cache;
	std::size_t                                index(action_channel_id id) const {
        auto found =
            std::lower_bound(channels.begin(), channels.end(), id,
		                                                    [](const auto &channel, auto key) { return channel.id < key; });
        return found != channels.end() && found->id == id ? std::size_t(found - channels.begin())
		                                                                                 : channels.size();
	}
};

const action_value *routed_action_channel::find(action_id id) const {
	for (const auto &value : values)
		if (value.id == id)
			return &value;
	return nullptr;
}
const routed_action_channel *routed_action_frame::find(action_channel_id channel,
                                                       action_owner      owner,
                                                       action_clock      expected_clock) const {
	if (expected_clock != this->clock || !owned(owner))
		return nullptr;
	for (const auto &sample : channels)
		if (sample.channel == channel && sample.owner == owner)
			return &sample;
	return nullptr;
}
action_router::action_router(std::unique_ptr<implementation> state) : state_(std::move(state)) {}
action_router::~action_router()                                         = default;
action_router::action_router(action_router &&other) noexcept            = default;
action_router &action_router::operator=(action_router &&other) noexcept = default;

ice::expected<action_router, action_diagnostic>
action_router::create(std::vector<action_descriptor>      declarations,
                      std::vector<action_channel_binding> routes, action_clock clock) {
	if (!valid(clock) || declarations.empty() || declarations.size() > maximum_actions ||
	    routes.size() != declarations.size())
		return ice::unexpected(reject("Invalid action routing clock/profile size"));
	std::map<action_id, std::size_t> ids;
	std::set<std::string>            names;
	for (std::size_t index = 0; index < declarations.size(); ++index) {
		const auto &item = declarations[index];
		if (!item.id || item.name.empty() || !core::valid_utf8_text(item.name, 128) ||
		    item.kind < action_kind::button || item.kind > action_kind::axis2 ||
		    item.behavior < action_behavior::state || item.behavior > action_behavior::rate ||
		    (item.kind == action_kind::button && item.behavior != action_behavior::state) ||
		    !ids.emplace(item.id, index).second || !names.insert(item.name).second)
			return ice::unexpected(reject("Invalid or duplicate action routing declaration"));
	}
	std::set<action_channel_id> channels;
	std::set<action_id>         routed;
	for (const auto &binding : routes)
		if (!binding.channel || !ids.contains(binding.action) ||
		    !routed.insert(binding.action).second)
			return ice::unexpected(reject("Unknown, duplicate or unowned semantic action route"));
		else
			channels.insert(binding.channel);
	auto state          = std::make_unique<implementation>();
	state->profile      = {clock, declarations.size(), channels.size()};
	state->declarations = std::move(declarations);
	state->routes.resize(state->declarations.size());
	state->armed.resize(state->declarations.size(), false);
	state->channels.reserve(channels.size());
	for (auto id : channels)
		state->channels.push_back({id, {}, 0, 0});
	for (const auto &binding : routes) {
		const auto index                      = state->index(binding.channel);
		state->routes[ids.at(binding.action)] = index;
		++state->channels[index].count;
	}
	return action_router(std::move(state));
}
action_router_profile action_router::profile() const noexcept {
	return state_ ? state_->profile : action_router_profile{};
}
std::optional<action_channel_lease> action_router::lease(action_channel_id channel) const {
	if (!state_)
		return std::nullopt;
	const auto index = state_->index(channel);
	if (index == state_->channels.size())
		return std::nullopt;
	return action_channel_lease{state_->channels[index].owner, state_->channels[index].lease};
}
ice::expected<void, action_diagnostic>
action_router::stage_handoffs(std::span<const action_channel_handoff> changes) {
	if (!state_ || changes.size() > state_->channels.size())
		return ice::unexpected(reject("Unavailable router or oversized handoff transaction"));
	std::set<action_channel_id> seen;
	for (const auto &change : changes) {
		const auto index = state_->index(change.channel);
		if (index == state_->channels.size() || !valid(change.desired) ||
		    !seen.insert(change.channel).second)
			return ice::unexpected(reject("Unknown channel, invalid owner or duplicate handoff"));
		const auto &channel = state_->channels[index];
		if (change.expected_lease != channel.lease ||
		    (change.desired != channel.owner &&
		     channel.lease == std::numeric_limits<std::uint64_t>::max()))
			return ice::unexpected(reject("Stale or exhausted action channel lease"));
	}
	std::vector<action_channel_handoff> next(changes.begin(), changes.end());
	state_->pending.swap(next);
	return {};
}
ice::expected<action_route_result, action_diagnostic>
action_router::publish(const action_frame &source, action_clock clock, double seconds) {
	if (!state_ || !valid(clock) || clock != state_->profile.clock || !std::isfinite(seconds) ||
	    seconds < 0 || seconds > 1 || source.generation == 0 ||
	    source.context < input_context::terminal || source.context > input_context::player ||
	    source.values.size() != state_->declarations.size())
		return ice::unexpected(reject("Invalid action routing frame, clock or duration"));
	for (std::size_t index = 0; index < source.values.size(); ++index) {
		const auto &value    = source.values[index];
		const auto &declared = state_->declarations[index];
		if (value.id != declared.id || value.kind != declared.kind ||
		    value.behavior != declared.behavior || !std::isfinite(value.x) ||
		    !std::isfinite(value.y) || std::abs(value.x) > maximum_component ||
		    std::abs(value.y) > maximum_component ||
		    (value.kind != action_kind::axis2 && value.y != 0) ||
		    (value.kind == action_kind::button && value.x != (value.down ? 1 : 0)) ||
		    (value.kind != action_kind::button &&
		     (value.down || value.pressed || value.released)) ||
		    (value.behavior == action_behavior::rate && seconds == 0 &&
		     (value.x != 0 || value.y != 0)))
			return ice::unexpected(
			    reject("Action frame differs from the configured scalar profile"));
	}
	if (state_->previous) {
		if (source.tick < state_->previous->tick ||
		    source.generation < state_->previous->generation)
			return ice::unexpected(reject("Action routing frame is stale"));
		if (source.tick == state_->previous->tick) {
			if (seconds != state_->seconds || !same(source, *state_->previous))
				return ice::unexpected(
				    reject("Repeated action tick has conflicting source contents"));
			return action_route_result{state_->cache, false};
		}
	}
	// All fallible construction uses private copies; even allocation failure leaves the
	// accepted source, handoff transaction, arming and published lease epochs unchanged.
	auto              next_channels = state_->channels;
	auto              next_armed    = state_->armed;
	std::vector<bool> changed(next_channels.size(), false);
	for (const auto &change : state_->pending) {
		const auto index   = state_->index(change.channel);
		auto      &channel = next_channels[index];
		if (change.expected_lease != channel.lease)
			return ice::unexpected(reject("Pending action handoff no longer matches its lease"));
		if (change.desired == channel.owner)
			continue;
		channel.owner = change.desired;
		++channel.lease;
		changed[index] = true;
	}
	auto frame        = std::make_shared<routed_action_frame>();
	frame->clock      = clock;
	frame->tick       = source.tick;
	frame->generation = source.generation;
	frame->context    = source.context;
	frame->seconds    = seconds;
	frame->channels.reserve(next_channels.size());
	frame->retirements.reserve(next_channels.size());
	const bool cancelled =
	    source.cancelled || (state_->previous && source.generation != state_->previous->generation);
	for (std::size_t index = 0; index < next_channels.size(); ++index) {
		const auto &channel = next_channels[index];
		frame->channels.push_back(
		    {channel.id, channel.owner, channel.lease, cancelled || changed[index], {}});
		frame->channels.back().values.reserve(channel.count);
		if (changed[index] && owned(state_->channels[index].owner)) {
			const auto           &old = state_->cache->channels[index];
			routed_action_channel retirement{old.channel, old.owner, old.lease, true, {}};
			retirement.values.reserve(channel.count);
			for (const auto &value : old.values)
				retirement.values.push_back(
				    neutral(value, value.kind == action_kind::button && value.down));
			frame->retirements.push_back(std::move(retirement));
		}
	}
	for (std::size_t index = 0; index < source.values.size(); ++index) {
		const auto  channel_index = state_->routes[index];
		auto       &channel       = frame->channels[channel_index];
		const auto &value         = source.values[index];
		auto        routed        = neutral(value);
		if (changed[channel_index])
			next_armed[index] = false;
		if (!owned(channel.owner))
			next_armed[index] = false;
		else if (changed[channel_index])
			next_armed[index] = neutral_level(value, seconds);
		else if (value.behavior == action_behavior::delta || next_armed[index])
			routed = value;
		else if (neutral_level(value, seconds))
			next_armed[index] = true;
		channel.values.push_back(routed);
	}
	auto previous = source;
	static_assert(std::is_nothrow_move_assignable_v<action_frame>);
	static_assert(std::is_nothrow_move_constructible_v<action_frame>);
	state_->channels.swap(next_channels);
	state_->armed.swap(next_armed);
	state_->previous = std::move(previous);
	state_->seconds  = seconds;
	state_->cache    = std::move(frame);
	state_->pending.clear();
	return action_route_result{state_->cache, true};
}
} // namespace ice::input
