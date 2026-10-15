# Archived experiments

These sources preserve unfinished or superseded ICE experiments for reference.
They are excluded from the maintained CMake targets and public engine headers.

- `include` and `src`: the original object model, scripting, and event experiments.
- `engine`: dormant DX9/DX11/GLES backends, incomplete geometry/material types,
  old synchronization code, and disabled navigation experiments.
- `build`: historical Visual Studio and Xcode projects whose source lists predate
  the current engine layout.
- `stop_watch.h`: an unimplemented timer prototype, superseded by `ice::util::timer`.

Use the root CMake project and `engine/include/ice` for supported APIs. Restoring
an experiment requires updating it to the current interfaces and adding a real
consumer and validation; presence here does not imply platform support.
