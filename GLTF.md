# glTF and URI playground

EngineTest now opens a static glTF scene by default. `--gltf <path-or-URI>` selects
another scene; `--project <path-or-URI>` selects its resource mounts. `--legacy`
retains the Lua cube/pathfinder exercise. Left drag orbits, right/middle drag pans,
wheel zooms, F frames, Space pauses rotation, N cycles inspection modes, W toggles
wireframe, M isolates a material, C switches helmet/material lab and R reloads.
A failed import, shader compilation or upload keeps the previous scene usable.

## Resource identities

`icResourceURI` parses and resolves hierarchical URIs. `icResourceResolver` dispatches
byte reads through `icVirtualSchemeHandler` and applies project mounts. glTF models,
their relative buffers/images and renderer shader sources use this boundary.
EngineTest's `ice.project` contains a small `[mounts]` section, for example:

```ini
[mounts]
assets://project/ = ./Resource/
shaders://engine/ = ../../Engine/Resources/shaders/
textures://project/ = assets://project/textures/

[search]
shaders://project/ = assets://project/shaders/
shaders://project/ = shaders://engine/
```

Repeated entries in `[search]` define ordered roots: a game override first, then the
engine library. An existing empty or invalid shader still wins; shader compilation
reports its error rather than silently choosing another source. Providers distinguish
`NotFound` from `Error`, so permissions/I/O failures stop the search. Exhausted lookup
diagnostics list the attempted locations. `MountSearch` exposes the same policy in C++.
The explicit `shaders://engine/` namespace always addresses the engine library.

Targets resolve relative to the project document, not the process working directory.
The packaged project mounts its copied resources instead. Logical identity survives
a storage-provider change. The built-in provider reads local `file://` resources;
custom providers can supply other schemes. HTTP, credentials, asynchronous reads,
offline caches and cancellation are future work. Register providers and mounts before
starting loads; configuration is not currently synchronized for concurrent mutation.

Queries are part of document identity; fragments select objects within a document.
The resolver preserves query order, repeated keys and literal `+`, and removes only
the fragment before reading bytes. It does not interpret every query as form data.
The file provider rejects queries instead of silently discarding them. Native paths
enter through `FromPath`; percent-escaped dependencies decode once at the provider.

### Prepared scripts and containers: proposed next contract

`scripts://project/table.lua?seed=42&tag=wood&tag=metal#prepared` can identify a
prepared invocation. A typed loader above byte I/O should use metagen input metadata
to validate types, defaults and constraints. A prepared object is not necessarily a
byte stream, so this belongs above `icVirtualSchemeHandler::Read`.

The loader should define whether input order matters and normalize typed values for
its cache key. Include source revision, resolved inputs and dependency revisions;
never cache merely by pathname. Keep identity distinct from mutability: callers
needing isolated mutable script state must explicitly instantiate it rather than
accidentally sharing a cached running VM. Do not execute scripts merely because a
serializer encountered an external reference; preparation/execution is an explicit
loader capability.

Container URIs can expose generated collections or filtered views. Use stable element
IDs or map keys for member references; list positions describe a view, not durable
object identity. A list-index selector can still be useful for transient inspection.
Query/default/type semantics and fragment selectors belong to each typed loader's
documented contract. These typed loaders are proposals, not implemented features.

## Rendering boundary

`icShader` represents a compiled stage; `icPipeline` represents linked stages, vertex
layout and render state. `icShaderGL`/`icPipelineGL` implement desktop GLSL. The
transitional `icProgramGL` keeps SkeeBall's linked-program usage working. Shader
sources live under `Engine/Resources/shaders` and load through `shaders://project/`,
which falls back to `shaders://engine/` unless the game supplies an override.
R reload also recompiles them. Objects must be released before their GL context.

The CPU scene representation and importer have no GL dependency. Meshes are shared
by instances; the GL renderer owns uploaded resources. It handles inverse-transpose
normals, mirrored winding, UV0/UV1, vertex colors, metallic/roughness, normal maps,
occlusion, emissive, alpha mask/blending and double-sided surfaces. Linear lighting
and blending use an RGBA16F framebuffer, followed by tone mapping and sRGB encoding.
Lighting uses two studio lights and an analytic environment approximation, not
prefiltered image-based lighting. Missing tangents use a simple UV derivative method,
not exact MikkTSpace. Transparency sorts instance centers and cannot correctly order
all intersecting transparent geometry.

This implementation requires compatibility GLSL 1.20, framebuffer-object functions
and renderable floating-point color targets; unsupported capabilities produce an
error. The common interfaces are an initial backend boundary, not completed Metal,
Vulkan or DX12 implementations. Descriptor layouts, synchronization and compute
still need concrete backend work.

## Import scope and validation

Supported: static glTF 2.0 JSON/GLB, external and data resources, PNG/JPEG, sparse and
interleaved accessors, normalized attributes, indexed/nonindexed triangles, strips
and fans, hierarchy and shared instances, core materials and KHR_materials_unlit.
Skins, animation, morph targets, compressed geometry and required unsupported
extensions fail explicitly. Parsing, decoded images and geometry have bounded
budgets; a candidate replaces the previous scene only after successful loading.

Headless CTest suites `ice_gltf_assets` and `ice_resource_uri` cover importer and
resolver behavior. `ice_scene_render_tests <RenderProbe.glb> <ice.project>` requires
a real GL display and checks actual pixels for UVs, mirrored culling, alpha sorting,
masking, normal maps, channel selection and resized HDR output. EngineTest
`--gltf-smoke <existing-directory>` captures debug modes and exercises reload/resize.

Checkpoint 2026-10-14: all 13 CTest suites pass on macOS and Linux/spacegoo.
The graphical pixel suite, viewer inspection/reload/resize suite, legacy EngineTest,
and SkeeBall shader/fallback smoke runs pass on both. All desktop targets cross-build
with MinGW for Windows; Windows runtime remains unverified. The headless importer
also passes AddressSanitizer/UndefinedBehaviorSanitizer on Linux. The local Mac ASan
runtime stalled during its own initialization, so it supplied no test result.

## Provenance

Readers: pinned cgltf (MIT) and stb_image (MIT/public domain), with licenses and
upstream records beside the vendored files. Generated material/probe fixtures are
CC0; regenerate with `Tools/GltfFixtures/generate.py`.

The Damaged Helmet is a noncommercial reference asset: the included upstream
README/LICENSE identify ctxwing's CC-BY-4.0 conversion and theblueturtle_'s original
CC-BY-NC-4.0 work. Do not treat it as unrestricted shipping game content.

References: [glTF specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html),
[cgltf](https://github.com/jkuhlmann/cgltf),
[sample asset](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/DamagedHelmet).
