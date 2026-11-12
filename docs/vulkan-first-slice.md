# Win32 Vulkan first slice

This optional backend is an initial ICE rendering path, not complete application renderer
support or Windows qualification. It uses `ice::render::device`, `shader_snapshot`,
`create_mesh`, `prepare_pipeline`, desktop `attach`, and `presentation_surface::draw_batch`.
Applications still need their texture/UI/shader/HDR/readback feature chain. Do not route
their Metal/GL shader files or normal renderer selection into this slice.

`ICE_BUILD_VULKAN` defaults OFF, so existing builds do not find or link a Vulkan SDK.
Enabling it requires Win32 and `ICE_BUILD_DESKTOP`, Vulkan SDK headers at least1.3.261
and the Vulkan loader. The device requires Vulkan1.1, a combined graphics/Win32-present
queue, Depth32Float attachments, BGRA8 UNORM presentation and these extensions:

- Instance: `VK_KHR_surface`, `VK_KHR_win32_surface`,
  `VK_KHR_get_surface_capabilities2`, `VK_EXT_surface_maintenance1`.
- Device: `VK_KHR_swapchain`, `VK_EXT_swapchain_maintenance1` and its enabled feature.
- Default `ICE_VULKAN_VALIDATION=ON`: `VK_LAYER_KHRONOS_validation` and
  `VK_EXT_debug_utils`. Missing requested validation fails device creation. An explicit
  validation-OFF build is a different configuration, not validation evidence.

The source module is `cmake/ICEVulkan.cmake`, native target `ICEVulkanRuntime` /
`ice::vulkan`. `ICERenderRuntime` dispatches additive `backend_kind::vulkan` through
the existing logical-device token and exclusive window lease. The Win32 window owns
its HWND; it must outlive the attached presentation owner. No storage foundation is changed.

## Supported shader and draw profile

The existing UTF-8, NUL-free,1MiB `shader_snapshot` contract remains unchanged. Vulkan
consumes an explicit `ice-vulkan-spirv-v1` packet, never a GLSL/Metal source inferred by
extension. Two trusted offline-compiled, validated Vulkan1.1 SPIR-V modules are encoded
as text:

```text
ice-vulkan-spirv-v1
vertex WORD_COUNT
07230203 00010300 ...
fragment WORD_COUNT
07230203 00010300 ...
```

Each module has5..32768 complete eight-digit hexadecimal words, SPIR-V1.0..1.3,
a bounded ID space, exactly one matching vertex/fragment entry and Shader capability.
The profile reader rejects malformed instruction lengths, wrong stage/entry,
extensions, descriptor/storage resources, unsupported vertex inputs and push layouts.
It is bounded reflection, **not a complete SPIR-V validator or compiler sandbox**.
Use the SDK compiler/validator on authored modules before providing them to a native driver.
`tools/pack_vulkan_shader.py` packs explicit offline outputs; runtime never invokes tools.

Supported pipelines have explicit `sampled_bindings=0`, RGBA8 UNORM target format,
procedural triangles or indexed24-byte `position_normal` vertices (float3 locations0/1),
less-than depth test/write and existing premultiplied-alpha blending. Culling is disabled.
Clip depth stays0..1 and a negative-height viewport preserves positive-Y-up coordinates.
Mesh vertex/index limits, finite input checks, device ownership,131072-draw/64MiB batch
limits and the portable16KiB uniform ceiling remain unchanged.

Shaders either use no push constants (empty per-draw uniforms) or exactly this80-byte
column-major block, reflected from both modules:

```glsl
layout(push_constant) uniform Draw { mat4 transform; vec4 color; } params;
```

Matrix offset0, stride16, color offset64 and float32 types must match. A pipeline using
the block requires exactly80 uniform bytes. Uniform values are synchronously copied
by command encoding. This narrower supported profile does not enlarge existing limits.

Textures/descriptors, UV/skin layouts, HDR/render targets, postprocess chains, borrowed
destinations/EGL, offscreen capture and asynchronous readback are unsupported and return
diagnostics. They do not publish empty successful resource owners. Surface-lost and
terminal native failures require draining/recreating the surface or device owner.
`live_pipeline::prepare` also remains unavailable because its required offscreen probe
cannot run; the existing transactional activation oracle is not bypassed.

## Submission and retirement

One frame is in flight per presentation owner. Acquisition uses timeout0; unavailable
images return `busy`. Zero extent suspends presentation, and out-of-date/suboptimal
acquisition or presentation schedules swapchain recreation after actual retirement.
All draw validation and host allocations precede acquisition. Submitted command owners
retain native pipelines and mesh buffers even if public owners are destroyed.

Acquisition, GPU submission and maintenance1 presentation have separate fences.
Only successful/suboptimal acquisition creates an acquisition-fence wait. The two
spec-guaranteed unqueued allocation failures from queue submission create no submission
fence wait. A call that exits without a typed result retains its conservative obligation.
The frame polls them without waiting before resource/semaphore reuse. Explicit handoff
and closure wait those owned fences; destruction retains ownership on an uncertain
retirement error rather than treating it as safe. Device loss is terminal. There is no
bounded driver latency or crash-containment promise, and arbitrary driver/wait failures
can prevent closure. A failed new swapchain preparation never falls back to a retired
old swapchain.

Submission-fence or `vkDeviceWaitIdle` success alone does **not** establish presentation
retirement. The maintenance1 requirement follows Khronos's
[semaphore/closure guidance](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html)
and [presentation fence contract](https://docs.vulkan.org/refpages/latest/refpages/source/VkSwapchainPresentFenceInfoKHR.html).

## Focused CPU checks

No Vulkan SDK, window or GPU is needed:

```sh
cmake -S tests/vulkan_profile -B build/vulkan-cpu23 -DICE_CXX_STANDARD=23 -DCMAKE_BUILD_TYPE=Release
cmake --build build/vulkan-cpu23
ctest --test-dir build/vulkan-cpu23 --output-on-failure
```

Repeat in a separate directory with `ICE_CXX_STANDARD=20`. The actual profile reader is
tested with live checks under NDEBUG. Synthetic instruction fixtures establish parser/
profile behavior, not complete SPIR-V validity, Windows compilation or GPU behavior.
The actual portable call-obligation adapters are also exercised with injected typed
outcomes and no-result exceptions. These verify fence-wait selection without a driver;
they are not native allocation-fault injection or GPU retirement proof.
The root CTest also enrolls `ice_vulkan_profile` as a CPU/offline/no_gpu test.

## Explicit Windows platform smoke

Only the existing platform operator runs this on a qualified Windows11 host. No SDK or
driver installation is performed by the source changes. Use the existing admitted
toolchain and paths, keeping C++23 daily/PR and separate C++20 checks:

```powershell
cmake -S . -B build/vulkan-win23 -DICE_CXX_STANDARD=23 -DICE_BUILD_VULKAN=ON -DICE_BUILD_VULKAN_SMOKE=ON -DICE_VULKAN_VALIDATION=ON -DICE_BUILD_OPENGL=OFF
cmake --build build/vulkan-win23 --config Release --target ice_vulkan_smoke ice_vulkan_profile_tests
ctest --test-dir build/vulkan-win23 -C Release -R '^ice_vulkan_profile$' --output-on-failure
# Exact executable configuration directory depends on the generator:
build/vulkan-win23/engine/Release/ice_vulkan_smoke.exe build/vulkan-win23/engine/ice-vulkan-mesh.shader
```

`ICE_BUILD_VULKAN_SMOKE` additionally requires the SDK `glslc` and Python3. Its CMake
producer builds both checked-in shaders offline and emits the named text packet.
The smoke creates one ICE Win32 window, presents a rotating indexed mesh for ten
seconds, resizes after two seconds, checks duplicate attachment/unsupported texture/
invalid resize/zero-size suspension, drops public mesh/pipeline owners and drains
before detaching. It is deliberately **not** an automatic CTest or normal-app launch.
The operator retains stdout/stderr, validation output, exact compiler/source/artifact
pins, visible mesh/resize observations and owned closure result. A real window and
visible mesh, successful resizing/reacquisition, clean error log and exit0 are required.
An early manual close is an incomplete smoke; minimize/restore is an additional explicit
operator check, not inferred from the source's zero-size API check.

Source/CPU checks, Windows SDK compile/link, real driver validation and visible runtime
acceptance are distinct. This smoke does not qualify private-store behavior or any
application-specific persistence, networking, installed assets or complete rendering.
