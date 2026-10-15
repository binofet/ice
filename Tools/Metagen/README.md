# Metagen proof

For the implemented C# provider, the definition DLL is the authoritative source. The SDK reads it through reflection and exposes `IMetaSource`, `IMetaType`, `IMetaMember`, `IMetaAttribute` and `IMetaValue`. Consumers can use those interfaces without depending on reflection objects or a mandatory output format.

## Proposed native integration

A consumer can generate native descriptors/code for serialization and bindings from
that same DLL. Those artifacts serve native runtime loading without requiring a CLR;
they do not become a second hand-maintained source of truth. This consumer and the
following reference/inheritance rules are not implemented yet.

Persisted/networked types and members need stable IDs. Object data should distinguish
inline definitions, local object-ID references and external URI references, with
attributes constraining allowed forms. Ownership/strong-versus-weak lifetime is a
separate concern. Local IDs should survive list reordering; external references can
use a document URI plus stable object-ID fragment. Allocate then fix up references
to allow forward references and ordinary cyclic object graphs.

Data inheritance should mean prototype plus explicit overrides, independently of C#
type inheritance. Start with one compatible prototype; reject prototype cycles.
Lists replace by default; opt-in merging needs stable element IDs/map keys and
explicit insertion/removal/reordering. Missing, null, default and removal must remain
distinct. Reference-policy and merge-policy attributes can make exceptions explicit.

Typed URI loaders can consume these descriptors to prepare scripts or resolve
container views. See [the URI contract](../../GLTF.md) for query, identity and caching
semantics. The current native URI layer reads bytes; it does not yet serialize object
graphs or execute prepared scripts.

`Definitions` produces a DLL containing PaddleSettings and a custom attribute. `Smoke` consumes that DLL twice: once as a plain-text type description, once as INI defaults. It also verifies constructor and named attribute arguments are preserved. The sample attribute constructor deliberately throws, so successful reading proves it was not instantiated to inspect metadata.

Requires the installed .NET 8 SDK; there are no package dependencies. The local NuGet configuration clears external package feeds.

```sh
dotnet restore Tools/Metagen/Smoke/Smoke.csproj --configfile Tools/Metagen/NuGet.Config -p:NuGetAudit=false
dotnet run --project Tools/Metagen/Smoke/Smoke.csproj --no-restore -- ./build/metagen-output
```

This is an initial consumption API, not the full object model or a runtime sandbox. It currently describes exported types, declared public fields and non-indexed properties and their attributes. Methods, events, relationships, dependency/loading diagnostics and reloadable source lifetimes remain to be designed. DLL loading currently uses the default assembly load context. The native engine does not depend on this tool or on .NET.

## Planned Python authoring provider

Python may also be authoritative for a schema. This is a new provider for the same
consumption SDK, not a requirement to emit a C# DLL or to maintain a second schema.
Choose the authoritative provider for each type; duplicate type identities in a
production schema set must be diagnosed rather than silently merged. Preserve the
existing DLL workflow for C# authors.

Python has the required building blocks: type annotations, `typing.Annotated`, class
decorators, enums, and `dataclasses.field(metadata=...)`. Metagen defines how to
interpret them and validates instances; Python annotations do not enforce runtime
types. Attribute-like metadata should be typed, namespaced values with equivalent
meaning across providers. Unknown metadata can be preserved for specialized consumers.
See [Annotated](https://docs.python.org/3/library/typing.html#typing.Annotated),
[dataclass field metadata](https://docs.python.org/3/library/dataclasses.html#dataclasses.field),
and [C# attribute metadata](https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/language-specification/attributes).

The shared model needs canonical type descriptors rather than interpreting CLR type
names or Python runtime classes in every consumer. Extend the current string-based
`ValueType` proof with explicit kinds for signed/unsigned integers and widths, float32
and float64, booleans, UTF-8 text, bytes, enums, records, lists, maps, optional values,
and object references. Python `int` does not select an integer width and Python
`float` does not mean C# `float`; schema declarations must make these choices explicit.
Do not infer native or shader layout from either language's object layout.

Normalize stable IDs, literal defaults, required versus optional fields, nullability,
units/ranges, inheritance, reference policies, and container policies in the reader.
Missing, explicit null, default, and removal remain distinct. A callable Python default
factory is executable behavior, not a portable literal default; reject it in the
portable schema profile or require an explicitly identified extension. CLR properties,
Python descriptors, and constructors must not be invoked merely to discover defaults.
Language-specific methods and arbitrary objects are outside the initial portable
profile; richer consumers can retain provider-specific information without claiming
portable semantics.

Importing Python modules or resolving annotations may execute code. Prefer a constrained
AST reader for the declarative profile, with allowed syntax/import aliases and useful
unsupported-expression diagnostics. If trusted project definitions require executable
schema construction, load them in a disposable worker with explicit limits; isolation
alone is not a security sandbox. Do not promise passive inspection of arbitrary Python
code. The existing C# reader avoids attribute constructors, but assembly loading also
has a different trust boundary from a fully isolated metadata parser.

First proof: author the same PaddleSettings test schema independently in C# and Python,
then compare canonical IDs, types, metadata, and literal defaults. Run the same native
serializer/generator and validation fixtures against either provider, including integer
overflow, missing/null values, reference-policy violations, and a version migration.
The Python provider and native generator are not implemented yet.

A worker result or optional serialized snapshot is a derived projection of a specific
source revision and provider version. It is not another hand-maintained source of truth;
consumers continue to use the shared model or visitors. Schema authoring language and
embedded gameplay language are independent choices.
