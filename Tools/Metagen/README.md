# Metagen proof

The definition DLL is the source. The SDK reads it through reflection and exposes `IMetaSource`, `IMetaType`, `IMetaMember`, `IMetaAttribute` and `IMetaValue`. Consumers can use those interfaces without depending on reflection objects or a mandatory output format.

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
