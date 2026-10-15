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
character controllers are future extensions.

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

Public backend references: [Box2D documentation](https://box2d.org/documentation/)
and [Jolt 5.6 documentation](https://jrouwe.github.io/JoltPhysicsDocs/5.6.0/).
