# glTF, animation and resource URIs

`ice::assets` loads glTF into CPU-owned scene descriptions without a graphics context.
It retains hierarchy, shared instances, material descriptions, decoded images, animation
clips and skins. The former EngineTest/OpenGL inspector was removed; its core importer,
pose evaluation and fixtures remain as inputs to the modern renderer.

## Resource identities

`ice::io::resource_uri`, `resource_resolver` and `virtual_scheme_handler` provide
hierarchical identity, scheme dispatch and ordered mounts. Project files can contain:

```ini
[mounts]
assets://project/ = ./assets/
shaders://engine/ = ./engine-shaders/
textures://project/ = assets://project/textures/

[search]
shaders://project/ = assets://project/shaders/
shaders://project/ = shaders://engine/
```

Relative targets resolve against the project document. Repeated search roots implement
overrides. Only `not_found` permits fallback; a provider error stops lookup. An existing
empty/invalid shader wins and should produce its actual compilation diagnostic. Logical
identity survives a provider/storage change. Built-in local file reads work; HTTP,
credentials, caching, asynchronous preparation and cancellation remain future work.

Queries/fragments are preserved and relative dependencies resolve through the same URI
rules. Handler-specific inputs must eventually define canonicalization, versioning,
cache keys and resource budgets. Prepared scripts and virtual containers are planned
consumers; URI parsing alone does not implement execution or container factories.

## CPU import and animation

Supported: glTF 2.0 JSON/GLB, external/data resources, PNG/JPEG, sparse/interleaved and
normalized accessors, indexed/nonindexed triangles/strips/fans, hierarchy/shared
instances, core materials and KHR_materials_unlit, TRS animation and four-weight skins.
Unsupported required extensions, morphs, extra influence sets and compressed geometry
fail explicitly. Parsing/images/geometry have bounded budgets; failed loads preserve
the prior scene. This is not a sandbox for arbitrary native decoder faults.

Animation supports clip sampling, hierarchy evaluation, skin palettes, reference pose
bounds and local-pose blending. Quaternions use normalized shortest-path interpolation;
blends preserve authored rest components and rebuild globals/palettes from local poses.
The CPU evaluator allocates working arrays and is not yet a crowd animator. Layer graphs,
root motion, events, retargeting and modern GPU palette transport remain future work.

`scene_camera` uses RH world-to-camera matrices, +Y up, with explicit clip depth. Default
is [0,1]; callers can select [-1,1]. Viewport orientation and winding belong to a backend.
The engine contains no surviving GL shader, GPU skinning or PBR implementation in this
checkpoint; modern backend tests must re-establish image correctness.

Headless `ice_gltf_assets`, `ice_resource_uri` and `ice_scene_animation` tests cover
resource decoding, transactional failure, hierarchy/skin/blend behavior and camera math.
Generate owned fixtures with `Tools/GltfFixtures/generate.py`.

## Provenance

Pinned cgltf and stb_image retain upstream licenses/records in `engine/3rdparty`.
Generated probes/materials are CC0. Fox under `tests/data/gltf/Fox` retains PixelMannen's
model (CC0), tomkranis's rig/animation (CC-BY-4.0) and AsoboStudio/scurest's conversion
(CC-BY-4.0), pinned with upstream records. It supplies Survey/Walk/Run clips.

Damaged Helmet is a noncommercial reference asset: its included README/LICENSE records
ctxwing's CC-BY-4.0 conversion and theblueturtle_'s CC-BY-NC-4.0 original. Do not treat it
as unrestricted shipping content. See the per-asset license files before redistribution.
