#pragma once

#include "ice/core/expected.h"
#include "ice/platform/platform_events.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ice::input {
/// Stable, author-assigned semantic identity; zero is invalid. IDs survive binding changes.
using action_id = std::uint32_t;
/// Reducer-issued, process-local virtual input source lifetime; zero denotes aggregate desktop.
using source_id = std::uint64_t;
/// Stable, author-assigned virtual control identity, local to a binding profile; zero is invalid.
using control_id = std::uint32_t;
/// Reserved aggregate keyboard/mouse source; native events do not identify individual devices.
constexpr source_id desktop_source = 0;
/// One explicitly selected application input context; selection does not change native capture.
enum class input_context { terminal, editor_navigation, player };
/// Semantic shape. A button is a binary level with latched edges; axes use one/two components.
enum class action_kind { button, axis1, axis2 };
/// State persists, delta is consumed once, and rate is multiplied by each tick's supplied seconds.
enum class action_behavior { state, delta, rate };
/// Typed binding endpoint. Virtual endpoints are supplied explicitly by touch/XR/gamepad adapters.
enum class control_kind { physical_key, mouse_button, pointer_delta, scroll, virtual_control };
/// Component selected when a two-component input drives a one-component action.
enum class axis_component { x, y };
/// Immutable semantic declaration; neither a callback nor a native device binding.
struct action_descriptor {
	/// Nonzero stable identity, unique within this reducer.
	action_id id = 0;
	/// Unique nonempty UTF-8 name, at most 128 bytes, useful for discovery and generated schemas.
	std::string name;
	/// Public output shape.
	action_kind kind = action_kind::button;
	/// Buttons require state behavior; axis rates are integrated in consume_tick, never twice.
	action_behavior behavior = action_behavior::state;
	/// Clamp an axis1 to [-1,1] or an axis2 to the unit disk before optional rate integration.
	/// False retains scaled displacement/rate; all published components saturate at +/-1e9.
	bool normalize = true;
};
/// One immutable plain-data binding; multiple sources/bindings may contribute to an action.
struct action_binding {
	/// Context in which this binding contributes. Inactive desktop levels are still observed.
	input_context context = input_context::player;
	/// Target descriptor identity.
	action_id action = 0;
	/// Native or virtual input selector.
	control_kind control = control_kind::physical_key;
	/// Required for physical_key bindings; layout-independent, never interpreted as text.
	platform::physical_key key = platform::physical_key::unknown;
	/// Required for mouse_button bindings.
	platform::mouse_button button = platform::mouse_button::none;
	/// Required nonzero control identity for virtual_control bindings.
	control_id virtual_control = 0;
	/// Required virtual input shape; native key/button inputs have button shape.
	action_kind virtual_kind = action_kind::axis2;
	/// For scalar input ->axis2, the scalar multiplies both components; otherwise per-axis scale.
	/// Button outputs ignore scale. Button inputs cannot drive delta actions; native pointer/scroll
	/// inputs require delta behavior, and a virtual control cannot mix delta and level bindings.
	std::array<double, 2> scale{1, 1};
	/// Additional per-axis multiplier for raw device_counts pointer displacement only.
	/// Logical and hardware-count sensitivity are intentionally separate configuration.
	std::array<double, 2> device_count_scale{1, 1};
	/// Additional precise-scroll multiplier; default 120 logical units per nominal wheel step is
	/// a configurable navigation convention, not hardware calibration.
	std::array<double, 2> precise_scroll_scale{1.0 / 120, 1.0 / 120};
	/// Radial analog deadzone in [0,1); remaining magnitude is remapped to [0,1].
	/// Applies only to virtual state/rate axes, never keyboard buttons or accumulated deltas.
	double deadzone = 0;
	/// Input component for an axis2 source bound to an axis1 target.
	axis_component component = axis_component::x;
};
/// Configurable finite storage ceilings, independent of native event queue budgets.
struct action_limits {
	/// Simultaneously declared actions, in [1,65536].
	std::size_t actions = 1024;
	/// Immutable bindings, in [1,1048576].
	std::size_t bindings = 16384;
	/// Simultaneously registered virtual sources, in [1,65536].
	std::size_t sources = 1024;
	/// Retained binding/source level records, including desktop bindings, in [1,1048576].
	std::size_t contributions = 65536;
};
/// Recoverable invalid configuration/sample/tick or exhausted-storage diagnostic.
struct action_diagnostic {
	/// Owned explanation; ordinary rejected input does not mutate existing state.
	std::string message;
};
/// Virtual control sample from an explicitly registered source. It never synthesizes native keys.
struct virtual_sample {
	/// Stable control selector within the binding profile.
	control_id control = 0;
	/// Must match the declared virtual binding shape.
	action_kind kind = action_kind::button;
	/// Buttons require x=0/1,y=0; state/rate analog axes lie in [-1,1], deltas within +/-1e9.
	double x = 0, y = 0;
	/// False clears this control and requires a neutral level before it can become active again.
	bool active = true;
};
/// Owned semantic sample for one tick, independent of native window/device resource lifetimes.
struct action_value {
	/// Stable semantic identity.
	action_id id = 0;
	/// Declared output shape.
	action_kind kind = action_kind::button;
	/// Declared behavior; rate x/y already include the tick duration.
	action_behavior behavior = action_behavior::state;
	/// Axis components, or button level 0/1 in x. Axis1/button y is zero.
	double x = 0, y = 0;
	/// Current aggregate button level; false for axes.
	bool down = false;
	/// Aggregate rising/falling edges since the previous accepted tick, each consumed once.
	/// A quick tap may set both even when down=false. Cancellation suppresses pending presses.
	bool pressed = false, released = false;
};
/// One complete owned snapshot, in descriptor order. Held states survive subsequent ticks.
struct action_frame {
	/// Caller-assigned monotonic tick/sample identity.
	std::uint64_t tick = 0;
	/// Context that produced this frame.
	input_context context = input_context::terminal;
	/// Monotonic context/focus/cancellation epoch; not an authored-scene revision.
	std::uint64_t generation = 0;
	/// At least one all-input/source/context cancellation occurred since the last accepted tick.
	/// Values may include fresh input received after that cancellation; do not discard them
	/// blindly.
	bool cancelled = false;
	/// Complete semantic output, including inactive/zero-valued declared actions.
	std::vector<action_value> values;
	/// Borrow one sample until this frame's values are modified/destroyed; null means unknown ID.
	const action_value *find(action_id id) const;
};
/// UI/host-thread-only deterministic semantic reducer; owns no OS queue, device or worker.
/// Route an existing desktop stream once; adapters explicitly submit virtual controls here.
/// Allocation failure may throw; native failures/driver faults are outside this CPU component.
class action_reducer {
public:
	/// Validate/copy immutable descriptors/bindings. Starts in terminal context, unfocused.
	/// Window zero supports virtual-only/headless profiles; native bindings require a real ID.
	static ice::expected<action_reducer, action_diagnostic>
	create(platform::window_id window, std::vector<action_descriptor> descriptors,
	       std::vector<action_binding> bindings, const action_limits &limits = {});
	/// Retire all virtual source lifetimes and retained control state.
	~action_reducer();
	/// Transfer the reducer and registered source lifetimes; the source becomes unavailable.
	action_reducer(action_reducer &&other) noexcept;
	/// Retire this reducer, then take the source reducer and its lifetimes.
	action_reducer &operator=(action_reducer &&other) noexcept;
	/// A reducer cannot duplicate held/source state implicitly.
	action_reducer(const action_reducer &) = delete;
	/// A reducer cannot be copy-assigned.
	action_reducer &operator=(const action_reducer &) = delete;
	/// Borrow immutable declarations for the reducer lifetime.
	const std::vector<action_descriptor> &discover() const;
	/// Cancel old context state and select a profile. Same-context requests are a no-op.
	/// Held controls require release/neutral; delta inputs have no held level to rearm.
	ice::expected<void, action_diagnostic> set_context(input_context context);
	/// Return the selected profile, or terminal for a moved-from reducer.
	input_context context() const;
	/// Apply lifecycle focus. Loss cancels every source; returning does not restore held actions.
	void set_focused(bool focused);
	/// Return whether this reducer can currently produce active actions.
	bool focused() const;
	/// Allocate a virtual source lifetime. IDs are process-local, nonzero and never reused.
	ice::expected<source_id, action_diagnostic> add_source();
	/// Cancel this source's levels/deltas while retaining its registration; zero means desktop.
	/// Other source contributions remain intact. Virtual controls require fresh neutral before use.
	ice::expected<void, action_diagnostic> cancel_source(source_id source);
	/// Cancel and retire a virtual source; later submissions using its old ID are rejected.
	ice::expected<void, action_diagnostic> remove_source(source_id source);
	/// Discard all held/pending input without changing context/focus; marks the next frame
	/// cancelled.
	void cancel();
	/// Reduce one matching-window native event, or application-scope lifecycle cancellation.
	/// True means a bound control/lifecycle event was observed; text/IME is never an action.
	/// Invalid events fail unchanged. Text-only cancellation preserves physical/rearming state.
	ice::expected<bool, action_diagnostic> consume(const platform::platform_event &event);
	/// Apply a typed virtual level/delta. A new state/rate control first requires a neutral sample;
	/// inactive controls likewise rearm only after active neutral. Delta controls are
	/// instantaneous.
	ice::expected<void, action_diagnostic> submit(source_id source, const virtual_sample &sample);
	/// Publish a strictly increasing caller tick, with finite seconds in [0,1]. Tick zero is
	/// allowed initially. State persists, delta/edges clear only after success, and rate is
	/// integrated once per supplied tick duration. Failed/stale calls consume nothing, including
	/// cancellation.
	ice::expected<action_frame, action_diagnostic> consume_tick(std::uint64_t tick, double seconds);

private:
	struct implementation;
	explicit action_reducer(std::unique_ptr<implementation> state);
	std::unique_ptr<implementation> state_;
};
} // namespace ice::input
