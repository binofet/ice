# Physics integrations

ICE owns the public simulation API and body lifetimes. `ice::physics2d` integrates
Box2D; `ice::physics3d` integrates Jolt. Neither public API exposes upstream types,
requires a renderer, creates a background worker pool, nor advances an automatic
clock. Create the worlds a host needs and call `step()` for each fixed tick.

## Build and source provenance

Both modules are enabled by default on desktop and mobile builds, and disabled by
default for watchOS. Set `ICE_BUILD_PHYSICS_2D=OFF` or
`ICE_BUILD_PHYSICS_3D=OFF` independently. Ordinary headless presets retain physics;
`cmake --preset headless-tiny` disables both modules and their dependency downloads.
CMake 3.22 or newer and a C17 compiler are required alongside ICE's C++20/23 compiler.

Enabled modules fetch source archives at first configure. The immutable commit and
SHA-256 check are part of `cmake/ICEPhysicsDependencies.cmake`; source code is built
for the target platform, and no upstream binaries are checked into ICE.

| Backend | Release | Pinned commit | Archive SHA-256 |
| --- | --- | --- | --- |
| [Box2D](https://github.com/erincatto/box2d/releases/tag/v3.1.1) | 3.1.1 | `8c661469c9507d3ad6fbd2fea3f1aa71669c2fe3` | `dee7bcf0f50b3dcccf67678ce14ffe3ce02583b51b8b03e447ab30a84c3436ef` |
| [Jolt](https://github.com/jrouwe/JoltPhysics/releases/tag/v5.6.0) | 5.6.0 | `e77f175595e64cb44218cc9d9d56fc365ad0e36a` | `1f32328fb763135de10a244568d6ccb2ed9b1e6593fafe6dc6db5b2719d330bd` |

Both projects use the MIT license; the fetched trees retain their license files.
The configuration disables upstream examples, tests, profiling, GPU-compute/hair
support, extra shader tools, and host-specific AVX tuning. x86 uses its SSE2 baseline;
AArch64 uses NEON. These settings support portable rigid-body integration rather
than requiring the build host's instruction set on every deployment target.

CMake's normal FetchContent source/download directories live below the build tree
(`_deps` by default). Preserve that build cache when configuring without network
access. A fully offline configure can use explicit, already-extracted sources:

```sh
cmake -S . -B build/offline \
  -DICE_BOX2D_SOURCE_DIR=/absolute/path/to/box2d-source \
  -DICE_JOLT_SOURCE_DIR=/absolute/path/to/JoltPhysics-source
```

These deliberate source overrides bypass archive hashing, so the caller owns their
provenance and local modifications. Standard FetchContent source overrides and
`FETCHCONTENT_FULLY_DISCONNECTED` also remain available; disconnected mode assumes
the requested sources are already populated. The backend versions must provide the
APIs expected by ICE. No access to a private SDK or Metagen checkout is needed.

## World and body contracts

Use meters, kilograms, seconds, and radians. Gravity, timestep, solver substeps and
body/event budgets are explicit world construction settings. A successful `step()`
advances exactly one configured tick. Rendering may run at a different rate; a host
chooses its accumulator and interpolation policy. Results are approximate and are
not promised to be bit-identical across platforms or backends.

Worlds are move-only and confined to one host thread. Each engine `body_id` combines
a process-local world identity, a slot and a non-reusing 64-bit generation. Stale
handles, handles from another world, exhausted budgets and invalid numeric inputs
return diagnostics before ordinary mutation. Moving a world preserves its body
handles; retiring the world invalidates all of them. Bodies have one centered
collision shape in this first API. Joints, compound shapes, mesh colliders and
additional character shapes remain future extensions. The Jolt module includes the
world-owned capsule motor described below.

Contact spans borrow world storage until the next step or world retirement. Explicit
body removal preserves the current span and exposes retirement transitions on the
next step where the backend reports them. Events retain the retired engine identity,
even if a native handle or engine slot has been reused. Event overflow is explicit
in `step_report::contacts_truncated`; consumers must not infer a complete contact
set from a truncated stream.

These are backend-reported contact/manifold lifecycle transitions. They are not a
universal geometric-overlap enter/exit contract: sleeping and manifold retirement
can differ between Box2D and Jolt. Queries expose closest nonsensor ray hits, while
sensors expose overlap transitions without collision response.

The integration validates finite bounded inputs. Box2D additionally checks native
body state after stepping; nonfinite output faults further stepping. Jolt reports
native constraint-budget failures and likewise faults further stepping. A failed
native step is not rolled back. Upstream allocation failures, assertions and
arbitrary native-library faults are not sandboxed or crash-contained. Engine event
and lifetime-map storage is reserved at world creation, while native simulation
storage may still grow during use.

Jolt's assertion-enabled builds, including Debug, assert inside `PhysicsSystem::Update`
when a native resource error occurs, before ICE can receive the returned error flag.
Such exhaustion can therefore terminate the process rather than return a diagnostic.
The wrapper's faulted-world recovery path applies when the native call returns; it
does not intercept that assertion or make undersized native budgets safe.

## World-owned 3D characters

`ice/physics/character3d.h` defines the portable capsule description, intent, identity
and owned snapshot. `world3d::create_character()` prepares a Jolt `CharacterVirtual`
motor and its collision proxy together. The world owns both; applications retain a
`character_id`, never a Jolt pointer. Characters collide with rigid bodies and one
another and can push dynamic bodies within their configured `max_push_force`.

The capsule is upright along +Y. `position` is the **feet origin**; `height` is the
full capsule height, including both hemispheres, and must be at least twice `radius`.
The default capsule is 1.8 meters tall with radius 0.3 meters. Camera/eye height and
camera rotation are application policy, independent of the collision capsule.

`set_character_intent()` replaces desired world-X/Z velocity in meters per second;
its Y component must be zero. This is a velocity request, not a displacement or an
automatic tick. Each `world3d::step()` refreshes character contacts, advances the
motors once using the world's configured timestep, then advances rigid-body physics.
Rendering, input sampling and intent updates do not advance simulation. Horizontal
intent persists until replaced or cancelled; gravity and collision response determine
vertical motion.

`character3d_description` controls the maximum walkable slope in radians, upward
stair height, downward support distance and jump speed. `step_height=0` disables
automatic stair climbing; `support_distance=0` disables downward floor adhesion.
Walkable support supplies its contact-point velocity, and a jump adds the configured
upward speed to that support velocity. A ceiling collision cancels upward motor
velocity. Stair/floor snapping changes position without adding artificial momentum
to the reported motor velocity.

`jump=true` latches one attempt until the next fixed tick. Repeated input updates with
`jump=false` preserve it; the tick consumes the attempt once even if the character is
airborne or jumping is disabled. Use `cancel_character_intent()` on focus/context
loss, pause or controller retirement to clear horizontal input **and** the queued
jump. Cancellation retains physical vertical velocity and remains available after a
world fault. It does not perform a tick or teleport the character.

`inspect_character()` copies feet position, motor velocity, pending jump and support
state. Support is `on_ground`, `on_steep_ground`, `not_supported` or `in_air`, with
contact point, normal, velocity and a still-live supporting rigid-body identity where
available. Creation and `teleport_character()` invalidate support until the next
tick; teleport also clears input and accepts an explicit initial velocity. Teleport
is not a collision sweep. Character query overflow is separately exposed through
`character3d_snapshot::contacts_truncated`; it is independent of the world's retained
contact-event overflow flag.

Character IDs use world identity, slot and a non-wrapping generation, independently
of body IDs. Moving a world preserves both kinds of handles; stale and foreign
handles are rejected. `erase_character()` retires the motor and proxy together. The
snapshot's `proxy` is a slightly inset kinematic capsule available to ordinary body
inspection, nonsensor ray queries and sensor contact events. Direct body erase,
pose changes and impulses reject character proxies; use the character operations.
Each proxy consumes one `max_bodies` slot and appears in `body_count()`. Characters
also consume `max_characters` slots (default 64; zero disables creation), so both
budgets must have room. `character_count()` counts only live motors.

Retired motors that participated in a tick remain alive and noncolliding until the
next successful native update refreshes cached contacts. Their engine handles become
stale immediately. This quarantine protects both other motors' cached pointers and
proxy contact identities through repeated create/erase operations between ticks;
unstepped replacements can be destroyed immediately. It shares the bounded native
body reserve described under verification rather than enlarging the public live
budgets. Faulted worlds retain quarantined objects until world destruction.

This first motor requires gravity parallel to -Y or zero; rotated or upward gravity
is rejected at character creation, though rigid-body-only worlds accept it. There is
no crouching, arbitrary capsule orientation, root motion or dedicated moving-platform
motion API. Support velocity is read from Jolt, but authored kinematic `set_pose()`
changes are teleports and do not provide a swept platform velocity contract. Moving
and rotating platform behavior has not been validated by this milestone.

## Verification scope

`ice_physics2d_tests` exercises settling, impulses, fixed ticks, sensors, closest-hit
queries, bounds, foreign/stale handles, move ownership and event overflow. Its
retirement test forces 65,535 native shape reuse cycles between ticks and checks
that a queued end event still names the original engine body. The sanitizer
configuration instruments the upstream dependency as well as the wrapper.

`ice_physics3d_tests` covers the equivalent ownership, fixed-step, impulse and closest-hit
contracts, plus continuous casting against a thin wall. A two-body world exercises 512
create/erase operations between ticks while a removed sensor contact is pending. Jolt's
eight-bit native ID sequence never substitutes for the engine's identity: stepped native
bodies remain quarantined until the next successful update retires their manifolds.
This reserves at most twice the configured body count in native slots; the public live-body
budget remains unchanged. Contact callbacks retain bounded storage and never inspect a
destroyed body.

`ice_character3d_tests` covers feet/height validation, world ownership, both capacity
budgets, explicit ticks and cancellation, one-shot jumps, floor support, wall sliding,
ceiling response, enabled/disabled stair climbing, walkable/steep slopes, dynamic-body
pushing, character separation and explicit teleport. Retirement regressions perform
600 replacements between ticks, including stationary and simulated sensor volumes,
then check that old proxy end events and new proxy begin events retain their distinct
engine identities. These are CPU simulation tests; they do not prove moving-platform
behavior or cross-platform deterministic lockstep.

Public backend references: [Box2D documentation](https://box2d.org/documentation/)
and [Jolt 5.6 documentation](https://jrouwe.github.io/JoltPhysicsDocs/5.6.0/).
