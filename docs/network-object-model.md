# Networking metadata and engine boundaries

The [networking metadata proof](../tools/networkmetadata/README.md) is an isolated
ICE consumer of Metagen. Custom C# attributes and passive Python tags generate
equivalent typed records, descriptors, field visitors, full/delta codecs and explicit
host event-validator hooks. The native namespace is `ice::network_proof`; this is
an experimental codec/policy profile, not a shipped networking service.

## What the example establishes

The checked-in generated caches support ordinary native builds without .NET,
Python packages or a private SDK checkout. Explicit regeneration compares the two
providers and records source/cache provenance. The example's own
[build and wire-contract documentation](../tools/networkmetadata/README.md)
defines the supported records, framing, limits and negative cases.

Decoding does not authorize an operation. Host callbacks own current capabilities,
reference lifetimes, ownership and event dispatch. A replay window belongs to an
authenticated principal and authority epoch supplied by the host; a payload cannot
authenticate its own identity. Codecs do not implement delivery, relevance,
interpolation or durable exactly-once execution.

## Separate runtime responsibilities

A networking runtime needs explicit authority and transport contracts beyond this
example. Keep authored data, local generation-checked handles and wire identities
separate. Stable type/field identities and schema versions must survive presentation
renames. A native object pointer or C++ memory layout is not a wire format.

Validate a complete candidate before publishing it, retain the previous state on
failure and declare parsing/allocation budgets. Simulation ticks, presentation
frames, authored revisions and transport sequences have different meanings.
Resource and owner retirement must invalidate stale references before later work
can invoke them.

Transport security, authenticated admission, component/entity adapters, relevance,
durable receipts, baseline retention and clients remain outside the metadata proof.
Do not infer those capabilities from generated descriptors or one codec test. Add
each through its own bounded implementation and conformance evidence.

## Related public contracts

- [Object composition](object-composition.md) describes proposed component ownership
  and prepared edits; it is not an implemented general component runtime.
- [Commands](commands.md) and [scripting](scripting.md) document the current headless
  invocation and bounded Lua interfaces.
- [Native metadata integration](../tools/metagenintegration/README.md) documents
  generated schema consumers and owned-data round trips.

Networking implementations and host products choose their own application schemas,
protocol profiles and acceptance cases. Those choices are not required by this
engine example.
