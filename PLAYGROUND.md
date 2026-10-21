# ICE playground direction

ICE is a place to write games and simulations, inspect rendering, try language runtimes, and learn the underlying systems. Airhockey and SkeeBall remain retained game examples awaiting modern renderer ports. Small experiments should produce runnable programs rather than require every subsystem to be finished.

## Application events do not need a root window

A process owns the event service. Windows are registered participants, each with a stable ID; none needs to be the parent of the others. Window ID zero means application scope, not a hidden native window.

Targeted events include keyboard input, pointer input, focus, resize and close requests. Application events include quit, device connection, gamepad input and application commands. An action such as Pause can consume either category according to an explicit policy.

OS-wide input is a third, separate feature. Registering a system hotkey or monitoring input while another application is focused is an opt-in platform operation, subject to the OS's permissions and restrictions. Some APIs use a message-only window as their delivery endpoint; that endpoint need not be a visible root or the owner of all other windows.

Routing should be platform event -> application queue -> window/device input state -> action map -> game/tool. Tools may observe events, but capture and consumption should be explicit. A global observer must not mean that an unfocused preview can steer the game. Focus loss clears held state and releases pointer capture. A destruction event carries an ID rather than a potentially dangling C++ pointer.

The first implementation adds monotonically allocated window IDs and a bounded application event stream for registration, retirement, focus, close and quit. The obsolete singleton input path is removed; desktop input still needs connection to the shared host. Registration means a logical window object was allocated, not that native window creation succeeded. Close state remains authoritative even if the diagnostic queue overflows; overflow is counted. The next step is targeted key/pointer events, window-local state and an action-routing test with two interactive previews. Text input/IME must be a distinct path from physical keys.

## A small fast core with visible behavior

Keep the runtime core in C/C++ with explicit ownership, handles, allocation budgets and fixed-step simulation. Use a stable C ABI at runtime boundaries. The ABI exposes versioned functions, handles, plain data and batch operations; it does not expose STL containers or raw object graphs.

Make debuggability a feature: resource names and lifetimes, event traces, allocations, script errors, GPU validation and frame captures. Assertions explain violated contracts. Profiling establishes whether a batch or an allocation is actually expensive before adding complexity. Deterministic simulation is an opt-in profile with recorded input, controlled randomness and documented numerical limits; a fixed timestep alone does not guarantee it.

Optional modules should be removable. A shader experiment should not require Python, C#, a network stack or an editor. A headless simulation should not create a window or GPU device.

## Metagen: authored schemas and shared consumers

C# declarations and attributes compile into a definition DLL, which is authoritative for a C#-authored schema. Python declarations should also be able to author a schema, through an explicit Metagen provider and constrained metadata conventions. Choose one authoritative source per schema type; do not maintain equivalent C# and Python definitions by hand. Separate equivalence fixtures can describe the same test schema to verify the providers. The current scalar proof reads C# assemblies and passive Python declarations, and routes both through the consumption SDK. See [tools/metagen/README.md](tools/metagen/README.md).

A shared Metagen reader interprets the reflected metadata consistently and exposes consumption interfaces. Emit a neutral manifest only when a real consumer benefits: native builds without .NET, language tools that cannot load managed assemblies, deployment compatibility checks, or small retro targets. Such a manifest is a derived view of a specific DLL, identified by a fingerprint and generator version, not a second source of truth. Compiler integration through [Roslyn](https://learn.microsoft.com/en-us/dotnet/api/microsoft.codeanalysis.iincrementalgenerator) is optional; an ordinary reflection-based generator is a valid first prototype. Use attribute metadata inspection where possible, so reading declarations does not execute gameplay or attribute constructors. Native and managed memory layout still remain separate from serialized and network formats.

Metagen itself is a reusable consumption SDK, not an engine-specific file generator. A reflected model offers source identity, types, members, relationships, attribute values and diagnostics through queryable interfaces. Consumers need not repeat reflection logic or know how another consumer works.

**Independent Metagen project.** The toolkit now lives in a separate private repository,
pinned by ICE as a Git submodule at `tools/metagen`. Its generic core and portable examples
build/test without ICE. ICE's native consumers and generated caches live in
[tools/metagenintegration](tools/metagenintegration/README.md). Public release remains a
separate decision after licensing, packaging, documentation and independent CI standards
are met. See [DEVELOPMENT.md](DEVELOPMENT.md) for the dependency and branch strategy.

A possible API shape is `IMetaSource` -> `IMetaType` -> `IMetaMember`, with metadata values and relationships available at each level. Consumer libraries can query this model or use a visitor; they do not need to implement a single mandatory emitter interface. One can draw an infographic, another emit plain text or INI, another generate language bindings, another derive shader layouts. Source loading, metadata interpretation, output rendering and target-specific policy remain separate responsibilities. The model need not erase unknown attributes or force every definition to inherit an ICE object base class.

Shader consumers need explicit layout rules for their target, rather than assuming CLR layout matches shader-buffer layout. A binding consumer likewise owns the target ABI/marshalling rules. Metagen can support reusable policies and diagnostics without choosing one universal representation for every use.

The schema needs stable explicit type/field IDs, versioning and migration policies; ownership and reference semantics; field units, ranges and editor hints; serialization inclusion; and separate replication policy. Renaming a field must not accidentally change its wire identity. Native memory layout, persistent storage format and network representation are separate contracts.

Generate C/C++ declarations and ABI adapters, serializers, migration scaffolding, editor descriptions, binding manifests and language wrappers. Treat automatic data binding as generated observability and explicit updates, not invisible mutation on every field access. Treat replication as explicit authority, change tracking, cadence, quantization and reliability rules rather than attaching a generic Networked attribute to everything.

The scalar settings proof and rich graph/container conformance now establish native round trips, migration and independent provider/generator examples. Continue proving concrete consumers before expanding the attribute library. Cache compiler outputs so shipping an ordinary native build can consume generated artifacts without starting .NET.

## Language runtimes and shared bindings

- Interpreted Lua: the first compact embedded gameplay runtime, with protected calls,
  bounded memory/instructions, source URI diagnostics and prepared replacement.
- LuaJIT: an additional optional backend, with explicit language compatibility and JIT
  capability reporting. It is not assumed to accept every script authored for newer Lua.
- CPython: experiments, numerical tools, asset pipelines and automation. Use batch
  interfaces instead of crossing into Python per particle or per vertex.
- Hosted .NET, with C# as the primary gameplay/tool language: distinct from the existing
  C# Metagen source provider. Native AOT is another deployment profile, not arbitrary
  assembly hot reload. [Native AOT interop](https://learn.microsoft.com/en-us/dotnet/core/deploying/native-aot/interop).
- V8: the JavaScript runtime adapter, with explicit jobs, isolate ownership, termination,
  resource budgets and platform capabilities.

All adapters share Metagen schema semantics and ABI rules. C# assemblies and Python
definitions are already alternative authoring inputs; that does not embed either runtime
in the engine. Each runtime owns its managed objects; native handles mediate references.
Unload/reload needs generations and explicit teardown so stale callbacks cannot invoke
dead runtimes. Implement and prove interpreted Lua first, then optional .NET/C#, CPython,
V8 and LuaJIT consumers of the same binding contracts. Do not turn every target into a
mandatory SDK download. Report interpreter/JIT/AOT and isolation capabilities per target;
an in-process interpreter is not a security boundary against native extension failures.

Use [entity composition](docs/object-composition.md) for gameplay: stable objects with
optional render, material, body, script and trigger features. Scripting must drive those
features through the same validated scene operations available to humans and agents.

## Rendering across eras

The fixed-function OpenGL renderer and old device APIs are removed. Build a new explicit rendering interface around modern resource, pipeline, synchronization and surface requirements. Retained CPU import/animation fixtures provide reference data; backend pixel correctness must be established again.

The common interface describes buffers/textures, pipelines, bindings, draw/dispatch work, render passes, synchronization and presentation surfaces. It exposes capabilities and native extensions. Resource state tracking and lifetime validation belong in the debuggable layer. Avoid promising that every backend supports the same shader features, binding model or command submission behavior: [D3D12 binding differs substantially from older Direct3D](https://learn.microsoft.com/en-us/windows/win32/direct3d12/important-changes-from-directx-11-to-directx-12), and [Metal has its own argument-buffer model](https://developer.apple.com/documentation/metal/improving-cpu-performance-by-using-argument-buffers).

Metal and OpenGL 4.1+ core now share the first procedural pipeline/presentation/capture
slice, with actual Mac/Linux tests and same-window Mac backend handoffs. Keep the two
close in feature parity as buffers, textures, mesh draws, depth and glTF materials are
added. Mac OpenGL's 4.1 ceiling defines one portable profile, not every backend's maximum
capability. Windows OpenGL has compile/link coverage; runtime validation awaits hardware.
Add Vulkan against the same contracts on Linux/Android, then D3D12 when Windows test
hardware is available. Resize, multiple previews, failed preparation and retained CPU
state must continue working as each resource type is introduced.

Keep backend-specific shaders available for experiments. Shared material shaders can acquire an offline compilation path after two real backends demonstrate the requirements. For Apple targets, native Metal stays the first choice for this learning project; Vulkan portability layers are a separate experiment. [Khronos documents the additional layer needed for Vulkan on Apple platforms](https://github.khronos.org/Vulkan-Site/guide/latest/windowing_audio_input.html).

Define capability profiles rather than a universal lowest common denominator: portable programmable rendering, modern compute/advanced rendering, and optional constrained retro experiments. A scene declares requirements. The asset pipeline can produce simpler variants; unsupported features report useful errors rather than silently changing the image.

## Runtime renderer replacement

Runtime backend switching is an explicit design goal for the rebuilt renderer. Retain
CPU-side asset identity, resource descriptions and supported shader variants independently
of a device. Prepare the replacement device, surfaces, pipelines and uploads while the
current scene remains active; expose progress and capability failures as diagnostics.
At a host boundary, quiesce submissions, finish the old device's in-flight work, switch
validated bindings/presentation and advance resource-handle generations. Keep simulation,
script state and the session alive. Some native surfaces may need recreation; describe
that transition rather than treating a swapchain as a permanent window-owned object.

Device loss/recovery is a related path with different guarantees: the old device may no
longer be usable while preparing a replacement. Keep enough retained descriptions to
rebuild resources and report unrecoverable loss. A backend switch may temporarily double
resource residency; enforce an explicit preparation budget and allow a deliberate paused
replacement when overlap is impossible. A prepared candidate must satisfy the scene's
required capabilities before publication.

Runtime module loading/unloading is optional and follows the lifetime model, rather than
reusing the old renderer DLL machinery. A versioned function table/C ABI can expose
backend modules without exporting C++ container/vtable layout. Do not unload a module
until its callbacks, worker jobs, function pointers, device resources and GPU submissions
are gone. Retain modules when safe unloading cannot be proven. Malformed data/shader errors
can be recoverable; arbitrary in-process native corruption cannot be promised recoverable.
Use disposable workers where crash containment is actually required.

Remove abandoned fixed-function and old backend APIs instead of carrying compatibility
layers into the modern interface. Metal, OpenGL, Vulkan and D3D12 should be driven by the same
resource/command/lifetime tests, with backend capabilities and extensions explicit.

## Retro targets as experiments

An original Xbox target would test allocation discipline, asset conversion and small runtime profiles. The open homebrew route is [nxdk](https://github.com/XboxDev/nxdk), including pbkit for the GPU; it is not interchangeable with the desktop Direct3D SDK. Keep modern GPU features and heavyweight language runtimes optional. Start with a tiny rendered scene or Airhockey-derived demo, then measure its actual memory and timing budgets.

Longevity comes from documented data formats, reproducible offline builds, backend isolation, explicit capabilities, readable code and retained reference samples. It cannot guarantee that future toolchains or hardware will behave identically.

## Agents and resilient live development

Agents are first-class development clients alongside humans, inspectors, and scripts.
They should inspect and change a running playground, observe the outcome, and keep or
undo an experiment without restarting the game for each data/script edit. Keep these
capabilities in an optional development module; ordinary games do not need an agent,
MCP server, Python, or an always-on network endpoint.

Use one typed runtime command service for the inspector, scripts, and agent adapters.
MCP is an initial adapter, not the engine object model or the internal command protocol.
A local CLI/IPC client is enough to prove the service before implementing MCP. Current
engine APIs do not yet expose this service. See the [MCP architecture](https://modelcontextprotocol.io/docs/2026-07-28/learn/architecture)
for its separation of hosts, clients, and servers.

The initial command surface should support:

- Inspect schemas, stable object handles, scene state, resource URIs, and diagnostics.
- Submit a batch of typed changes against an expected scene revision; validate the
  entire batch before applying it at a simulation/frame boundary.
- Prepare replacement assets/scripts away from the active scene, then swap only on
  success. Preserve the last working version and explicit compatible state.
- Pause, step, capture an image, record/replay input, and collect bounded performance
  measurements so an experiment can be evaluated rather than merely described.
- Return a change receipt, diagnostics, and an undo record. Undo restores supported
  engine state; it cannot reverse arbitrary external file/network effects.

No raw pointers cross this boundary. Use generation-checked handles, operation IDs,
explicit lifetimes, bounded requests/queues, and revision checks to reject stale edits.
An edit transaction must include staged resource allocation and reversible commit work;
validation alone is insufficient if applying the edit can fail halfway through.
Persist authored changes only through an explicit save operation. A schema change that
requires native regeneration/recompilation is distinct from a live instance-data edit;
this does not make arbitrary C++ changes reloadable.

Resilience means malformed data, failed compilation, stale references, and ordinary
script exceptions become diagnostics while the session retains working state. Validate
inputs, impose script time/memory budgets, and keep recovery snapshots or input logs.
These measures cannot guarantee an in-process C++ engine never crashes. Use disposable
worker processes for schema loaders and experimental/native extensions when crash
containment is required; process separation still needs OS restrictions for a real
security sandbox. Native execution on the render thread remains part of the engine's
trusted boundary.

A complete replication stack is not a prerequisite: the first editor/agent client can
use local IPC. We do need enough schema, reference, serialization, and lifecycle
semantics to make a small set of mutations safe. Start with one runnable experiment,
then grow the supported operations from demonstrated needs.

## Independent host applications

Applications compose reusable engine modules and own their product behavior, assets
and command policy. Engine examples and conformance tests should demonstrate the
public module contracts without requiring an application checkout.

## Current contracts and follow-on work

The independently owned Metagen SDK is pinned as an ICE submodule. Native adapters
and generated consumer caches live in ICE. The rich data model supports owned
records/references, bounded preparation and revision-checked publication; native
JSON has strict Unicode, exact integer handling and resource budgets. The optional
`ice::commands`, `ice::script_lua`, physics and semantic-input modules expose separate
headless contracts. See [commands](docs/commands.md), [scripting](docs/scripting.md),
[physics](docs/physics.md), [input actions](docs/input-actions.md) and
[Metagen integration](tools/metagenintegration/README.md) for supported behavior.

Preserve tested C++20/23 behavior, explicit resource ownership and backend parity
as those modules evolve. Prepare changes before activation, reject stale identities
and keep failures from replacing working state. Optional modules must remain useful
without a window, device or unrelated runtime. Physical device behavior and new
backend capabilities require their own evidence.

Future engine examples should exercise one bounded public contract at a time.
Language/runtime bindings, component composition and networking remain explicit
profiles rather than implicit features of a metadata declaration. The
[networking metadata example](tools/networkmetadata/README.md) proves codec/policy
generation; it does not implement a replicated room.

Airhockey and SkeeBall remain source/art/physics examples awaiting ports onto the
modern renderer. Keep their provenance and focused gameplay checks while replacing
retired APIs. Mobile/browser and XR probes retain their own supported build and
hardware records; a successful host build does not establish device acceptance.
