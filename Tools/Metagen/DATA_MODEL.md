# Metagen data contract: review draft and conformance direction

This document records the intended contract. Implemented proof coverage is described
in README.md; a design below does not imply that every native adapter already exists.
Native Python/C++ conformance and detached graph activation now exercise the initial
profile. Finish each resource/lifecycle contract before connecting it to Ignition.

The **core SDK** owns basic optional attributes, generic source/type/member/attribute
interfaces, metadata readers, diagnostics and optional generator composition. All of
the persistence semantics below belong to **independent consumers/profiles**, including
the demonstration profile. They aren't requirements imposed on other projects.
An arbitrary documentation/shader/binding generator can use custom attributes and the
core alone, without loading any serialization profile or depending on ICE.

## Meaning, storage, and representation

A source DLL remains authoritative for C# definitions. A Python module is authoritative
for Python definitions. The reader exposes recursive type expressions and source-level
attributes; consumers select a portable profile when they need serialization or bindings.
An emitted profile is a disposable projection, not a required second definition language.

Keep three independent decisions:

1. Logical type and semantics: sequence, fixed array, set, map, tuple, record, enum,
   optional, tagged union, object occurrence/reference; element/key/value types and constraints.
2. Native realization: allocator, vector/map implementation, small-buffer capacity,
   handles, strings, factories and immutable/frozen containers. An adapter bridges these.
3. Representation: human field names in JSON/XML, stable IDs and compact encodings in
   cooked binary, plus an independent compression envelope.

Changing a vector implementation must not change a save-file fingerprint. Changing
ordering, uniqueness, key equivalence or serialized element type *does* change semantics.
Do not expose native pointers, allocator state, padding, capacities, iterator state or
hash bucket layout in persistent data. Borrowed spans/views are not owning load targets;
they require a lifetime-bearing buffer adapter, or a generated owned staging type.

## Container mappings

C# supports assembly attributes with unbound generic type tokens:

```csharp
[assembly: ContainerMap(typeof(List<>), ContainerShape.Sequence, TargetProfile.Cpp23,
    "game/small_vector.h", "game::small_vector<{0}>", "game::small_vector_adapter")]
```

The real type is `System.Collections.Generic.List<T>`, and `typeof(List<>)` supplies
the open generic definition. C# attributes cannot use `List<T>` with a free parameter.
See Microsoft's [generic attribute documentation](https://learn.microsoft.com/en-us/dotnet/csharp/advanced-topics/reflection-and-attributes/generics-and-attributes).
Mappings use a parsed type-expression template, not arbitrary generated source text.
Nested expressions are substituted recursively. Each source/target pair is unique;
duplicate mappings are errors. C++20 and C++23 are explicit profiles. No implicit mapping
fallback may silently change storage when the CMake standard changes.

Python has module scope rather than an assembly attribute target. A declarative,
literal module-level `CONTAINER_MAPS` collection serves the same purpose. A package or
project may explicitly aggregate such maps. Field overrides should select a named
mapping, not secretly change the logical shape. Future project-level selection must
report provenance and conflicts; do not implement silent last-file-wins registration.

An adapter is code with a checked contract, not a type spelling. Sequence adapters
provide ordered read visitation, bounded detached construction, optional reserve, checked
append and finalization. Maps also provide canonical key conversion, duplicate detection
and checked insert; sets provide uniqueness checks. Immutable containers use builders.
Fixed arrays enforce extent. Custom constructors and allocation failures leave the active
object untouched. Generators diagnose missing capabilities; no guessed `push_back` or
`operator[]` calls. Iterators/pointers never escape the visit. Serializers work against
adapters or a generated typed visitor, not implementation names or RTTI.

Nested containers, optional containers versus containers of optional values, empty versus
missing versus null, composite keys, duplicate source entries, deterministic iteration,
and partial construction failure belong in the shared conformance corpus. A map is a
sequence of key/value pairs in the universal model: JSON object spelling is allowed only
with an injective reversible string-key encoding. `1` and `"1"` must never collide.
The first portable profile permits string/integer/enum keys; richer immutable key records
need an explicit canonical-key codec. Floating keys and locale-dependent comparers fail.
Multimaps and multisets are distinct kinds, not maps/sets with duplicate checks disabled.

## Enums and records

Every named type, field and enum symbol has a stable identity; names are presentation.
Integer enum values and symbol IDs are different. Underlying width/signedness are explicit.
Closed enums reject unknown values; extensible enums retain unknown numeric values;
flags declare their mask/unknown-bit policy. Do not guess flags from power-of-two values.
Nested records are values by default and may contain recursive type expressions. Cyclic
type declarations are allowed through references or finite containers; an unconditional
by-value native-layout cycle is diagnosed by the target generator.

## Objects and references

An occurrence may be inline, a local reference to a stable object ID, or an external URI
reference. A field attribute constrains permitted representations, independently of
nullability and independently of runtime ownership. Container occurrence policy reaches
object leaves, not unrelated records used as map keys. A future per-type-expression-path
policy can distinguish nested leaves; reject ambiguous blanket policies rather than guess.

Local IDs are document-scoped stable text identities, never array indices or addresses.
External identity is a resolved document URI plus an object fragment. Normalize through
the URI handler, retain query strings, cache by the complete resolved identity, and retain
the authored relative spelling for source editing. Inline objects can carry IDs when
they need identity; two definitions of one ID are an error. A loader allocates identities,
validates all values, then fixes references and publishes atomically. Forward references,
sharing and ordinary reference cycles are valid; strong ownership cycles are a separate
lifetime question. Required missing targets fail; weak/soft references require an explicit
policy. No implicit pointer ownership inference from C#/Python object reachability.

External loading uses an injected resolver with depth, object, byte and URI-scheme budgets.
The model doesn't perform network I/O or arbitrary imports. In-flight document caching
allows cross-document reference cycles; prototype cycles still fail. Failed batches don't
replace last-good data. Runtime references are checked handles with generations; serialized
IDs are not runtime handles. A Rust projection should use arena IDs/handles too: arbitrary
graphs aren't trees of Rust references or recursively owned `Box<T>` values.

## Data inheritance and container edits

Prototype inheritance is opt-in and independent of language class inheritance. Start
with one explicitly referenced prototype per object. Require the same schema identity
and version, resolve recursively, detect cycles, apply operations to a detached candidate,
then validate the complete result. Multiple prototypes require a future explicit conflict
model; neither C# inheritance nor file order is a substitute.

Preserve authored operations/prototype URI separately from effective data. Inspectors
need field/element origin and inherited/local state; save should preserve the author's
intent. A cooked snapshot may deliberately flatten it. Missing means inherit (or use a
declared default without a prototype); explicit null means null; reset means use the
declared default; container remove means delete an identified entry. These are separate
tagged operations. An explicit value equal to a default is still an explicit override.

Records can opt into recursive field merging. Sequences replace by default. Opt-in
keyed sequences declare a nonnullable immutable scalar/enum identity field on each
element and admit ordered operations:

- insert a new ID before/after an existing ID, or append;
- update a present ID with field overrides, keeping its identity;
- remove a present ID;
- move a present ID before/after an existing ID, or to the end.

Operations evaluate in authored order against the current candidate. An unknown anchor,
duplicate insertion, missing removal/update, self-anchor or identity mutation is an error.
No positional inheritance patches: inserting into a base must not retarget an override.
Replace is a complete opt-out from merging at a particular occurrence. Map merging uses
typed canonical keys and explicit put/update/remove; a missing map key removal is an
error. Sets use explicit add/remove with uniqueness checks. A removed inherited element
is a tombstone in the authored operation stream; do not discard it by saving only the
effective container. Whole-list replacement remains valid for reordering when no stable
element identity exists. Bounds and uniqueness apply after every complete transaction.

For base revisions, record dependency fingerprints and report conflicts rather than
silently resurrect missing entries. The initial model rejects conflicting authored
operations. Three-way rebase/conflict UI is separate future work, not a hidden list heuristic.

## Formats, compression and performance

JSON and XML carry the same explicit object/reference/operation semantics. Reserved
control keys/elements never collide with ordinary user fields. Reject duplicate keys,
DTD/entity expansion, ambiguous map encodings, nonfinite numbers and unknown control
operations. Round trips preserve *semantic authored form*; comments, whitespace and
attribute order need a separate lossless editor syntax tree and are not promised here.

Cooked binary uses a versioned envelope, schema fingerprint, stable type/field identities,
explicit widths and endian rules, bounded lengths, and typed contiguous scalar blocks
when useful. No format is universally optimal: benchmark decoded allocation count,
load latency, file size, random access and cross-language cost on real ICE scenes before
claiming a winner. Keep authored-operation storage and flattened runtime snapshots as
explicit modes. A new native-memory layout must not invalidate the format.

Compression is orthogonal: default distribution should cover none, raw DEFLATE, zlib,
gzip, bzip2, XZ/LZMA2, Zstandard, LZ4 frame, Brotli and Snappy framed data through a codec
registry. Codec IDs, framing, versions, output budgets, checksums and concatenation policy
are explicit. ZIP, tar and 7z are archives with entry lookup/path policies, not synonyms
for compression; handle those through URI/container handlers. Do not silently fall back
to another codec. Default codecs require pinned, licensed dependencies; a minimal build
may explicitly disable codecs and must reject unsupported encoded input. Compression
dictionaries are assets with their own identities. Streaming and chunking should retain
random access rather than compressing every asset into one opaque block.

## C++ and Rust consumers, then extraction

Support selected C++20/C++23 through CMake. C++23 doesn't provide standardized general
static reflection; Metagen is still useful. Runtime APIs must support both modes with
one result abstraction and document target library requirements. RTTR remains an optional
consumer/example, not the schema definition or engine-wide runtime dependency.

Generate Rust types and a real binary reader/writer from the same source projection.
Prove cross-language bytes, invalid-input rejection and container order; successful
declarations alone don't prove serialization. Use explicit widths, `Option`, `Vec`,
`BTreeMap` for deterministic maps, tagged enums and typed handles. Serde integration is
a useful consumer, but Serde alone doesn't define object identity/prototypes/migrations.
Rust is attractive for isolated loaders, asset tooling or script/agent workers; keep C++
engine rendering/platform work moving. A whole-runtime rewrite needs measured benefits
and an FFI/lifetime plan, not just an additional generator.

Extract Metagen into its own public repository after these conformance proofs have
useful examples. Keep provider/consumer tests headless and independent of ICE; engine
adapters consume released contracts. Audit license, packaging, generated-cache policy,
schema evolution, diagnostics and CI before publishing. Publication is a separate milestone.
