# Explicit action-channel routing

This standalone target builds the existing portable platform/reducer modules and the
new router. It does not initialize a window, device, graphics backend, physics world,
provider or network service. Configure with `ICE_CXX_STANDARD=20` or `23`.

The host supplies the reducer's actual complete declarations and one route per semantic
action. All channels start unowned. The profile reports actual action/channel counts,
matching the upstream ceiling of 65536 actions; it adds no lower default limit. The
router retains one source/cache and at most one pending handoff transaction. Historical
shared snapshots retained by callers are caller-owned storage.

An ownership change is explicit `{channel, expected_lease, desired_owner}`. Owners are
host identities plus generations; they are data, not authentication or external lifetime
validation. A valid staging call atomically replaces the whole pending batch. The next
accepted strictly newer frame applies it: cancel the old channel owner, discard that
channel's boundary-frame input, and send neutral values to its replacement. Previously
delivered held buttons receive a cancellation release; queued presses/deltas do not
cross the boundary. State/rate values rearm only after neutral. Unchanged channels keep
their source values and receive no handoff cancellation or context change.

One router has one explicit simulation or presentation clock. `seconds` is the same
duration used by the reducer: rate values are already integrated and must not be
multiplied again. A zero-duration rate sample cannot establish neutral. This component
does not resample across clocks or arbitrate device/UI policies.

All consumers read one immutable routed packet. Exact repeated publication returns
the same pointer with `advanced=false`; it does not apply pending handoffs or create a
new edge/cancellation dispatch. The host executes each producer once per clock/tick
and observes `advanced`, rather than dispatching once per eye or reader. Reading old
snapshots never authorizes acting under an old current lease.

The nine named CPU cases use real reducer frames and cover profile size, held/rearming,
delta/rate handling, cancellation with fresh input, generations/revocation, consumers
and cache identity, invalid handoff/frame atomicity, clocks, and finite allocation
failure prefixes. No whole-path allocation-free or performance claim is made.
