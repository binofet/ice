# Rigid motion foundation (CPU only)

`ice::motion` contains rigid-pose value math, independent of cameras, scene objects,
Lua, input, rendering, and physics. Include `cmake/ICEMotion.cmake` and link
`ice::motion`; consumers include `ice/motion/pose.h`. The existing engine aggregate
and render-camera API are intentionally outside this lane's integration scope.

`validate_pose` accepts finite float positions and finite nonzero quaternions. It
normalizes in double precision, stores a float quaternion, chooses a deterministic
sign (first nonzero component in w/x/y/z order positive), and removes signed zeros.
Float unit length and geometric round trips have normal floating-point rounding;
bit-identical canonicalization after arbitrary repeated math is not promised.
Geometric operations use double intermediates and reject outputs outside finite
float position storage. Quaternion magnitude can span the entire nonzero finite
float range. There is no scene/physics bounds policy here.

`pose_sample` is staged data, not an automatically published transform or a
producer/ECS base class. Hosts allocate nonzero coordinate-space and clock IDs
that include lifetime, and assign exact frame identities; frame zero is valid.
Hosts must not reuse these identities across distinct lifetimes. No timestamp,
clock resampling, frame increments, or frame-rate policy is inferred here.
`blend_samples` requires identical space, clock, and frame identities, including
at weights zero and one. Both poses are validated at endpoints. Translation is
linear and orientation follows the shortest quaternion arc. Weight must be finite
in [0,1]; invalid weights are rejected rather than clamped. Exactly antipodal
quaternion representations describe the same rotation. Exactly 180-degree
distinct goals have two equal arcs; no semantic turn-direction preference is
promised.

Use blending for alternative absolute goals. `compose_pose(parent, local_offset)`
applies a local offset before its parent. `compose_sample` accepts a **timeless**
local offset and retains the parent's tag; it is not a bypass for mixing sampled
frames. Raw composition, inverse, and relative helpers have no identity tags:
the caller establishes the coordinate relationship. `relative_pose(parent, world)`
assumes both poses are expressed in the same coordinate space. These functions do
not build a coordinate graph, blend constraints, or supply lens/projection policy.

A host may adapt one completed pose to visual presentation and to a desired motor
or supported kinematic target. Collision handling and the simulation retain
authority over actual physics state. The fixture's CPU adapters demonstrate data
reuse and unchanged actual state upon target submission; they do not exercise a
physics backend, motor, collision response, or a teleport API. Spring following
and other controller algorithms remain host/Lua concerns.

Configure and run each supported language mode without the full engine:

```sh
cmake -S tests/motion -B build/motion-20 -DICE_CXX_STANDARD=20
cmake --build build/motion-20
ctest --test-dir build/motion-20 --output-on-failure
cmake -S tests/motion -B build/motion-23 -DICE_CXX_STANDARD=23
cmake --build build/motion-23
ctest --test-dir build/motion-23 --output-on-failure
```

Seven independent cases cover canonical validation, endpoints and shortest arc,
nonidentity parent composition, inverse/relative math, invalid/nonfinite/overflow
rejection, exact tag matching, and shared visual/desired-target consumption. This
proof needs no GPU, window, app session, Jolt, provider, network, or performance run.
