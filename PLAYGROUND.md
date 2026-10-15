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

## Metagen: authored schemas and shared consumers

C# declarations and attributes compile into a definition DLL, which is authoritative for a C#-authored schema. Python declarations should also be able to author a schema, through an explicit Metagen provider and constrained metadata conventions. Choose one authoritative source per schema type; do not maintain equivalent C# and Python definitions by hand. Separate equivalence fixtures can describe the same test schema to verify the providers. The current scalar proof reads C# assemblies and passive Python declarations, and routes both through the consumption SDK. See [Tools/Metagen/README.md](Tools/Metagen/README.md).

A shared Metagen reader interprets the reflected metadata consistently and exposes consumption interfaces. Emit a neutral manifest only when a real consumer benefits: native builds without .NET, language tools that cannot load managed assemblies, deployment compatibility checks, or small retro targets. Such a manifest is a derived view of a specific DLL, identified by a fingerprint and generator version, not a second source of truth. Compiler integration through [Roslyn](https://learn.microsoft.com/en-us/dotnet/api/microsoft.codeanalysis.iincrementalgenerator) is optional; an ordinary reflection-based generator is a valid first prototype. Use attribute metadata inspection where possible, so reading declarations does not execute gameplay or attribute constructors. Native and managed memory layout still remain separate from serialized and network formats.

Metagen itself is a reusable consumption SDK, not an engine-specific file generator. A reflected model offers source identity, types, members, relationships, attribute values and diagnostics through queryable interfaces. Consumers need not repeat reflection logic or know how another consumer works.

**Public Metagen project (planned).** Once references, containers, inheritance and the
format contract are functioning with useful examples, extract Metagen into its own
repository and publish it. Keep its consumption SDK, providers, generators and test
corpus independent of ICE now; ICE will become a versioned consumer. Prepare licensing,
standalone build/test instructions, C#/Python authoring examples and C++/Rust consumers
before release. Extraction and publication are future milestones, not authorized actions
to create a repository or publish this working tree today.

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

All adapters share Metagen schema semantics and ABI rules. C# assemblies and Python definitions are alternative authoring inputs; a universal garbage collector is not required. Each runtime owns its managed objects; native handles mediate references. Unload/reload needs generations and explicit teardown so stale callbacks cannot invoke dead runtimes. None of these runtimes is implemented by the new event foundation.

## Rendering across eras

Retain the current OpenGL sample as the reference during migration. Add a new rendering interface alongside the legacy device instead of stretching fixed-function calls into modern resource/barrier semantics.

The common interface describes buffers/textures, pipelines, bindings, draw/dispatch work, render passes, synchronization and presentation surfaces. It exposes capabilities and native extensions. Resource state tracking and lifetime validation belong in the debuggable layer. Avoid promising that every backend supports the same shader features, binding model or command submission behavior: [D3D12 binding differs substantially from older Direct3D](https://learn.microsoft.com/en-us/windows/win32/direct3d12/important-changes-from-directx-11-to-directx-12), and [Metal has its own argument-buffer model](https://developer.apple.com/documentation/metal/improving-cpu-performance-by-using-argument-buffers).

Implement Metal first on this Mac, Vulkan next on spacegoo, and D3D12 with Windows compile checks followed by actual hardware validation when available. Each backend must clear a surface, draw a shader triangle, upload a textured mesh, handle resize and independent previews, then run the same scene. These are planned backends, not completed implementations.

Keep backend-specific shaders available for experiments. Shared material shaders can acquire an offline compilation path after two real backends demonstrate the requirements. For Apple targets, native Metal stays the first choice for this learning project; Vulkan portability layers are a separate experiment. [Khronos documents the additional layer needed for Vulkan on Apple platforms](https://github.khronos.org/Vulkan-Site/guide/latest/windowing_audio_input.html).

Define profiles rather than a universal lowest common denominator: legacy/fixed-function, portable programmable rendering, and modern compute/advanced rendering. A scene declares requirements. The asset pipeline can produce simpler variants; unsupported features report useful errors rather than silently changing the image.

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

## Ignition: the interactive playground

**Ignition** becomes the primary development application after the current Metagen/data
milestone. Keep EngineTest as a regression fixture; new live development belongs in
Ignition. Its first experience is deliberately small: start a native window/GPU surface
quickly, show a procedural GPU logo, and arrive at a usable prompt. Expensive schema,
asset and language services initialize asynchronously and report readiness without
holding the first presented frame hostage.

The opening is a mathematical GPU animation, with selectable **1, 2, 3 and 5 second**
versions for comparison. Generate the image from code and shader math; timing must be
based on elapsed time rather than frame count. Support skipping it and a reduced-motion
option. Benchmark cold start separately from animation duration: a long logo must not
conceal a slow startup. Keep the initial shader small enough to compile quickly; a more
elaborate warm-up can prepare pipeline variants while it runs.

The next view is black with luminous green text, using a restrained shader treatment
inspired by a 1980s display. During the first **30 seconds after entering the terminal**,
a fading ghost/flicker effect suggests something arriving in the machine. It then settles
into a simple, stable prompt. Text must stay legible, the cursor and typed input must stay
responsive, and effects can be disabled. Use actual text-input events with Unicode/IME
support rather than synthesizing text from physical key codes.

Human input and connected AI clients share one typed command service: discover
capabilities, inspect, prepare a change, activate at a safe boundary, observe and undo.
The terminal is a client of this service; MCP is another adapter. Show connected clients,
operation progress, diagnostics, ownership and revision conflicts. An agent may issue a
long-running operation without blocking presentation or stealing human input. Start with
local connections; explicitly configured project capabilities govern file/network/code
access. A GPU device, compiler, language runtime or network request can fail independently
without invalidating the last working scene.

**iOS and Android are first-class Ignition hosts from the start**, alongside desktop.
The shared document/command/session model must not depend on a desktop terminal, mouse,
filesystem layout or window singleton. Native presentation, touch/pen, text/IME, voice,
motion/camera, permissions, background/foreground transitions, memory pressure and surface
loss are host capabilities with explicit lifecycle rules. Mobile can run a local scene,
control a remote one, or present a remote simulation through an independently negotiated
view. Preserve responsive local input and a clear connection/recovery state.

**VR and AR are first-class host profiles too**, with **Quest 3** as the first available
XR hardware target. Model tracked views/poses, action input, hands/controllers, spatial
UI, session focus/visibility and frame scheduling explicitly. Presentation can have multiple
views and projection/layer requirements; it must not assume one camera feeding one flat
window. The same command/session service powers a spatial prompt, local immersive scene
or remote simulation. A capability-based XR adapter should share the renderer/asset model
while exposing device-specific features. Establish comfort controls and frame-time budgets
in the first sample, and test pause/resume, tracking loss and reconnection on the user's
Quest 3. AR composition, scene understanding and passthrough are opt-in capabilities to
prove on each device, not assumptions made by every game. Keep a desktop preview useful
for development without treating it as XR hardware validation.

The first **Ignition XR scene** is an intentionally anachronistic workspace: an old
typewriter, a ribbon microphone and a desk under flickering candlelight, connected to an
immense computer somewhere beyond the room. Direct typing animates the keys; AI replies
appear on the paper; voice enters through the microphone. Experiments can grow from the
desk into the surrounding space. The candle is a lighting/material experiment, while the
typewriter/paper is a spatial view of the same command transcript as the green terminal.
Keep the document/session contract shared, let the user choose input/presentation, and
make ambient flicker independently adjustable. Begin with authored procedural objects;
record provenance for any imported models, textures or audio.

**Watch is an optional companion target in the design.** The motivating demonstration
is a person on stage wearing only a watch: speak an idea, connect to a workstation or
supercomputer running the simulation, and see the result on a stage display. The watch
client sends intent through the same session/command protocol, reports progress and
provides confirmation/cancel/undo controls. Compute, rendering, speech recognition and
agent orchestration may be remote capabilities; do not require a full engine renderer or
language runtime on the watch. Account for reconnects, latency, tiny displays, battery and
background limits. Separate simulation authority, controller and presentation endpoints
so this scenario does not require the watch to relay rendered frames. Device support
and transport details need later hardware proofs; no new watch deployment is part of
the current data milestone.

Build scenes from primitives, procedural geometry and generative 2D/3D shaders first.
Asset downloads are a deliberate URI-provider capability, not an invisible side effect
of scene loading. Preserve source URL, creator, license/SPDX or license text, attribution,
retrieval time and content hash alongside every downloaded asset, including derivatives.
Missing/unclear permission is a reported import issue. Cache by content identity and retain
an offline path. Record the generator/version/seed/inputs for procedural assets as well.

The public command surface should eventually reach each supported programmable stage:
shader sources and compilation diagnostics, reflected bindings, geometry, materials,
passes/render targets, draw/dispatch, synchronization, presentation, capture and timing.
Expose backend capabilities and extensions rather than pretending OpenGL, Metal, Vulkan
and D3D12 have identical stages. Grow a runnable example for each technique as the backend
supports it, with required features and expected output documented.

Once the prompt and MCP adapter work, test them with exploratory agents that receive
**only public documentation and tool discovery**, without source access or private
implementation hints. Have them build examples, report missing capabilities and reproducible
failures, and keep operation receipts/captures. These trials measure usability of the
public interface; do not repair a trial by giving the agent an undocumented mutation path.

Scripts provide rapid experimentation. Promote useful, performance-critical behavior to
native C++ or the appropriate platform/GPU implementation when profiling justifies it.
Preserve the same schema/command contract and comparison fixtures so promotion can be
measured without changing authored meaning. Generated code is a consumer, not a special
privileged way around lifecycle and validation rules.

## Implementation order after the standards/GLM milestone

The desktop native backends, mobile controller probes, URI resource search, glTF
materials/animation/skinning, and standards/GLM migration form the current foundation.
CMake-selectable C++20/C++23, C#/Python schema proofs, native/RTTR/Rust consumers and
headless transactional data labs extend it. Native graph preparation/activation is now
implemented independently of graphics; gameplay/GPU resource activation and the shared
command service still need their own contracts. See [Tools/Metagen/README.md](Tools/Metagen/README.md).

1. **Metagen/native data foundation.** Complete the current review/validation and push.
   The scalar runtime and rich Python/C++ graph conformance now cover enums, recursive
   containers, inline/local/external references and opt-in prototypes/merge operations.
   Native snapshots support revision-checked activation, checked handles and undo.
   Continue richer adapters, cooked binary benchmarks and native JSON/XML/codec consumers
   from real needs; the generic Metagen SDK remains independent. See
   [Tools/Metagen/DATA_MODEL.md](Tools/Metagen/DATA_MODEL.md).
2. **Ignition boot and prompt.** Establish shared desktop/iOS/Android host and session
   boundaries, an XR profile targeting Quest 3, and an optional watch remote client. Add the native application, procedural GPU logo
   variants and the timed green terminal. Establish text input, command history,
   diagnostics, resize/fullscreen and an inspectable readiness model. EngineTest remains
   a regression fixture and receives no new interactive development surface.
3. **Resilient live development in Ignition.** Connect detached data preparation and
   revision-checked publication to the host boundary; inspect, replace and undo a scene.
   Add one reloadable Lua behavior with explicit state transfer and a procedural art
   experiment. Document edits that require native regeneration/recompilation.
4. **Agent development clients.** Expose the same command service through local MCP.
   Demonstrate discover -> inspect -> edit -> step/replay -> capture/measure -> undo/save
   without restarting. Run the docs-only agent trials and turn their limitations into
   reproducible examples and interface improvements.
5. **Window-local input and modern rendering.** Finish targeted text/key/pointer events,
   action routing and two independent previews. Build reusable Metal rendering on Mac/iOS,
   then matching Vulkan and D3D12 examples. Keep OpenGL as the tested reference and expose
   capabilities throughout. Validate Windows rendering on hardware when available.
6. **Broader experiments and extraction.** Extend language bindings, headless replay,
   ARKit/ARCore party controllers and optional retro targets as samples require them.
   Extract/publish Metagen after standalone packaging, licensing and CI review.
   SkeeBall physics tuning remains deferred.

Each milestone leaves a runnable playground. Ignition should drive the next requirements
through actual experiments rather than waiting for every future subsystem to be finished.

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
The first no-install touch controller now uses a small local HTTP bridge into the
native UDP path. Permission-based browser motion, HTTPS and an eventual
WebSocket/WebRTC adapter remain future experiments. Host/join
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
