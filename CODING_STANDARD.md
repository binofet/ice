# ICE coding standard

ICE targets C++20 and Objective-C++20 through CMake. The engine API may change
without compatibility aliases during this migration.

## Names and directories

Engine-owned directories and filenames use lowercase snake_case. The public
include root is `engine/include/ice`; include headers using paths such as
`ice/math/mat4.h` and `ice/animation/scene_animation.h`. Implementations live in
`engine/src`, with the same responsibility directories. Engine assets live in
`engine/resources`. Preserve upstream third-party names and platform project
bundle names when their tooling requires them.

Use `ice` as the root namespace, with responsibility namespaces such as `math`,
`platform`, `input`, `io`, `assets`, `animation`, `render`, and `net`. Backend APIs
belong in namespaces such as `ice::render::gl`. Types, functions, methods, enum
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

## Validation

Use the desktop CMake presets for C++20 builds. Quality tools are pinned in
`Tools/Quality/requirements.txt`; run `Tools/Quality/check.py` for the configured
formatting and analysis scope. Build and test on a case-sensitive filesystem
when moving directories or changing include paths. Review the complete migration
diff before committing or pushing.

## Migration status

The working tree contains the breaking namespace/directory/API migration and the
GLM conversion. The argument-name and public-documentation audit is automated;
review comments for semantic quality as well as coverage. Native platform entry
points retain operating-system spellings. A small set of legacy result macros
and format/usage constants still retain historical prefixes; their conversion
is separate from the completed type/method/member/argument naming pass.
See `Tools/Quality/README.md` for check commands and platform coverage limits.
CMake is the maintained build system. Historical IDE project files do not define
the current source list.
