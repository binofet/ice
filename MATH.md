# ICE math

GLM 1.0.3 supplies the default vector, matrix, and quaternion implementation.
The headers are vendored with their upstream license and archive checksum in
`engine/3rdparty/glm`. Building ICE does not fetch a dependency from the network.

Link the CMake target `ice::math` and include `ice/math/types.h` . The target is available to desktop, mobile,
and headless consumers through `cmake/ICEMath.cmake`.

## Types and operations

`ice::math::vec2`, `vec3`, `vec4`, `ivec2`, `mat3`, `mat4`, and `quat` are direct
GLM aliases. Use GLM's free functions for normalization, dot/cross products,
rotations, inversion, interpolation, and matrix construction. The retired ICE
classes, implicit pointer conversions, static axis members, and mutating math
methods are removed. `dvec3` and `dmat4` are explicit double-precision alternatives;
GPU upload structures remain single precision.

```cpp
using ice::math::mat4;
using ice::math::vec3;
auto world = glm::translate(mat4(1), vec3(4, 5, 6)) *
             glm::mat4_cast(rotation) *
             glm::scale(mat4(1), vec3(2, 3, 4));
auto position = ice::math::transform_point(world, local_position);
auto direction = ice::math::transform_direction(world, local_direction);
```

Matrices use column vectors and column-major storage. Composition is
`projection * view * world * position`. GLM indexing is `matrix[column][row]`;
`glm::value_ptr(matrix)` exposes the contiguous upload data. `elements(matrix)`
returns a borrowed span of sixteen scalars, and `element(matrix, index)` accesses
a zero-based column-major scalar when a flat data algorithm needs it. Neither
helper extends the matrix lifetime. `transform_point` uses homogeneous w=1 and
`transform_direction` uses w=0; neither performs perspective division.

## Initialization and storage

ICE propagates `GLM_FORCE_CTOR_INIT` to every consumer. Default vectors are zero;
default matrices and quaternions are identity. Use `mat4(0)` explicitly for a
weighted skinning accumulator. Do not depend on `{}` producing a zero matrix.

Vectors and matrices use packed default storage. Compile-time checks enforce a
12-byte `vec3` and 64-byte `mat4`, as required by existing vertex buffers and joint
palettes. Globally aligned default GLM types are prohibited. Upload bytes through
`glm::value_ptr`, rather than casting a C++ object or relying on a conversion
operator.

GLM's scalar quaternion constructor takes **w, x, y, z**. glTF data is stored as
**x, y, z, w**: `glm::make_quat(data)` reads that storage order, and
`glm::value_ptr(quaternion)` exposes it. Changing GLM's quaternion storage order
is prohibited. Animation sampling validates quaternion data before using GLM's
normalization and shortest-path spherical interpolation.

## Camera and clip conventions

Select handedness and clip depth explicitly; do not use global GLM handedness
or depth-range switches to choose a rendering backend.

- `scene_camera` uses a right-handed world, +Y up and `glm::lookAtRH`.
  Its default projection is `glm::perspectiveRH_ZO`; explicit `clip_depth` selects
  [0,1] or [-1,1]. The camera is CPU math and does not create a rendering device.
- The retired game renderer accepted camera-to-world poses and inverted them.
  That convention and its helper were removed from the supported API; ports should
  use explicit world-to-camera matrices.
- Metal, Vulkan and D3D12 must handle viewport orientation, winding, depth and
  surface transforms explicitly, with backend image tests.

GLM scaling now scales the full basis rather than only the old matrix diagonal.
This correctly handles a nonuniform scale composed with rotation.

## Validation

`ice_math_tests` checks direct type aliases, initialization, packed upload order,
TRS composition, point/direction semantics, inverse/transpose, XYZW quaternion
import, round trips, shortest-path interpolation, camera frames, and RH/LH NO/ZO
clip depth. Animation tests exercise posed bounds, blending, and skin palettes;
modern backend probes will need to revalidate uploaded matrices and normals visually.

GLM is the default implementation, not a claim of universal peak performance.
Measure representative simulation or animation workloads before adding a
specialized SIMD implementation or a different data layout.
