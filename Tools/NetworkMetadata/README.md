# ICE networking metadata proof

This is an ICE consumer of Metagen's **raw `IMetaSource` interfaces**, with its own
custom C# attributes. Metagen core has no dependency on ICE networking. The three
demonstration declarations are transform, health and a weapon-fire event; equivalent
passive Python `ice.net.*` tags normalize through the same C# consumer. Python is
parsed by the pinned toolkit's constrained AST reader and is never imported/evaluated.
Production types should select one authoritative provider. These duplicate fixtures
test provider equivalence.

The proof generates typed C++20 records, policy descriptors, stable-ID field visitors,
an event validator adapter, model/editor discovery and a semantic projection. Native
full-state/delta encoding walks the **generated field visitors and descriptors**;
there is no separately maintained native field table. Explicit logical reference
metadata projects a scalar source string into a native room/epoch/spawn reference;
arbitrary strings are not interpreted as handles.

This is a codec/policy integration example, **not a networked room**. Authentication,
transport, relevance selection, durable receipts, baseline retention, mobile peers,
component/entity adapters and production multiplayer remain follow-up work. The
native namespace is deliberately `ice::network_proof` rather than a shipped engine
network API. Only scalar records and the declared five-field fire-event adapter are
supported; recursive/container/reference traversal needs another explicit profile.

## Build without source tooling

Ordinary ICE builds use checked-in text caches under `Generated`, with no CLR,
Python package, private submodule or network dependency. The standalone build is
also available (select either 20 or 23):

```sh
cmake -S Tools/NetworkMetadata/native -B build/network-proof23 -DICE_CXX_STANDARD=23
cmake --build build/network-proof23 --parallel 4
ctest --test-dir build/network-proof23 --output-on-failure
python3 Tools/NetworkMetadata/check.py --cached-only --native-executable build/network-proof23/network_metadata_tests
```

The last command uses Python's standard library and Git to check source/cache
hashes and independently encode all three examples. It compares **complete packet
bytes**, including framing and schema fingerprints, against the native executable.

## Explicit source-tooling proof

Requires the pinned `Tools/Metagen` checkout, installed .NET 8+ SDK reference packs,
Python standard library and clang-format **18.1.8**. No NuGet feeds, Python packages,
downloads or SDK installation are used. Build products go under `build`:

```sh
python3 Tools/NetworkMetadata/check.py
# After deliberate source changes:
python3 Tools/NetworkMetadata/check.py --update
```

The consumer compares both providers' policies and generated artifacts byte for byte.
It tests rejection of duplicate/zero IDs, conflicting typed attributes and tags,
unknown `ice.net.*` keys, unsupported codec versions, writers, audiences, ranges,
units, event grants and reference policies. Declaration/attribute order cannot select
policy. Presentation renames preserve wire fingerprints; semantic range changes
change them. Unknown unrelated custom attributes remain available through Metagen
and do not become replication policy. Invalid generation returns diagnostics with
zero artifacts. The C# tool stages a complete new output directory before renaming
it into place; updating reviewed caches is a separate explicit script operation.

The manifest records consumer/profile and formatter versions, pinned toolkit commit,
owned declaration/generator/runtime source hashes and generated artifact hashes.
The tool's temporary `providers.json` additionally records reflected assembly and
passive source identities; assembly hashes are not semantic wire identities and are
not used as a cross-machine reproducibility expectation.

## Experimental wire contract

All integers are little-endian with explicit widths. No C++ struct bytes or padding
are serialized. A packet is at most 65,536 bytes:

| Bytes | Meaning |
| --- | --- |
| 4 | `INW1` profile magic |
| 1 | Full state = 1, delta = 2 |
| 32 | SHA-256 semantic schema/policy fingerprint |
| 4 | Explicit type version |
| 8 | Baseline revision; zero for full state |
| 8 | New nonzero revision; greater than a delta baseline |
| 2 | Present field count |
| Per field | Field ID `uint32`, byte length `uint16`, exact scalar bytes |

Fields are strictly increasing by stable ID. Full state contains every field;
deltas contain only changed **encoded** values, so sub-quantization changes do not
create spurious traffic. Applying a delta requires an exact, validated baseline;
missing baselines fail explicitly. Unknown fields, wrong version/fingerprint,
duplicate/out-of-order/redundant delta fields, trailing/oversized/truncated data,
wrong lengths, nonfinite floats, negative-zero float encodings and invalid values
fail. Decode stages a typed candidate; failures preserve the output record/revision.
Encoding likewise preserves existing output bytes on validation failure. Allocation
failure or exceptions thrown by a host callback are outside this validation-result
contract.

`signed16_cm` version 1 normalizes input to float32 and encodes signed 16-bit
centimeters using nearest-even rounding, independently of the ambient rounding
mode. It never clamps/wraps. Its native code interval is -32,768..32,767; authored
transform ranges are deliberately -300..300 meters. Tests cover exact ties, signed
limits and at most half-centimeter error plus float32 conversion tolerance. Larger
worlds require a declared sector/local or wider codec, rather than changing this
codec silently. Health uses exact `uint16`; aim uses finite canonical float32.
This initial profile requires centimeter-grid range endpoints after float32
normalization, so a quantized value at a boundary remains valid as a delta baseline.
Fire events use full payloads; state deltas cannot encode an event.

## Host context and event semantics

Generated metadata describes authority-written components, relevance audience,
delivery, whole-field compare-and-set and presentation interpolation; codecs do not
implement delivery, relevance or interpolation. `authorize_write` proves the
authority/CAS decision independently of decoding: **decoding never grants a write**.

The fire adapter calls the host's current capability, room/epoch/reference/ownership
and cooldown validators, then dispatches on the simulation owner thread. Aim must
be finite, in range and unit length within 0.001 squared-length error. Identity and
grants come from an authenticated binding outside the event payload. A host must
bind one replay window to one authenticated principal and authority epoch and
serialize these calls; this proof does not authenticate a Boolean or payload claim.

The window retains 64 input sequences, accepts unseen out-of-order requests in that
window, rejects duplicates (including altered payload reuse), and rejects older
ones. It does not cache/replay event outcomes or promise durable exactly-once
delivery. Failed host validation/dispatch does not consume a sequence. `uint32`
rollover is forbidden; the host must establish a new authenticated input epoch and
reset before wrapping. A reset is a host lifecycle action, never a client request.
