# Engine quality checks

Install the pinned tools into a local virtual environment:

```sh
python3 -m venv build/quality-tools
build/quality-tools/bin/pip install -r Tools/Quality/requirements.txt
cmake --preset desktop-debug
cmake --build --preset desktop-debug
ctest --preset desktop-debug
```

On Windows, use the virtual environment's `Scripts` directory in place of `bin`.

```sh
build/quality-tools/bin/python Tools/Quality/check.py format --all
build/quality-tools/bin/python Tools/Quality/check.py lint --build-dir build/desktop-debug
build/quality-tools/bin/python Tools/Quality/api.py --build-dir build/desktop-debug
```

Formatting covers maintained first-party C/C++/Objective-C sources, including
untracked files. Add `--fix` to apply the formatter. Vendor libraries and archived
experiments are excluded. Static analysis currently covers the explicit
translation units in `scope.json`, including its native-platform entries. The default
scope expects a desktop build with the platform's default renderers enabled; select
`--files` for a smaller configuration. This is separate from the engine-wide compiler
warnings and public-header build checks. GPU test executables require an explicit run
on a device/display; ordinary CTest remains headless.

The compiler-based API audit checks the active platform configuration: public
Doxygen coverage, named snake_case arguments, matching declaration/definition
argument names, and valid Doxygen parameter tags. It requires Clang builtin
headers as well as the pinned libclang package. Xcode supplies them on macOS;
install `clang-18` on Linux. `--resource-dir` can select an explicit Clang resource
directory. Run on each platform to inspect conditional APIs; a successful Mac
audit does not cover the Win32 or X11 branches. Comments still require human
review for accuracy and usefulness.

On Apple hosts, the auditor and analyzer query the configured compiler for SDK, libc++ and
framework defaults. This also handles CMake configurations which omit an explicit
sysroot and rely on the `/usr/bin/c++` driver. Keep that compiler/SDK available when
auditing its database; libclang's builtin resource selection remains separate from
the configured standard-library headers.

CMake builds `tests/public_headers.cpp` to ensure supported public headers can be
included together. Retained game sources are not part of the current supported API build.
The GitHub Actions workflow checks Linux and macOS; Windows runtime validation
requires a Windows machine.

For iOS probe analysis, use a compile database configured for the iOS SDK. With the
pinned Clang 18 analyzer and Xcode's iOS 26 SDK, explicitly add the SDK's
`System/Library/SubFrameworks` directory using `--extra-arg=-iframework` and
`--extra-arg=<sdk>/System/Library/SubFrameworks` in a direct `clang-tidy -p <build>`
invocation. Xcode's native compiler finds those SDK subframeworks automatically;
this analyzer search-path adjustment does not change the application's build.
