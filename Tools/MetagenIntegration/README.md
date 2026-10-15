# ICE's Metagen integration

The toolkit is independently owned by `binofet/metagen`, initially private. ICE pins it
at `Tools/Metagen` as a Git submodule. There is no second maintained SDK copy here.
This directory owns ICE's native adapters, RTTR/container examples, live-data lab,
C++/Rust byte agreement harness and generated consumer caches.

## Ordinary builds

The engine and native integration build from checked-in caches; they need no private
Metagen checkout, .NET, Python packages, graphics or network access. Cache integrity
checks require only Python's standard library and Git:

```sh
python3 Tools/MetagenIntegration/check.py --cached-only
cmake -S Tools/MetagenIntegration/native -B build/metagen-native -DICE_CXX_STANDARD=23 -DICE_METAGEN_RTTR=ON
cmake --build build/metagen-native --parallel 4
ctest --test-dir build/metagen-native --output-on-failure
python3 Tools/MetagenIntegration/check_rust.py --native-build build/metagen-native
build/metagen-native/metagen_lab --demo
```

RTTR is optional. Its exact upstream revision is
`7edbd580cfad509a3253c733e70144e36f02ecd4` (MIT), archive SHA256
`bba4b6fac2349fa6badc701aad5e7afb87504a7089a867b1a7cbed08fb2f3a90`.
`FETCHCONTENT_SOURCE_DIR_RTTR` supports verified offline source. Two checked compatibility
patches apply only to a private build copy. ICE selects C++20 or C++23; RTTR builds as C++17.
Rust 1.90+ is needed only for the Rust proof; the script never installs a toolchain.

## Work on both repositories

```sh
git submodule update --init Tools/Metagen
# Work on a named branch inside the submodule, commit and push it first.
python3 -m pip install -r Tools/Metagen/requirements.txt
python3 Tools/MetagenIntegration/check.py --update
git add Tools/Metagen Tools/MetagenIntegration/Generated
python3 Tools/MetagenIntegration/check.py --cached-only
# Review/test, then commit and push this parent-repository dependency update.
```

An explicit `--metagen-source PATH` can regenerate against another checkout. The source
must be committed before updating consumer provenance, and the staged submodule pointer
must identify that same commit. `--csharp-json PATH` reuses adjacent reflected outputs;
it skips source-provider .NET tests. No automatic clone, floating dependency update or
credential handoff is performed. Public ICE CI checks native caches/integration without
fetching private Metagen; the private toolkit runs its own full authoring conformance CI.

## Native contract

`ice::meta` is a focused scalar settings/ICM1/migration proof. `ice::data` owns a dynamic,
validated catalog and graph with explicit references, recursive containers, closed enums
and opt-in prototypes/merge operations. It prepares detached candidates and publishes
against an expected revision. Eight states support undo. Every publication/undo advances
handle generations; retained immutable snapshots stay readable. Live operations belong
on the host thread; preparation can use a separate thread with a thread-safe reader.
Allocator/reader exceptions are outside the validation-diagnostic contract.

51 shared Python/C++ cases cover 13 accepted and 38 rejected graphs, plus native ownership,
handle/history/publication and malformed-binary checks. Effective data agrees byte for
byte with the generated reference views. MDB1 remains an experimental authored tagged
format; it is not a cooked, optimal native layout. Native JSON/XML/codecs, arbitrary typed
object materialization, gameplay/GPU resource activation and a shared command service are
separate consumers. See the Metagen submodule's README/DATA_MODEL for the portable profile.
