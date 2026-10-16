# Semantic actions and input contexts

`ice::input` reduces physical or explicitly supplied virtual controls into owned semantic
frames. Link `ice::input` and include `ice/input/actions.h`; the module is also part of
`ice::engine`. It needs no window, renderer, language runtime or network transport. It
does not poll an OS queue, change pointer capture, enable text input or start a worker.
All reducer operations belong on one UI/host thread.

Consume the application's existing desktop event stream once and route each event to
the reducer for its window. Touch, XR and gamepad adapters can instead submit typed
virtual samples. Native window ownership and cursor behavior are described in
[PLATFORM.md](../PLATFORM.md).

## Declarations and binding profiles

`action_reducer::create(window, descriptors, bindings, limits)` validates and owns the
configuration. Each `action_descriptor` has a unique nonzero `action_id`, a unique UTF-8
name of 1–128 bytes, an `action_kind` and an `action_behavior`. IDs are assigned by the
author and should remain stable when bindings change. Discovery borrows the immutable
descriptor vector. Moving a reducer transfers its configuration, control state and source
lifetimes; the moved-from object becomes unavailable.

Kinds are `button`, `axis1` and `axis2`. Button actions use `state` behavior and expose
`down`, `pressed` and `released`. Axes carry one or two numeric components:

| Behavior | Accepted tick output | Retained input |
| --- | --- | --- |
| `state` | Current scaled aggregate value | Level remains until changed or cancelled |
| `delta` | Accumulated displacement once, including when tick duration is zero | Cleared after successful publication |
| `rate` | Current scaled aggregate rate multiplied by tick duration | Rate remains for subsequent ticks |

For example, mouse/touch displacement belongs in a look-delta action, while a stick's
continuous turning rate belongs in a separate look-rate action. Rate output already
includes elapsed seconds; multiplying it by the duration again is an error. Movement
intent can remain a `state` axis for a motor to convert into velocity.

Bindings are plain immutable data, selected by one of three explicit contexts:
`terminal`, `editor_navigation` or `player`. The reducer starts in `terminal`, unfocused.
There is no implicit priority stack, chord language, rebinding UI or built-in gameplay
meaning for these names. An author may leave a context without bindings. The application
owns pointer/text policy and should enter navigation only after its capture request
succeeds.

`action_binding` selects a physical key position, mouse button, pointer delta, scroll
or a nonzero virtual `control_id`. Scalars can contribute to either axis component through
`scale`; a two-component input driving an axis1 uses `component`. Button outputs ignore
scale. Button inputs can drive state/rate axes, but cannot drive delta actions. Native
pointer and scroll bindings require delta behavior. A virtual control ID must have the
same input shape and level-versus-delta meaning across its bindings.

Native logical pointer displacement and raw device counts use separate
`device_count_scale` configuration. `precise_scroll_scale` separately handles precise
logical scrolling; its default of 1/120 is a configurable navigation convention, not
hardware calibration. Input coordinates are not silently treated as interchangeable
across these unit systems.

Virtual state/rate axes apply a configurable radial deadzone and remap the remaining
magnitude to the unit interval. This also caps the individual analog vector to the unit
disk. Keyboard buttons and delta samples bypass the deadzone. Contributions then sum
per action. `normalize=true` clamps an axis1 to [-1,1] or an axis2 to the unit disk before
rate integration; it never expands a small value. Set it to false for unclamped look
displacement or authored turning rates. Published components saturate at +/-1e9.

## Window routing and text

`consume(platform_event)` receives an event; it never removes one from the application
queue. It observes only the configured window and application-scope lifecycle/cancellation.
Native bindings require a nonzero window ID. Window zero supports virtual-only headless
profiles.

The return type is `expected<bool, action_diagnostic>`: check success first, then inspect
the contained bool for whether a bound control or lifecycle event was observed. This is
not an exclusive UI-consumption decision. Inactive-context native levels are still
observed for rearming, even though they contribute no active action.

Committed text, composition, interpreted edit commands and text activity never become
actions. Physical letter names describe keyboard positions, not characters; typing `W`
into an IME/prompt does not synthesize a movement key. `input_cancellation::text` is
ignored by the physical reducer, preserving held controls and their rearming state.
The application continues routing text/IME to its text editor independently.

## Source contributions and lifetime

Native events use the reserved `desktop_source == 0`, an aggregate keyboard/mouse
source for the reducer's window. `add_source()` issues a nonzero virtual source lifetime.
Use `submit(source, virtual_sample)` with a declared control ID, matching kind, numeric
level/delta and `active` state. Virtual buttons use x=0/1 and y=0; state/rate analog
components lie in [-1,1]. Axis1 uses only x. Delta components may lie within +/-1e9.

These sources are independent. Releasing a keyboard movement key cannot cancel an active
touch stick; releasing one Shift key cannot cancel the other held Shift key. Button edges
are aggregate transitions. Joining an already published held button does not retrigger it.
A shared, unconsumed press survives cancellation of its first source while another source
still contributes that press. Unrelated pending taps and deltas survive source cancellation.

`cancel_source(source)` clears that source's held/pending contributions but keeps its
registration; zero selects the aggregate desktop source. `remove_source(source)` also
retires a virtual lifetime and frees its retained contribution records. Source IDs are
process-local, never reused and rejected by another reducer or after retirement. They are
not portable device identities, session credentials or network authority tokens.

An adapter can register one source per touch controller, gamepad or XR hand/control
endpoint, with control IDs defined by its profile. It should report an inactive control
with `active=false`, or cancel/remove the source when its lifecycle ends. Repeated inactive
polling is idempotent. A reconnect should receive a new source lifetime rather than reuse
a retired ID.

## Cancellation and fresh arming

Changing context, losing focus or calling `cancel()` clears held actions, pending deltas
and affected pending presses. Selecting the same context is a no-op. Native focus loss,
close/retire and application quit also make the reducer unfocused. An all-input cancellation,
including application-scope event-queue overflow notification, clears controls without
changing the current focus flag. Returning focus restores no held action automatically.

A fresh native non-repeat key/button down is an observed new press. Autorepeat cannot
rearm a control held across a context/focus cancellation. A release followed by another
press works normally, including when a prior release happened outside the application.
Modifier masks alone are not synthesized into left/right physical keys; any special
"hold Shift before entering navigation" behavior is explicit application policy.

A new virtual state/rate control first requires an active neutral sample. After context,
focus, source or inactive-control cancellation it requires fresh neutral again before a
held level can contribute. A value inside the configured deadzone counts as neutral.
This prevents a held jump/attack button or deflected stick from activating when returning
from a menu. Virtual delta controls represent instantaneous displacement and have no held
level to rearm; deltas arriving outside the active context/focus are discarded.

`action_frame::cancelled` reports a reset since the previous accepted tick. Its values can
contain valid fresh input received after that reset; do not discard the frame blindly.
`generation` is a monotonic input-context/focus/cancellation epoch, separate from authored
scene revisions and simulation ticks.

## Tick publication and failures

Call `consume_tick(tick, seconds)` at the consumer's explicit sampling boundary. Tick zero
is allowed initially; subsequent ticks must increase strictly. Duration must be finite
and in [0,1] seconds. A player motor can sample each fixed simulation tick; a developer
viewer can use a monotonic navigation-sample counter and its own clamped frame duration.
The reducer does not schedule catch-up or infer elapsed time.

The resulting frame owns one sample per descriptor in declaration order, including zeros.
`find(id)` borrows a sample from that frame. Button edges latch until an accepted tick,
then clear once: a quick down/up between ticks can report both edges with `down=false`.
Held levels/rates survive later ticks. Additional catch-up ticks do not repeat a jump
edge or mouse delta.

Unknown controls/sources, invalid relevant payloads, conflicting declarations, exhausted
storage and stale/invalid ticks return diagnostics. Ordinary rejections preserve active
state; failed tick publication consumes no deltas, edges or cancellation notice. Building
the owned frame precedes clearing those latches. Allocation failure can throw, and this
CPU reducer does not contain arbitrary native faults or data races.

The configurable defaults allow 1024 descriptors, 16384 bindings, 1024 live virtual sources
and 65536 retained binding/source records. Construction validates finite ceilings up to
65536 actions/sources and 1048576 bindings/contributions. These are storage capacities,
not an event queue. Removing a source reclaims its records; pending input is not evicted
to admit another source. Immutable data can be used by generated schema/configuration
tools, but this module does not yet load binding assets or provide a network/replay codec.

## Verification and present scope

```sh
cmake -S . -B build/actions23 -DICE_CXX_STANDARD=23
cmake --build build/actions23 --target ice_input_action_tests
ctest --test-dir build/actions23 --output-on-failure -R '^ice_input_actions$'
# Repeat with ICE_CXX_STANDARD=20 in a separate build directory.
```

The CPU suite covers keyboard diagonals/release, independent Shift keys, text/context
isolation, fresh neutral/release arming, held/tapped/repeated buttons, logical/raw-count
pointer scales, precise scrolling, virtual source combinations, source/control cancellation,
stale/foreign source rejection, contribution-capacity reclamation, failed-tick preservation
and delta-versus-rate behavior across zero and multiple ticks. C++20/23 strict builds and
standalone ASan/UBSan runs pass locally, along with the pinned formatter/analyzer and public
API audit. The virtual touch/XR cases are synthetic reducer tests.

The native desktop adapters currently report aggregate per-window keyboard/mouse events,
without individual hardware device IDs or original native sample timestamps. Automatic
device hotplug/unplug cancellation is not implemented by this reducer. Explicit lifecycle
and source cancellation are supported; OS enumeration/device notifications remain adapter
work. Native platform regressions are separate from these CPU tests.

Touch/XR adapters have a typed virtual-sample surface but are not yet wired to it. Existing
mobile/XR probes retain their own input paths. XR head/view tracking, validity and reference
spaces remain separate from locomotion actions; a desktop look action must not rotate the
user's tracked head pose. These action tests do not establish a complete first-person
player, collision motor or mobile/XR gameplay integration. Identical ordered input and tick
sequences drive the same reduction rules; cross-platform bit-identical floating-point
simulation is not promised.
