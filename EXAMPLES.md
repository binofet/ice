# ICE examples

Airhockey and SkeeBall are retained as game examples under `examples/airhockey` and
`examples/skeeball`. Source, art, original authoring files, recent gameplay/controller
work and existing Box2D/Bullet snapshots remain available. Their shared sample helper
is under `examples/common`; the Airhockey gameplay test moved with its sample.

These examples currently depend on retired engine APIs and are not built by the
modern root CMake project. Their old IDE project files are historical source, not
supported build instructions. The previous ignored build products may still run,
but rebuilding this checkpoint does not produce new game executables.

## Port sequence

1. Prove the modern renderer with native surfaces, shaders, resize and resource lifetime
   tests in the external Ignition application.
2. Add optional engine-level physics integrations with explicit worlds, body/joint
   handles, shape ownership, stepping and debug visualization. Games consume those
   APIs instead of carrying a private engine integration each.
3. Port Airhockey's small scene/input/menu stack; retain its useful anti-stuck, scoring,
   pause and gameplay regression cases. Replace the old unverified network mode with
   a deliberate protocol when multiplayer is revisited.
4. Port SkeeBall meshes/materials, score/menu layout and companion/browser party input.
   Tune physics after the mobile interaction and engine integration are usable.

Use these games to exercise the same engine paths as Ignition. No requirement preserves
legacy APIs or the exact embedded physics library versions. Retain asset provenance and
check distribution rights before publishing a game.

## Other examples

`examples/mobile` contains native iOS/iPadOS, Android and watchOS lifecycle/controller
probes plus the browser controller. See [MOBILE.md](MOBILE.md) and [COMPANION.md](COMPANION.md).
EngineTest and the disposable prototype games were removed. CPU glTF fixtures and the
licensed Fox/Helmet reference assets moved to `tests/data/gltf`.
