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
translation units in `scope.json`; this is separate from the engine-wide compiler
warnings and public-header build checks.

The compiler-based API audit checks the active platform configuration: public
Doxygen coverage, named snake_case arguments, matching declaration/definition
argument names, and valid Doxygen parameter tags. It requires Clang builtin
headers as well as the pinned libclang package. Xcode supplies them on macOS;
install `clang-18` on Linux. `--resource-dir` can select an explicit Clang resource
directory. Run on each platform to inspect conditional APIs; a successful Mac
audit does not cover the Win32 or X11 branches. Comments still require human
review for accuracy and usefulness.

CMake builds `tests/public_headers.cpp` to ensure supported public headers can be
included together. Historical, unimplemented APIs live in `Experimental/legacy`.
The GitHub Actions workflow checks Linux and macOS; Windows runtime validation
requires a Windows machine.
