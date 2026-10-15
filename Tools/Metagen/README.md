# Metagen: an extensible metadata and generation toolkit

Metagen's core provides basic attributes, generic reflection/consumption interfaces and
an optional generator convention. Projects can define arbitrary attributes and write
independent processors. Serialization, object graphs, container mappings, RTTR and ICE
integration are consumers. They do not define what every Metagen user must do.

The core SDK builds independently of ICE, the portable-data profile, native libraries,
Python, graphics and networking. Extraction to its own public repository is planned once
rich examples, conformance coverage and packaging are ready. ICE will become an extensive
consumer. Nothing in this working tree has been published as a new repository.

## Boundaries and examples

| Location | Responsibility |
| --- | --- |
| `Metagen/` | Generic metadata interfaces, CLR reader, basic optional schema/field/tag attributes, diagnostics and generator composition. No portable-data dependency. |
| `CoreTests/` | Core-only proof using user attributes, type tokens, arrays, enum arguments, nested generics, nullability and independent generators. |
| `Examples/Documentation/` | A generator referencing only the core; reads custom attributes without referencing their defining assembly or any persistence profile. |
| `Profiles/PortableData/` | An independent C# consumer: scalar/rich data profiles, reference/prototype/merge attributes, container mappings and a scalar projection adapter. |
| `Definitions/`, `Examples/DataDefinitions/`, `Fixtures/` | Independent C# and passive Python authoring fixtures for consumer conformance. |
| `Python/` | Portable-profile provider/validator/generator examples; rich graph reference model, formats and codecs. Not a mandatory core model. |
| `Generated/` | Disposable C++/RTTR/Rust caches, regenerated from authoritative fixture sources. |
| `native/` | ICE native data/RTTR integration and real custom-container example. This integration intentionally consumes ICE's native runtime. |
| `Examples/PartyData/`, `data_lab.py` | Standalone authored party data with external references, prototype operations and format/compression round trips. |
| `Examples/Rust/`, `check_rust.py` | Independent compiled Rust type/serialization consumer and C++ → Rust → C++ byte agreement. |

C# definitions compile into an authoritative source DLL. The reader exposes arbitrary
attributes without invoking attribute constructors or property getters. Unknown metadata
is retained; a custom generator decides its meaning. `IMetaSource.Attributes` also
exposes assembly-level metadata. Recursive type uses preserve generic arguments, arrays,
enum underlying types and available nullable annotations without assigning persistence
semantics in the core. Attribute reading is not a security sandbox; assemblies currently
load into the default load context.

The Python examples use an explicit, constrained AST grammar without importing modules,
evaluating annotations or running factories. The scalar projection can enter the C# SDK
through an independent consumer adapter. Rich C#/Python fixtures compare through their
portable-data projections. This isn't yet a general Python reflection SDK or a reader for
arbitrary decorators. Production types choose one authoritative language; matching sources
here are intentionally independent tests, not a recommended dual-authoring workflow.

`IMetaGenerator` is optional. A consumer may use the query interfaces directly and emit
anything it chooses. Generator composition stages artifacts and diagnostics in memory,
rejects duplicate identities/portable output paths and suppresses artifacts on error.
The core doesn't write output files or execute discovery plugins automatically.

## Run the standalone proofs

Requires .NET 8 SDK, Python 3.12+, and the pinned formatter for C++ cache generation.
No NuGet packages or external NuGet feeds are required.

```sh
python3 -m pip install -r Tools/Quality/requirements.txt -r Tools/Metagen/requirements.txt
python3 Tools/Metagen/check.py
# After intentional source changes, review regenerated caches:
python3 Tools/Metagen/check.py --update

# The generic core and its tests have no ICE/profile dependency:
dotnet run --project Tools/Metagen/CoreTests

python3 Tools/Metagen/data_lab.py --all-codecs
```

`check.py` runs the core and C# consumer proofs, compares independent C#/Python scalar
and rich schemas and assembly/module container maps, checks reproducible generated caches
and runs Python tests. Outputs/provenance go under `build/metagen-output`.
`--csharp-json PATH` reuses all three adjacent reflected outputs (`csharp-schema.json`,
`csharp-data.json`, `container-maps.json`); it doesn't rebuild/retest the .NET providers.

The data lab resolves its example external URI through an explicit local resolver, loads
and validates a detached graph, materializes inheritance, and preserves the authored
prototype/operation data in JSON, XML and MDB1. It doesn't load ICE or contact a network.

## Container mapping and graph conformance

```csharp
[assembly: ContainerMap(typeof(List<>), ContainerShape.Sequence, TargetProfile.Cpp23,
    "playground/custom_vector.h", "playground::custom_vector<{0}>",
    "playground::vector_adapter")]
```

A mapping chooses target storage and an adapter. It doesn't change serialized meaning.
The equivalent Python convention is a literal module-level `CONTAINER_MAPS` collection.
Templates are parsed bounded type expressions; duplicates, missing arguments, malformed
includes and unknown profiles are errors. The generated C++ example selects `std::vector`
in C++20 and a deque-backed custom container with different operations in C++23. Both
serialize nested sequences to identical bytes through an explicit visitation/builder
contract. Truncation, item bounds and deliberate construction failure are exercised.
These are demonstration adapters, not a completed universal engine container library.

The richer portable-data **Python reference model and native ICE consumer** exercise:

- Closed int32 enums with stable symbol IDs; nested records; nullable values; recursive
  sequences, dynamic arrays, scalar sets and maps with canonical scalar keys.
- Inline/local/external occurrence policies, stable IDs, forward/shared references and
  ordinary reference cycles. External fetching is injected; no implicit I/O.
- Explicit document schema versions/dependencies; opt-in single-prototype inheritance;
  defaults, missing/null/reset/replace distinctions; ordinary prototype cycles rejected.
- Keyed sequence insert/update/remove/move, map put/update/remove and set add/remove.
  Duplicate identities, missing anchors/targets and identity-changing edits fail.
- Detached evaluation and owned snapshots; preserving authored operations separately
  from effective values. Named inline definitions cannot be silently forked by an
  occurrence-level merge: edit their definition or replace with a distinct identity.

The shared authored-tree profile bounds each input to 1 MiB of accounted tree data,
65,536 nodes, 48 levels and 4096 items per container, with 128 types/members and 16
resolved documents. Evaluation has a separate work bound. The native consumer additionally
bounds aggregate authored/effective/copy work to 262,144 nodes and 8 MiB in each category.
These are deterministic accounting budgets, not exact allocator usage or a hostile-code
sandbox. Diagnostic paths and full inherited-origin tracking still need expansion.
Native external references use ICE's URI parser/resolution, preserve queries and decode
fragments; the host supplies the document reader and any I/O/scheme policy.

A generated corpus contains 51 shared Python/C++ graph cases (13 accepted, 38 rejected).
Accepted graphs must agree byte for byte on effective values, typed references, canonical
container ordering and float-width conversion. Rejections cover operation conflicts,
reference policies, schema mismatches and prototype cycles, including cross-document
cases. Derived MDB1 fixtures are checked against the authoritative sources by `check.py`.

The design contract and remaining hard cases are in [DATA_MODEL.md](DATA_MODEL.md):
fixed extents, tuples/tagged unions, multimaps/multisets, richer key codecs, flags/open
enums, borrowed/immutable/custom container capabilities, dependency rebase conflicts,
lossless authoring syntax and typed native object/container materialization. These are not claimed implemented.

## Formats and compression

JSON rejects duplicate keys and nonfinite values. The typed-tree XML example rejects
DTD/entities, processing instructions, ambiguous shapes and duplicate entries. XML carries
JSON-escaped scalar/key text so control characters round-trip reversibly. It is a universal
example encoding, not yet the polished author-selected XML field/attribute layout.

MDB1 is a canonical tagged-tree binary proof with a schema fingerprint, explicit little
endian floats and bounded varints/containers. It preserves authored operations. It isn't
yet a schema-guided cooked format with stable field IDs, packed scalar blocks or random
access, and has not been benchmarked as an optimal shipping format.

The example's default codec set is none, raw DEFLATE, zlib, gzip, bzip2, XZ/LZMA2,
Zstandard, LZ4 frame, Brotli and Snappy framed streams, using pinned `cramjam` (MIT).
MCP1 envelopes carry explicit codec/lengths, encoded digest and decoded CRC. Decoding
writes into a fixed-size bounded output buffer and verifies size/checksums. Tests cover
all three formats through every codec, empty streams, truncation, corruption and lying
output lengths. Native codec workspace/CPU limits still require separate policies;
the output bound isn't an execution sandbox. ZIP/tar/7z entry handling belongs in an
archive/URI consumer. These codecs aren't yet linked into ICE or the Metagen core.

## Native runtime, RTTR and Rust

```sh
cmake -S Tools/Metagen/native -B build/metagen-native -DICE_CXX_STANDARD=23 -DICE_METAGEN_RTTR=ON
cmake --build build/metagen-native --parallel 4
ctest --test-dir build/metagen-native --output-on-failure
python3 Tools/Metagen/check_rust.py --native-build build/metagen-native
build/metagen-native/metagen_lab --demo
```

Use `-DICE_CXX_STANDARD=20` in another build folder to exercise that mode. The standalone
Rust proof requires Rust 1.90+ (standard library only); it never installs a toolchain.
An isolated locally installed proof toolchain under `build/metagen-rust-tools` is also
recognized. Normal engine/native builds require neither Rust, .NET nor Python.

RTTR is optional and outside the engine dependency graph. Its upstream revision is
`7edbd580cfad509a3253c733e70144e36f02ecd4`, archive SHA256
`bba4b6fac2349fa6badc701aad5e7afb87504a7089a867b1a7cbed08fb2f3a90` (MIT).
`FETCHCONTENT_SOURCE_DIR_RTTR` permits an offline verified source. `patch_rttr.py` applies
two checked compatibility changes to a private copy: move a default vector construction
past an incomplete type, and raise CMake minimum 3.0 to 3.5. Original sources stay intact.
RTTR builds as upstream C++17; consumers use the selected ICE standard.

The scalar runtime supports bounded ICM1 persistence/migration, generational object
handles, revision-checked atomic edit batches and bounded undo. The RTTR example proves
generated registration and reverse metadata consumption. The scalar headless lab supports
`inspect`, `set REVISION FIELD_ID VALUE`, `undo REVISION`, `step TICKS` and `quit`.

The separate `ice::data` consumer adds an owned dynamic graph and native MDB1 authored
persistence without graphics, CLR, Python, RTTR or implicit network I/O. A validated catalog
prepares a sealed candidate, resolving and validating the whole discovered graph before
activation. Publication checks the expected revision and candidate's owning store, stages
allocations first and swaps only on success. Eight prior graph states support undo.
Every activation/undo advances revision and generation; callers reacquire handles, while
retained immutable snapshots remain readable. Inspection returns owned copies. Foreign or
stale candidates/handles cannot modify active state. Ordinary allocation/reader exceptions
are outside the diagnostic contract. Live methods belong on the host thread; preparation
may run separately with a thread-safe reader, and immutable snapshots may be retained by
other threads. See `engine/include/ice/data/document.h`.

The native proof tests last-good state after failed preparation, stale revisions/handles,
undo/history bounds, snapshot ownership, all truncated input prefixes, malformed binary,
UTF-8 and numeric boundaries. It doesn't yet materialize arbitrary generated C++ graph
objects or activate gameplay/GPU resources. Native JSON/XML/compression readers, a frame-loop
inspector, Lua state transfer and MCP remain follow-up consumers.

The independent Rust generator emits scalar types plus checked ICM1 readers/writers,
and rich record/enum/container/typed object-reference declarations. The Rust proof reads C++
fixtures, validates/re-encodes identical bytes, rejects every truncated prefix and sends
output back to C++ for verification. Rich Rust declarations are compiled/exercised;
rich graph serialization/activation is a separate pending adapter.

## Next milestone

Keep the generic SDK reusable and the consumer examples rich. Ignition replaces
EngineTest as the next interactive development application; EngineTest remains a
regression fixture. Begin with its fast procedural GPU opening and typed human prompt,
then connect the proven detached graph service at a safe host boundary. Lua reload,
procedural scenes and the shared agent/MCP command adapter follow. The full direction is
in [PLAYGROUND.md](../../PLAYGROUND.md), including public-surface agent trials and asset
provenance. Richer typed adapters, cooked layouts and native URI/codec consumers should
be driven by those experiments. Public Metagen extraction follows useful standalone
examples, license/package review and independent CI.

## Validation

The standalone checks above exercise authoring conformance, generated caches, Python
formats/codecs, native data/RTTR/container/graph behavior and Rust byte agreement. The
repository CI matrix selects C++20 and C++23 on Mac and Linux. Windows cross-compilation
is useful maintenance evidence; runtime/graphics validation still requires Windows.
Neither mobile deployment nor native graphics changes are part of this data milestone.

This milestone was checked locally with:

- Mac engine regressions: 17 tests in each of C++20/C++23; standalone native/RTTR
  integration: five tests in each mode.
- Linux C++23 native/RTTR: five tests; C++20 address/undefined-behavior sanitizers:
  four tests, including the shared graph corpus and binary rejection checks.
- Windows graph/runtime and container proofs: strict cross-compilation in both modes;
  execution remains untested.
- Core-only .NET/custom-generator proofs, C#/Python schema and mapping conformance,
  reproducible caches, 34 Python tests and compiled Rust/C++ byte agreement.
- Formatting of 266 first-party C/C++ files, 1488 documented engine declarations with
  consistent argument names, and focused analysis of the five new graph translation units.
