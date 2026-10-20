# Ordinary motion and action router consumers

The root ICE project exports portable `ice::motion` and `ice::action_router` targets and
includes both in `ice::engine`. No native device, scripting, physics or companion module
is required. `cmake/ICEActionRouting.cmake` can also be included directly with the existing
portable dependency modules; the existing motion module is reused unchanged.

This small external project uses `add_subdirectory` on ordinary ICE, then compiles the
same consumer linked to the two module targets and to the engine aggregate. Neither
consumer compiles library source manually. Optional backends are disabled inside this
fixture to keep it CPU-only and offline. Production defaults are unchanged.

```sh
cmake -S tests/motion_action_consumer -B build/motion-consumer20 \
  -DICE_CXX_STANDARD=20 -DICE_WARNINGS_AS_ERRORS=ON \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/motion-consumer20 --parallel 2
ctest --test-dir build/motion-consumer20 --output-on-failure --parallel 2
```

Repeat with standard 23 and a separate output directory. The fixture inherits ICE's
language level and strict warning policy. For Android or Apple mobile cross-compilation,
pass the normal CMake toolchain/system arguments; build the targets without running CTest
on the host. No runtime mobile or hardware qualification is implied by a cross-build.
The retained iOS Simulator 26.2 SDK checks distinguish the module consumer at deployment
target 15 from the aggregate at 16.3. The existing aggregate JSON floating `to_chars` call
is unavailable below 16.3 in that SDK; the initial target-15 aggregate failure is retained.
This registration change does not alter deployment defaults or JSON source.

Ordinary `BUILD_TESTING=ON` ICE now registers the original seven motion cases and nine
action-router cases, plus `ice_motion_action_consumer`. Those tests reuse the frozen test
sources and case names; their separate standalone projects remain unchanged. The public
header compilation also includes both new module headers.
