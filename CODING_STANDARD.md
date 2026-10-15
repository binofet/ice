# ICE coding standard

ICE supports C++20 and C++23 (default), with matching Objective-C++ modes through CMake.
Select `-DICE_CXX_STANDARD=20` or `23`, or use the `desktop-cxx20` preset. Other values
are configuration errors. The engine API may change without compatibility aliases.
C++23 does not provide standard
reflection: a Metagen consumer defines portable data semantics and optional RTTR adapters supply
native runtime reflection for the current proof. New standard-library features need
an actual compile/test on supported toolchains, not just an accepted language flag.
NDK r26 selects C++23 with its supported `-std=c++2b` spelling; GNU extensions remain off.

## Names and directories

Engine-owned directories and filenames use lowercase snake_case. The public
include root is `engine/include/ice`; include headers using paths such as
`ice/math/types.h` and `ice/animation/scene_animation.h`. Implementations live in
`engine/src`, with the same responsibility directories. Engine assets live in
`engine/resources`. Preserve upstream third-party names and platform project
bundle names when their tooling requires them.

Use `ice` as the root namespace, with responsibility namespaces such as `math`,
`platform`, `input`, `io`, `assets`, `animation`, `render`, and `net`. Backend APIs
belong in namespaces such as `ice::render::metal`. Types, functions, methods, enum
values, and namespaces use lowercase snake_case. Private data members have a
trailing underscore. Public data fields use snake_case without that suffix.
Avoid type prefixes and Hungarian notation. Function and method arguments also use
lowercase snake_case. Use descriptive roles such as `delta_seconds`, `byte_count`,
and `out_texture`; keep names consistent between declarations, definitions, and
overrides. Doxygen parameter tags must match the current argument names.

## Layout and readability

Use tabs for indentation and spaces for alignment, with a tab width of four and
a target line length of 100 columns. `.clang-format` defines the mechanical
formatting rules; `.editorconfig` defines editor settings. Prefer explicit,
readable logic and useful names over compressed expressions. Explain subtle
algorithms, coordinate conventions, and lifetime constraints in comments.

## Public API documentation

Document every engine public API with Doxygen comments. Describe its purpose and
any relevant ownership, lifetime, failure behavior, units, coordinate system,
and threading constraints. Document parameters and return values when their
meaning is not obvious from the declaration. Comments should explain the
contract rather than repeat the spelling of a function.

## Dependencies and foundational types

Use standard library strings and threading facilities. Use the pinned GLM dependency through `ice::math` aliases and the `ice::math` CMake
target. Vectors, matrices, and quaternions are direct GLM types; do not rebuild the
old class interfaces as wrappers. Use explicit double-precision types where a
simulation needs them. Asset ingestion, animation, and rendering share the same
types rather than defining parallel matrix representations. See `MATH.md` for
storage, quaternion, and camera conventions. Third-party code retains
its upstream conventions and is excluded from first-party formatting.

Use `ice::expected` for checked result values. The pinned public-domain
TartanLlama/expected implementation gives both language modes the same API, including
SDK standard libraries that don't provide `std::expected`. Its include path propagates
through `ice::expected` / `ice::meta`; do not add compatibility names to `std`.

## Validation

Use the desktop CMake presets for either supported language mode. Quality tools are pinned in
`Tools/Quality/requirements.txt`; run `Tools/Quality/check.py` for the configured
formatting and analysis scope. Build and test on a case-sensitive filesystem
when moving directories or changing include paths. Review the complete migration
diff before committing or pushing.

## Migration status

The supported core uses the namespace/directory/API migration and GLM conversion.
Obsolete engine APIs, fixed-function rendering and historical engine build projects
have been removed. Retained game sources under `examples` need ports and do not
establish supported engine API compatibility. Native platform entry points keep OS
spellings. The argument-name and public-documentation audit is automated; review
comments for semantic quality as well as coverage. See `Tools/Quality/README.md` for
commands and platform coverage limits. CMake defines the maintained source lists.
