# Metagen proof

The definition DLL is the source. The SDK reads it through reflection and exposes `IMetaSource`, `IMetaType`, `IMetaMember`, `IMetaAttribute` and `IMetaValue`. Consumers can use those interfaces without depending on reflection objects or a mandatory output format.

`Definitions` produces a DLL containing PaddleSettings and a custom attribute. `Smoke` consumes that DLL twice: once as a plain-text type description, once as INI defaults. It also verifies constructor and named attribute arguments are preserved. The sample attribute constructor deliberately throws, so successful reading proves it was not instantiated to inspect metadata.

Requires the installed .NET 8 SDK; there are no package dependencies. The local NuGet configuration clears external package feeds.

```sh
dotnet restore Tools/Metagen/Smoke/Smoke.csproj --configfile Tools/Metagen/NuGet.Config -p:NuGetAudit=false
dotnet run --project Tools/Metagen/Smoke/Smoke.csproj --no-restore -- ./build/metagen-output
```

This is an initial consumption API, not the full object model or a runtime sandbox. It currently describes exported types, declared public fields and non-indexed properties and their attributes. Methods, events, relationships, dependency/loading diagnostics and reloadable source lifetimes remain to be designed. DLL loading currently uses the default assembly load context. The native engine does not depend on this tool or on .NET.
