#include "ice/input/actions.h"
#include "ice/core/utf8.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <utility>

namespace ice::input {
namespace {
constexpr double maximum_component = 1e9;
bool             valid(input_context context) {
    return context >= input_context::terminal && context <= input_context::player;
}
bool valid(action_kind kind) { return kind >= action_kind::button && kind <= action_kind::axis2; }
bool valid(action_behavior behavior) {
	return behavior >= action_behavior::state && behavior <= action_behavior::rate;
}
bool bounded(double value) { return std::isfinite(value) && std::abs(value) <= maximum_component; }
action_diagnostic diagnostic(std::string message) { return {std::move(message)}; }
source_id         allocate_source() {
    static std::atomic<source_id> next{1};
    auto                          value = next.load(std::memory_order_relaxed);
    while (value != std::numeric_limits<source_id>::max())
        if (next.compare_exchange_weak(value, value + 1, std::memory_order_relaxed))
            return value;
    return 0;
}
action_kind input_shape(const action_binding &binding) {
	if (binding.control == control_kind::virtual_control)
		return binding.virtual_kind;
	if (binding.control == control_kind::physical_key ||
	    binding.control == control_kind::mouse_button)
		return action_kind::button;
	return action_kind::axis2;
}
std::uint64_t control_token(control_kind kind, std::uint32_t code = 0) {
	return (static_cast<std::uint64_t>(kind) << 32) | code;
}
} // namespace

struct action_reducer::implementation {
	struct contribution {
		double x = 0, y = 0;
		bool   armed = false, active = false;
	};
	struct binding_state {
		action_binding                    config;
		std::size_t                       target = 0;
		std::map<source_id, contribution> sources;
	};
	struct button_state {
		bool                                          down = false, released = false;
		std::set<std::pair<source_id, std::uint64_t>> presses;
	};
	platform::window_id                   window = 0;
	std::vector<action_descriptor>        descriptors;
	std::vector<binding_state>            bindings;
	std::vector<std::vector<std::size_t>> action_bindings;
	std::vector<button_state>             buttons;
	std::set<source_id>                   sources;
	action_limits                         limits;
	input_context                         context = input_context::terminal;
	bool                                  focused = false, cancelled = false, tick_valid = false;
	std::uint64_t                         generation = 1, previous_tick = 0;
	std::size_t                           contributions = 0;

	void next_generation() {
		if (generation != std::numeric_limits<std::uint64_t>::max())
			++generation;
		cancelled = true;
	}
	void refresh_buttons(std::optional<std::pair<source_id, std::uint64_t>> origin = {}) {
		for (std::size_t index = 0; index < descriptors.size(); ++index) {
			if (descriptors[index].kind != action_kind::button)
				continue;
			bool down = false;
			if (focused)
				for (auto binding_index : action_bindings[index]) {
					const auto &binding = bindings[binding_index];
					if (binding.config.context == context)
						for (const auto &[source, value] : binding.sources) {
							(void)source;
							down |= value.armed && value.x != 0;
						}
				}
			const bool previous = buttons[index].down;
			buttons[index].down = down;
			if (down && origin && (!previous || !buttons[index].presses.empty()))
				for (auto binding_index : action_bindings[index]) {
					const auto &binding = bindings[binding_index];
					const auto &config  = binding.config;
					const auto  code    = config.control == control_kind::physical_key
					                          ? static_cast<std::uint32_t>(config.key)
					                      : config.control == control_kind::mouse_button
					                          ? static_cast<std::uint32_t>(config.button)
					                          : config.virtual_control;
					auto        value   = binding.sources.find(origin->first);
					if (config.context == context &&
					    control_token(config.control, code) == origin->second &&
					    value != binding.sources.end() && value->second.armed &&
					    value->second.x != 0) {
						// Another source sharing an unconsumed aggregate press also owns
						// that edge. Retiring the first source must not erase the survivor.
						buttons[index].presses.insert(*origin);
						break;
					}
				}
			buttons[index].released |= previous && !down;
		}
	}
	void reset(source_id source, bool all) {
		for (auto &button : buttons)
			std::erase_if(button.presses, [source, all](const auto &origin) {
				return all || origin.first == source;
			});
		for (auto &binding : bindings)
			for (auto &[id, value] : binding.sources)
				if (all || id == source) {
					value.armed  = false;
					value.active = false;
					if (descriptors[binding.target].behavior == action_behavior::delta)
						value.x = value.y = 0;
				}
		next_generation();
		refresh_buttons();
	}
	void update(binding_state &binding, contribution &value, double x, double y, bool active,
	            bool native_fresh, const std::array<double, 2> &unit_scale = {1, 1}) {
		const auto &descriptor = descriptors[binding.target];
		const auto  shape      = input_shape(binding.config);
		if (!active) {
			value = {};
			return;
		}
		value.active = true;
		if (binding.config.control == control_kind::virtual_control &&
		    descriptor.behavior != action_behavior::delta && shape != action_kind::button) {
			double magnitude = shape == action_kind::axis2 ? std::hypot(x, y) : std::abs(x);
			if (magnitude <= binding.config.deadzone)
				x = y = 0;
			else {
				double factor = (std::min(magnitude, 1.) - binding.config.deadzone) /
				                ((1 - binding.config.deadzone) * magnitude);
				x *= factor;
				y *= factor;
			}
		}
		if ((x == 0 && y == 0) || native_fresh)
			value.armed = true;
		if (!focused)
			value.armed = false;
		if (descriptor.kind == action_kind::button) {
			value.x = x;
			value.y = 0;
			return;
		}
		x *= unit_scale[0];
		y *= unit_scale[1];
		if (descriptor.kind == action_kind::axis1 && shape == action_kind::axis2)
			x = binding.config.component == axis_component::x ? x : y;
		if (shape != action_kind::axis2)
			y = x;
		x *= binding.config.scale[0];
		y = descriptor.kind == action_kind::axis2 ? y * binding.config.scale[1] : 0;
		if (descriptor.behavior == action_behavior::delta) {
			if (focused && binding.config.context == context) {
				value.x     = std::clamp(value.x + x, -maximum_component, maximum_component);
				value.y     = std::clamp(value.y + y, -maximum_component, maximum_component);
				value.armed = true;
			}
		} else {
			value.x = x;
			value.y = y;
		}
	}
};

const action_value *action_frame::find(action_id id) const {
	for (const auto &value : values)
		if (value.id == id)
			return &value;
	return nullptr;
}
action_reducer::action_reducer(std::unique_ptr<implementation> state) : state_(std::move(state)) {}
action_reducer::~action_reducer()                                          = default;
action_reducer::action_reducer(action_reducer &&other) noexcept            = default;
action_reducer &action_reducer::operator=(action_reducer &&other) noexcept = default;

ice::expected<action_reducer, action_diagnostic>
action_reducer::create(platform::window_id window, std::vector<action_descriptor> descriptors,
                       std::vector<action_binding> bindings, const action_limits &limits) {
	if (!limits.actions || limits.actions > 65536 || !limits.bindings ||
	    limits.bindings > 1048576 || !limits.sources || limits.sources > 65536 ||
	    !limits.contributions || limits.contributions > 1048576 || descriptors.empty() ||
	    descriptors.size() > limits.actions || bindings.size() > limits.bindings)
		return ice::unexpected(diagnostic("Invalid action configuration/storage limits"));
	std::map<action_id, std::size_t> ids;
	std::set<std::string>            names;
	for (std::size_t index = 0; index < descriptors.size(); ++index) {
		const auto &descriptor = descriptors[index];
		if (!descriptor.id || descriptor.name.empty() ||
		    !core::valid_utf8_text(descriptor.name, 128) || !valid(descriptor.kind) ||
		    !valid(descriptor.behavior) ||
		    (descriptor.kind == action_kind::button &&
		     descriptor.behavior != action_behavior::state) ||
		    !ids.emplace(descriptor.id, index).second || !names.insert(descriptor.name).second)
			return ice::unexpected(diagnostic("Invalid or duplicate action declaration"));
	}
	std::map<control_id, std::pair<action_kind, bool>> virtual_types;
	auto                                               state = std::make_unique<implementation>();
	state->window                                            = window;
	state->limits                                            = limits;
	state->descriptors                                       = std::move(descriptors);
	state->buttons.resize(state->descriptors.size());
	state->action_bindings.resize(state->descriptors.size());
	state->bindings.reserve(bindings.size());
	for (auto &binding : bindings) {
		auto target = ids.find(binding.action);
		if (!valid(binding.context) || target == ids.end() ||
		    binding.control < control_kind::physical_key ||
		    binding.control > control_kind::virtual_control || !valid(binding.virtual_kind) ||
		    (binding.component != axis_component::x && binding.component != axis_component::y) ||
		    !std::isfinite(binding.deadzone) || binding.deadzone < 0 || binding.deadzone >= 1)
			return ice::unexpected(diagnostic("Invalid action binding selector"));
		for (auto scale : {binding.scale, binding.device_count_scale, binding.precise_scroll_scale})
			if (!bounded(scale[0]) || !bounded(scale[1]))
				return ice::unexpected(
				    diagnostic("Action binding scales must be finite and bounded"));
		const auto &descriptor = state->descriptors[target->second];
		const auto  shape      = input_shape(binding);
		if ((binding.control != control_kind::virtual_control && !window) ||
		    (binding.control == control_kind::physical_key &&
		     !platform::valid_physical_key(binding.key)) ||
		    (binding.control == control_kind::mouse_button &&
		     !platform::valid_mouse_button(binding.button)) ||
		    (binding.control == control_kind::virtual_control && !binding.virtual_control) ||
		    (descriptor.kind == action_kind::button && shape != action_kind::button) ||
		    (shape == action_kind::button && descriptor.behavior == action_behavior::delta) ||
		    ((binding.control == control_kind::pointer_delta ||
		      binding.control == control_kind::scroll) &&
		     descriptor.behavior != action_behavior::delta))
			return ice::unexpected(diagnostic("Binding shape/behavior does not match its action"));
		if (binding.control == control_kind::virtual_control) {
			auto type =
			    std::pair{binding.virtual_kind, descriptor.behavior == action_behavior::delta};
			auto [found, inserted] = virtual_types.emplace(binding.virtual_control, type);
			if (!inserted && found->second != type)
				return ice::unexpected(
				    diagnostic("Virtual control has conflicting shape/level semantics"));
		}
		implementation::binding_state prepared{std::move(binding), target->second, {}};
		if (prepared.config.control != control_kind::virtual_control) {
			if (state->contributions == limits.contributions)
				return ice::unexpected(diagnostic("Desktop bindings exhaust contribution storage"));
			prepared.sources.emplace(desktop_source, implementation::contribution{});
			++state->contributions;
		}
		state->action_bindings[target->second].push_back(state->bindings.size());
		state->bindings.push_back(std::move(prepared));
	}
	return action_reducer{std::move(state)};
}

const std::vector<action_descriptor> &action_reducer::discover() const {
	static const std::vector<action_descriptor> empty;
	return state_ ? state_->descriptors : empty;
}
ice::expected<void, action_diagnostic> action_reducer::set_context(input_context context) {
	if (!state_ || !valid(context))
		return ice::unexpected(diagnostic("Invalid/unavailable action context"));
	if (state_->context != context) {
		if (state_->generation == std::numeric_limits<std::uint64_t>::max())
			return ice::unexpected(diagnostic("Action context generation exhausted"));
		state_->reset(0, true);
		state_->context = context;
	}
	return {};
}
input_context action_reducer::context() const {
	return state_ ? state_->context : input_context::terminal;
}
void action_reducer::set_focused(bool focused) {
	if (state_ && state_->focused != focused) {
		state_->focused = focused;
		if (!focused)
			state_->reset(0, true);
		else if (state_->generation != std::numeric_limits<std::uint64_t>::max())
			++state_->generation;
	}
}
bool action_reducer::focused() const { return state_ && state_->focused; }
ice::expected<source_id, action_diagnostic> action_reducer::add_source() {
	if (!state_ || state_->sources.size() == state_->limits.sources)
		return ice::unexpected(diagnostic("Virtual source storage unavailable/exhausted"));
	auto id = allocate_source();
	if (!id)
		return ice::unexpected(diagnostic("Virtual source identity exhausted"));
	state_->sources.insert(id);
	return id;
}
ice::expected<void, action_diagnostic> action_reducer::cancel_source(source_id source) {
	if (!state_ || (source != desktop_source && !state_->sources.contains(source)))
		return ice::unexpected(diagnostic("Unknown or retired input source"));
	state_->reset(source, false);
	return {};
}
ice::expected<void, action_diagnostic> action_reducer::remove_source(source_id source) {
	if (!state_ || !source || !state_->sources.contains(source))
		return ice::unexpected(diagnostic("Unknown or reserved input source"));
	state_->reset(source, false);
	for (auto &binding : state_->bindings)
		state_->contributions -= binding.sources.erase(source);
	state_->sources.erase(source);
	return {};
}
void action_reducer::cancel() {
	if (state_)
		state_->reset(0, true);
}

ice::expected<bool, action_diagnostic>
action_reducer::consume(const platform::platform_event &event) {
	if (!state_)
		return ice::unexpected(diagnostic("Action reducer is unavailable"));
	if (event.window && event.window != state_->window)
		return false;
	using enum platform::platform_event_type;
	if (event.type < window_registered || event.type > scroll)
		return ice::unexpected(diagnostic("Invalid native action event category"));
	if (event.type == input_cancelled) {
		if (event.cancellation == platform::input_cancellation::text)
			return false;
		if (event.cancellation != platform::input_cancellation::all)
			return ice::unexpected(diagnostic("Invalid native cancellation scope"));
		cancel();
		return true;
	}
	if (event.type == focus_lost || event.type == close_requested || event.type == window_retired ||
	    event.type == application_quit_requested) {
		set_focused(false);
		return true;
	}
	if (event.type == focus_gained) {
		if (!event.window)
			return false;
		set_focused(true);
		return true;
	}
	if (event.type != physical_key && event.type != pointer_button && event.type != pointer_move &&
	    event.type != scroll)
		return false;
	if (!event.window || !bounded(event.x) || !bounded(event.y) || !bounded(event.delta_x) ||
	    !bounded(event.delta_y) || !bounded(event.scroll_x) || !bounded(event.scroll_y) ||
	    (event.type == physical_key &&
	     (!platform::valid_physical_key(event.key) || (!event.pressed && event.repeat))) ||
	    (event.type == pointer_button && !platform::valid_mouse_button(event.button)) ||
	    (event.delta_units != platform::pointer_delta_unit::logical &&
	     event.delta_units != platform::pointer_delta_unit::device_counts))
		return ice::unexpected(diagnostic("Invalid native physical action payload"));
	bool matched = false;
	for (auto &binding : state_->bindings) {
		const auto &config = binding.config;
		const bool  match =
		    (event.type == physical_key && config.control == control_kind::physical_key &&
		     config.key == event.key) ||
		    (event.type == pointer_button && config.control == control_kind::mouse_button &&
		     config.button == event.button) ||
		    (event.type == pointer_move && config.control == control_kind::pointer_delta) ||
		    (event.type == scroll && config.control == control_kind::scroll);
		if (!match)
			continue;
		matched                 = true;
		double                x = event.pressed ? 1 : 0, y = 0;
		std::array<double, 2> units{1, 1};
		if (event.type == pointer_move) {
			x = event.delta_x;
			y = event.delta_y;
			if (event.delta_units == platform::pointer_delta_unit::device_counts)
				units = config.device_count_scale;
		} else if (event.type == scroll) {
			x = event.scroll_x;
			y = event.scroll_y;
			if (event.precise_scroll)
				units = config.precise_scroll_scale;
		}
		state_->update(binding, binding.sources.at(desktop_source), x, y, true,
		               (event.type == physical_key || event.type == pointer_button) &&
		                   event.pressed && !event.repeat,
		               units);
	}
	auto token =
	    event.type == physical_key
	        ? control_token(control_kind::physical_key, static_cast<std::uint32_t>(event.key))
	    : event.type == pointer_button
	        ? control_token(control_kind::mouse_button, static_cast<std::uint32_t>(event.button))
	    : event.type == pointer_move ? control_token(control_kind::pointer_delta)
	                                 : control_token(control_kind::scroll);
	state_->refresh_buttons(std::pair{desktop_source, token});
	return matched;
}

ice::expected<void, action_diagnostic> action_reducer::submit(source_id             source,
                                                              const virtual_sample &sample) {
	if (!state_ || !source || !state_->sources.contains(source))
		return ice::unexpected(diagnostic("Unknown, reserved or retired virtual source"));
	if (!sample.control || !valid(sample.kind) || !bounded(sample.x) || !bounded(sample.y) ||
	    (sample.kind != action_kind::axis2 && sample.y != 0) ||
	    (sample.kind == action_kind::button && sample.x != 0 && sample.x != 1))
		return ice::unexpected(diagnostic("Invalid virtual control sample"));
	std::size_t needed  = 0;
	bool        matched = false, was_active = false;
	for (const auto &binding : state_->bindings)
		if (binding.config.control == control_kind::virtual_control &&
		    binding.config.virtual_control == sample.control) {
			matched = true;
			if (sample.kind != binding.config.virtual_kind ||
			    (state_->descriptors[binding.target].behavior != action_behavior::delta &&
			     (std::abs(sample.x) > 1 || std::abs(sample.y) > 1)))
				return ice::unexpected(diagnostic("Virtual control shape/range mismatch"));
			const auto found = binding.sources.find(source);
			needed += found == binding.sources.end();
			was_active |= found != binding.sources.end() && found->second.active;
		}
	if (!matched)
		return ice::unexpected(diagnostic("Virtual control is not declared by this profile"));
	if (needed > state_->limits.contributions - state_->contributions)
		return ice::unexpected(diagnostic("Virtual control contribution storage exhausted"));
	// Allocate every missing slot before changing any active value. Ordinary sample
	// validation/capacity failures leave all prior contributions and edges intact.
	for (auto &binding : state_->bindings)
		if (binding.config.control == control_kind::virtual_control &&
		    binding.config.virtual_control == sample.control) {
			auto [entry, inserted] = binding.sources.try_emplace(source);
			(void)entry;
			state_->contributions += inserted;
		}
	for (auto &binding : state_->bindings)
		if (binding.config.control == control_kind::virtual_control &&
		    binding.config.virtual_control == sample.control)
			state_->update(binding, binding.sources.at(source), sample.x, sample.y, sample.active,
			               false);
	const auto origin =
	    std::pair{source, control_token(control_kind::virtual_control, sample.control)};
	if (!sample.active)
		for (auto &button : state_->buttons)
			button.presses.erase(origin);
	state_->refresh_buttons(sample.active ? std::optional{origin} : std::nullopt);
	if (!sample.active && was_active)
		state_->next_generation();
	return {};
}

ice::expected<action_frame, action_diagnostic> action_reducer::consume_tick(std::uint64_t tick,
                                                                            double        seconds) {
	if (!state_ || !std::isfinite(seconds) || seconds < 0 || seconds > 1 ||
	    (state_->tick_valid && tick <= state_->previous_tick))
		return ice::unexpected(diagnostic("Invalid or stale action tick/duration"));
	action_frame frame{tick, state_->context, state_->generation, state_->cancelled, {}};
	frame.values.reserve(state_->descriptors.size());
	for (std::size_t index = 0; index < state_->descriptors.size(); ++index) {
		const auto  &descriptor = state_->descriptors[index];
		action_value value{descriptor.id, descriptor.kind, descriptor.behavior};
		if (descriptor.kind == action_kind::button) {
			const auto &button = state_->buttons[index];
			value.down         = button.down;
			value.pressed      = !button.presses.empty();
			value.released     = button.released;
			value.x            = button.down ? 1 : 0;
		} else if (state_->focused) {
			for (auto binding_index : state_->action_bindings[index]) {
				const auto &binding = state_->bindings[binding_index];
				if (binding.config.context == state_->context)
					for (const auto &[source, contribution] : binding.sources) {
						(void)source;
						if (contribution.armed) {
							value.x += contribution.x;
							value.y += contribution.y;
						}
					}
			}
			if (descriptor.normalize) {
				double magnitude = descriptor.kind == action_kind::axis2
				                       ? std::hypot(value.x, value.y)
				                       : std::abs(value.x);
				if (magnitude > 1) {
					value.x /= magnitude;
					value.y /= magnitude;
				}
			}
			if (descriptor.behavior == action_behavior::rate) {
				value.x *= seconds;
				value.y *= seconds;
			}
			value.x = std::clamp(value.x, -maximum_component, maximum_component);
			value.y = std::clamp(value.y, -maximum_component, maximum_component);
		}
		frame.values.push_back(value);
	}
	// Publication is complete before consuming latches; allocation failure above
	// cannot lose a pending delta, button edge or cancellation notice.
	for (auto &button : state_->buttons) {
		button.presses.clear();
		button.released = false;
	}
	for (auto &binding : state_->bindings)
		if (state_->descriptors[binding.target].behavior == action_behavior::delta)
			for (auto &[source, value] : binding.sources) {
				(void)source;
				value.x = value.y = 0;
			}
	state_->cancelled     = false;
	state_->tick_valid    = true;
	state_->previous_tick = tick;
	return frame;
}
} // namespace ice::input
