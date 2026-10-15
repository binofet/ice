# ICE playground direction

ICE is a place to write games and simulations, inspect rendering, try language runtimes, and learn the underlying systems. Airhockey remains the first working sample. Small experiments should produce runnable programs rather than require every subsystem to be finished.

## Application events do not need a root window

A process owns the event service. Windows are registered participants, each with a stable ID; none needs to be the parent of the others. Window ID zero means application scope, not a hidden native window.

Targeted events include keyboard input, pointer input, focus, resize and close requests. Application events include quit, device connection, gamepad input and application commands. An action such as Pause can consume either category according to an explicit policy.

OS-wide input is a third, separate feature. Registering a system hotkey or monitoring input while another application is focused is an opt-in platform operation, subject to the OS's permissions and restrictions. Some APIs use a message-only window as their delivery endpoint; that endpoint need not be a visible root or the owner of all other windows.

Routing should be platform event -> application queue -> window/device input state -> action map -> game/tool. Tools may observe events, but capture and consumption should be explicit. A global observer must not mean that an unfocused preview can steer the game. Focus loss clears held state and releases pointer capture. A destruction event carries an ID rather than a potentially dangling C++ pointer.

The first implementation adds monotonically allocated window IDs and a bounded application event stream for registration, retirement, focus, close and quit. Existing native input and the singleton remain intact. Registration means a logical window object was allocated, not that native window creation succeeded. Close state remains authoritative even if the diagnostic queue overflows; overflow is counted. The next step is targeted key/pointer events, window-local state and an action-routing test with two interactive previews. Text input/IME must be a distinct path from physical keys.

## A small fast core with visible behavior

Keep the runtime core in C/C++ with explicit ownership, handles, allocation budgets and fixed-step simulation. Use a stable C ABI at runtime boundaries. The ABI exposes versioned functions, handles, plain data and batch operations; it does not expose STL containers or raw object graphs.

Make debuggability a feature: resource names and lifetimes, event traces, allocations, script errors, GPU validation and frame captures. Assertions explain violated contracts. Profiling establishes whether a batch or an allocation is actually expensive before adding complexity. Deterministic simulation is an opt-in profile with recorded input, controlled randomness and documented numerical limits; a fixed timestep alone does not guarantee it.

Optional modules should be removable. A shader experiment should not require Python, C#, a network stack or an editor. A headless simulation should not create a window or GPU device.

## Metagen: the definition DLL is the schema

C# declarations and attributes compile into a definition DLL, which is the authoritative schema artifact. Tools load that assembly and reflect over it to discover object definitions and their metadata. This matches the original Metagen idea; no parallel hand-maintained schema is needed.

A shared Metagen reader interprets the reflected metadata consistently and exposes consumption interfaces. Emit a neutral manifest only when a real consumer benefits: native builds without .NET, language tools that cannot load managed assemblies, deployment compatibility checks, or small retro targets. Such a manifest is a derived view of a specific DLL, identified by a fingerprint and generator version, not a second source of truth. Compiler integration through [Roslyn](https://learn.microsoft.com/en-us/dotnet/api/microsoft.codeanalysis.iincrementalgenerator) is optional; an ordinary reflection-based generator is a valid first prototype. Use attribute metadata inspection where possible, so reading declarations does not execute gameplay or attribute constructors. Native and managed memory layout still remain separate from serialized and network formats.

Metagen itself is a reusable consumption SDK, not an engine-specific file generator. A reflected model offers source identity, types, members, relationships, attribute values and diagnostics through queryable interfaces. Consumers need not repeat reflection logic or know how another consumer works.

A possible API shape is `IMetaSource` -> `IMetaType` -> `IMetaMember`, with metadata values and relationships available at each level. Consumer libraries can query this model or use a visitor; they do not need to implement a single mandatory emitter interface. One can draw an infographic, another emit plain text or INI, another generate language bindings, another derive shader layouts. Source loading, metadata interpretation, output rendering and target-specific policy remain separate responsibilities. The model need not erase unknown attributes or force every definition to inherit an ICE object base class.

Shader consumers need explicit layout rules for their target, rather than assuming CLR layout matches shader-buffer layout. A binding consumer likewise owns the target ABI/marshalling rules. Metagen can support reusable policies and diagnostics without choosing one universal representation for every use.

The schema needs stable explicit type/field IDs, versioning and migration policies; ownership and reference semantics; field units, ranges and editor hints; serialization inclusion; and separate replication policy. Renaming a field must not accidentally change its wire identity. Native memory layout, persistent storage format and network representation are separate contracts.

Generate C/C++ declarations and ABI adapters, serializers, migration scaffolding, editor descriptions, binding manifests and language wrappers. Treat automatic data binding as generated observability and explicit updates, not invisible mutation on every field access. Treat replication as explicit authority, change tracking, cadence, quantization and reliability rules rather than attaching a generic Networked attribute to everything.

Prototype one small schema, such as PaddleSettings, and prove a native round trip, a version migration and one language binding before building a large attribute library. Cache compiler outputs so shipping an ordinary native build can consume generated artifacts without starting .NET.

## Four languages, different jobs

- Lua: compact embedded gameplay and configuration; improve the existing integration first.
- Python: experiments, numerical tools, asset pipelines and automation. Use batch interfaces instead of crossing into Python per particle or per vertex.
- C#: authored object definitions and optionally managed gameplay/tools. Define whether a module uses hosted .NET or Native AOT. Native AOT can export explicit C entry points; it does not mean arbitrary managed plugins can be hot-reloaded. [Native AOT interop](https://learn.microsoft.com/en-us/dotnet/core/deploying/native-aot/interop).
- JavaScript: interactive tools and experiments. Choose and isolate a runtime when the first sample needs one; require jobs, error handling and lifetime ownership to be explicit.

All adapters share the DLL-defined schema semantics and ABI rules, not a universal garbage collector. Each runtime owns its managed objects; native handles mediate references. Unload/reload needs generations and explicit teardown so stale callbacks cannot invoke dead runtimes. None of these runtimes is implemented by the new event foundation.

## Rendering across eras

Retain the current OpenGL sample as the reference during migration. Add a new rendering interface alongside the legacy device instead of stretching fixed-function calls into modern resource/barrier semantics.

The common interface describes buffers/textures, pipelines, bindings, draw/dispatch work, render passes, synchronization and presentation surfaces. It exposes capabilities and native extensions. Resource state tracking and lifetime validation belong in the debuggable layer. Avoid promising that every backend supports the same shader features, binding model or command submission behavior: [D3D12 binding differs substantially from older Direct3D](https://learn.microsoft.com/en-us/windows/win32/direct3d12/important-changes-from-directx-11-to-directx-12), and [Metal has its own argument-buffer model](https://developer.apple.com/documentation/metal/improving-cpu-performance-by-using-argument-buffers).

Implement Metal first on this Mac, Vulkan next on spacegoo, and D3D12 with Windows compile checks followed by actual hardware validation when available. Each backend must clear a surface, draw a shader triangle, upload a textured mesh, handle resize and independent previews, then run the same scene. These are planned backends, not completed implementations.

Keep backend-specific shaders available for experiments. Shared material shaders can acquire an offline compilation path after two real backends demonstrate the requirements. For Apple targets, native Metal stays the first choice for this learning project; Vulkan portability layers are a separate experiment. [Khronos documents the additional layer needed for Vulkan on Apple platforms](https://github.khronos.org/Vulkan-Site/guide/latest/windowing_audio_input.html).

Define profiles rather than a universal lowest common denominator: legacy/fixed-function, portable programmable rendering, and modern compute/advanced rendering. A scene declares requirements. The asset pipeline can produce simpler variants; unsupported features report useful errors rather than silently changing the image.

## Retro targets as experiments

An original Xbox target would test allocation discipline, asset conversion and small runtime profiles. The open homebrew route is [nxdk](https://github.com/XboxDev/nxdk), including pbkit for the GPU; it is not interchangeable with the desktop Direct3D SDK. Keep modern GPU features and heavyweight language runtimes optional. Start with a tiny rendered scene or Airhockey-derived demo, then measure its actual memory and timing budgets.

Longevity comes from documented data formats, reproducible offline builds, backend isolation, explicit capabilities, readable code and retained reference samples. It cannot guarantee that future toolchains or hardware will behave identically.

## Implementation order

1. Preserve a local native-platform/Airhockey checkpoint. Publish only when explicitly requested.
2. Window IDs and application event queue; then per-window input, text input and action routing.
3. Separate presentation surfaces from windows and prove two independent previews.
4. Native Metal triangle and textured scene; establish the modern render interface from real code.
5. Match that scene on Vulkan, then D3D12, with backend capture and validation.
6. Metagen definition-DLL/reflection prototype and generated C ABI; Lua first consumer, then Python, C# and JavaScript examples.
7. Headless simulation/replay sample and an optional original-Xbox profile experiment.

The order may change when a particular experiment is more interesting. Every step should leave a runnable playground.

## Native mobile host

The first callback-driven host and iOS/Android platform probes are implemented. See
[MOBILE.md](MOBILE.md) for ownership, scheduling, input conventions, builds and the
remaining renderer/companion work. SkeeBall physics tuning is deferred; OrbitalSalvage
is the first prototype to deepen for simulation and replay.

## Companion input lab

The native mobile hosts now share calibrated shot gestures, touch fallback, bounded
recordings and deterministic replay. A desktop replay tool consumes the same exported
takes. See [COMPANION.md](COMPANION.md). Local pairing and SkeeBall delivery are implemented; SkeeBall
physics tuning remains deferred.


### Party-game direction

Keep an authoritative game host, controller inputs and presentation surfaces
separable. A future mobile host could run a whole game and present on an external
screen, while additional players join with browser controllers via a QR/link.
Start with touch and permission-based motion input over a WebSocket/WebRTC adapter;
the current native UDP socket is not directly accessible to a browser. Host/join
lifecycle, multiple players, reconnects and readable onboarding come first.

A native ARKit/ARCore controller can then supply tracked pose and tracking quality.
For SkeeBall, calibrate a launch plane and direction relative to the room, estimate
release velocity in that shared space, and gate throws on tracking quality. Treat
this as a measured experiment: visual-inertial tracking does not guarantee perfect
ball trajectories, especially under fast movement or poor lighting. Keep intent
mapping and recorded traces so physical accuracy and game feel can be compared.

Full-screen presentation, a better game camera and readable player feedback should
precede publication. Phone hosting requires completing the mobile game renderer;
current mobile apps are controllers. Prototype external display/mirroring first,
measure display latency, and evaluate dedicated Cast/TV receivers as a separate
presentation backend. This is a roadmap, not a claim these features already exist.
