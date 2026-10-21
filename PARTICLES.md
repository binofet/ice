# Native particles

`ice::particles` provides bounded native effect controls and the first CPU simulation.
No particle is a scene object, controller, collision shape or physics body. The module
uses only the existing math/expected dependencies and owns no device, window or service.
Link `ice::particles`, or use the default `ICE_BUILD_PARTICLES=ON` aggregate module.

`ice/particles/control.h` is the backend-neutral gameplay boundary. Its descriptors,
exact generational handles, budgets, owner lifetimes and create/configure/start/stop/
burst/attach/inspect operations do not require a CPU particle vector, render capture,
GPU readback or particular simulation implementation. Inspection's live count is
optional: the CPU implementation reports an exact value; a future GPU implementation
can omit it. No GPU implementation or compute capability is claimed by these controls.

`ice/particles/system.h` is the explicit CPU implementation. Its `advance()` and
owned `capture()` are implementation-specific conveniences, outside `emitter_control`.
Hosts own command registration and presentation. Engine delivery alone does not
establish an application integration, visual acceptance or a consumer dependency update.

## Ownership and work limits

One host thread owns a system. Creation preallocates simulation storage: defaults
64 emitters / 4096 particles, hard limits 256 / 65536. A mobile host can choose
512 particles or any smaller positive budget; each recipe also has a finite capacity.
Emission, stepping, configuration and cleanup allocate no new simulation storage.
Creation and frame-copy allocation failures return diagnostics. Expected errors do
not throw into a controller, pause a world, silently grow a pool or invoke callbacks.

Every emitter belongs to a nonzero session and optionally an exact object generation.
The host must supply genuine lifetime identities and call `destroy_owner()` after
successful object retirement/replacement, and `destroy_session()` on session teardown.
Do not identify objects only by a reused numeric slot, put effects into authored undo
checkpoints, or mutate live effect state from a semantic scene probe. Renderer/device
replacement does not itself create a new authored session or replay a burst.

An attachment binds another same-session object generation and an affine world
transform. The host resolver validates object lifetime and supplies updated transforms;
the core executes no scene callback. An emitted particle stays in world space when
the parent moves. `detach()` freezes the last emission transform. Removing either
an emitter's creator or its attached object retires the effect. Reused emitter slots
receive new generations, and cross-system/stale handles reject.

`stop(drain)` stops births immediately and lets existing particles expire; this is
the boost-release behavior. `stop(clear)` removes existing particles too. `configure()`
atomically validates the replacement, clears old contents and stops emission.
An explicit `start()` or `burst()` resumes work. Failed configuration/burst is inert.
Burst accepts the exact requested count or rejects without consuming RNG state.
Continuous emission drops births when either budget is full and reports that count;
it retains no deferred backlog. A zero-duration advance changes nothing. A positive
advance accepts at most .25 seconds; the host owns fixed/frame-step policy.

CPU recipes bound rates, lifetimes, transforms, color/size and motion. Origin/spread
and initial velocity are in the emitter frame and transformed once at birth. Size
and acceleration are in world units. Particles outside the finite world envelope
retire. Identical seeds, controls and step sequences reproduce the same particle data;
different time partitions or different floating-point implementations are not promised
bit-identical results.

## First effects and rendering seam

Built-in recipes cover sparse expanding/fading smoke, short-lived additive boost
fire, and one-shot colored confetti with gravity and XYZ tumble. A host selects
model-relative sizes/offsets; defaults are generic world units, not measured art or
performance acceptance. A finite 48–96-chip confetti burst fits the default recipe.

`capture()` copies a bounded frame into owned vectors which remain valid after
another simulation call, reconfiguration or system destruction. Positions, size,
linear straight-alpha RGBA, XYZ rotation, normalized age and appearance are copied.
At most two contiguous blend buckets permit batching across emitters. The renderer
owns soft silhouettes/chip geometry, camera alignment, alpha/depth ordering, uploads,
submitted resource retention and measured submission/CPU/GPU costs. CPU acceptance
does not claim visible smoke/fire quality or native rendering on any platform.

Focused cached C++23 checks (no window/device/dependency installation):

```sh
cmake -S tests/particles -B build/particles -DICE_CXX_STANDARD=23
cmake --build build/particles --parallel 1
ctest --test-dir build/particles --output-on-failure
```

The executable covers validation/capacity; life/size/color/gravity/spin; attachment
and exact generations; continuous drops/drain/clear; seeded confetti and atomic
rejection; replacement/session/move cleanup; retained owned blend snapshots; actual
allocation failure and a 512-particle workload under an allocation-denial guard.
It is one observed CTest executable with eight scenario groups, not eight independently
measured unit-test invocations. Ordinary root CTest registers the same proof.

## Future compute implementations

A compute-driven fluid simulation is a separate rendering capability from this CPU
particle implementation. Preserve the shared gameplay
controls, owner generations and lifecycle; backend implementations own GPU simulation
and presentation state. Do not require a CPU particle snapshot or synchronous GPU
readback for each tick. An explicit unsupported/CPU mode must remain distinguishable
from actual GPU acceptance.

The reviewed current `render/device.h` explicitly describes its API as incomplete
for compute. The maintained Metal/GL device headers expose no reusable compute
dispatch, storage-buffer/image or 3D-field barrier contract. Existing target-readback
and queue-drain fences are not a compute simulation API. Backend/API names alone
do not prove support on desktop, Quest or iPad.

Prerequisites for a compute implementation:

- Query actual backend compute, storage-buffer, 3D sampled/storage image/format,
  workgroup and resource-limit capabilities. Report unsupported paths explicitly.
- Own finite velocity, density/temperature and required pressure/divergence fields,
  with dimension/precision/ping-pong/byte and emitter bounds declared before allocation.
  No unbounded field allocation from a script or particle count.
- Submit bounded advection, forces/buoyancy and pressure/projection work at a defined
  simulation step, followed by actual field-driven smoke/fire presentation. Specify
  compute-to-compute and compute-to-render visibility and cross-frame field hazards.
- Retain resources through submitted work; owner/session/device retirement stops
  future submission and safely releases completed generations without an idle-world
  assumption. Allocation/device failures disable the effect, preserving control/world.
- Validate real field evolution, depth/blending and bounded CPU/GPU/resource cost on
  the actual supported hardware. Optional asynchronous diagnostics do not become
  per-tick readback. Numerical/visual acceptance and capability fallback require
  separate evidence; a noise shader or dispatch receipt alone is insufficient.

This records the interface boundary and prerequisites. It commissions no fluid solver,
large RHI refactor, device activity or compute parity claim in the current core change.
