# Networked objects and live worlds

Design checkpoint: 2026-10-15. This defines the next implementation milestone after
the Lua authoring bridge. Shared-room replication, durable prototype catalogs,
network attributes and persistent gameplay scripts are **not implemented** by this
document. The existing companion transport supports a particular party-game input
protocol; Ignition's command socket is private loopback control. Neither is a
general replicated world.

The first topology is one authoritative simulation with several independently
rendered clients. The authority can run inside a desktop application or a headless
server. A standalone world uses the same object and edit contracts with a local
authority. Quest/mobile clients, authoring agents and a later browser client are
peers with different declared capabilities. A headset's display rate and tracking
coordinate system do not determine the simulation clock or authority.

## Identity and composition

Introduce a portable entity/component document before persisting Ignition's current
`scene_object`. Its shape enum, tint and `body3d_description` are a temporary
application recipe. They must not become the permanent prototype or wire ABI.
Start with transform, renderable, material and rigid-body components, each with a
stable type identity and field IDs. Component adapters construct native resources
from validated owned data. Behaviors, triggers, weapons and attachments extend
that contract without changing every entity into a large base class.

| Identity | Meaning and lifetime |
| --- | --- |
| Prototype UUID | Stable catalog identity; a rename or new revision retains it. |
| Content revision | SHA-256 of a specified canonical manifest, including exact component data, schema identities and dependency hashes. Immutable. |
| Document object/component ID | Stable identity within an owned authored graph; component type alone is insufficient when several instances of that type are allowed. |
| Room UUID and authority epoch | Room history plus the current authoritative lifetime. A restarted/replaced authority fences earlier epochs. |
| Entity ID | Room/epoch plus an authority-assigned, non-reused 64-bit spawn serial. Compact serials may be used after the connection binds room/epoch. |
| Player ID | Authority-assigned participant identity, separate from an account principal, device, transport connection or entity. Reconnect can rebind it. |
| Local handle | Process/store/world owner, slot and generation. Used for checked native access only. |

The current `object:42` can remain a convenient room-local display alias. Logs and
cross-peer references include its room/epoch; a later process cannot infer what
bare `42` meant. Use strings or exact integer codecs at JavaScript boundaries.
Never replicate physics `body_id`, native pointers, Lua tables, GPU owners or
`data::graph_handle` as entity identity. Graph activation invalidates graph-handle
generations; that must not retire every live entity during an unrelated edit.

An instance records its prototype UUID and pinned revision, graph-local node
mapping and authored overrides. Inline definitions copy as values. Internal graph
references remap when instantiating/copying a graph; explicit external asset
references remain pinned. A live entity reference is a different tagged type from
a persistent asset reference. It declares required/optional resolution and behavior
when its target despawns. A reference never grants control of the target.

Reuse `data::catalog`, `graph_store::prepare` and existing inline/local/external
reference and inheritance semantics for portable documents. Keep original authored
overrides and dependency fingerprints alongside effective values. The graph store
prepares data; entity adapters separately prepare components and native resources
before publication. Schema migrations are explicit and cannot silently change an
active replicated type.

## Authority, ownership and generated policy

Simulation authority decides gameplay state. Ownership associates entities and
input rights with a player. Holding an object, owning a weapon or rendering a
predicted body does not transfer simulation authority. Editing permissions are a
separate room role. An authoring agent acts for an authenticated principal with
the room's grants, rather than becoming a privileged server because it can write
Lua. Existing command caller IDs are selection namespaces, not credentials.

The networking projection is an ICE consumer of Metagen's generic attribute model.
It can provide C# attributes and Python metadata for these proposed policies:

- Component creation/removal authority, required dependencies and schema version.
- Field writer (`authority`, validated player input, or local presentation),
  recipient visibility, persistence and relevance rules.
- Snapshot/event delivery, quantization, interpolation and optional prediction
  adapter. Units/ranges and acceptable error belong to the descriptor.
- RPC/event direction, argument schema, caller capability, rate/cooldown policy,
  retry identity and whether the result survives reconnect.
- Container identity, conflict policy and generated reference traversal.

Generate serializers, validators, replication descriptors, dependency walkers and
model/editor discovery from these definitions. Never serialize C++ layout or
infer replication from a public member. Reject unsupported policy combinations at
generation/preparation. A field annotated for owner input still requires server
validation; client-supplied ownership metadata has no authority.

Whole-field compare-and-set is the initial authoring conflict rule. Opt-in keyed
containers use stable element keys and explicit insert/update/remove operations,
matching the data profile; array positions do not identify inventory entries.
Independent component edits can use separate expected revisions while one accepted
edit receives a room-wide commit sequence. A universal CRDT would give incorrect
meaning to many physics, inventory and weapon edits. Text/presentation or another
game-defined field can select a CRDT only with its own tested merge contract.

Negotiate protocol major/minor, schema fingerprints and supported field codecs
before decoding a component. Encode explicit widths, endianness and lengths with
stable type/field IDs; never use a native struct dump or silently reinterpret an
unknown version. JSON is a useful diagnostic representation, while the real-time
codec can use compact generated binary. The current command service's flat scalar
argument profile is not the full entity graph codec. Authoring commands can pass a
pinned document URI; tick input/state uses its own typed protocol rather than one
MCP call or JSON command per field per frame.

### Metagen extension and the first generated proof

Networking attributes are a first-class authoring feature, owned by an **ICE
networking metadata package**. Keep Metagen's core generic: another project can
still reflect its own attributes and generate an infographic without adopting an
ICE object or replication model. C# assemblies/Python declarations remain the
authoritative inputs; a network projection is generated consumer data with a
specific purpose, not a second hand-maintained schema.

The existing extension points are sufficient for the first scalar proof:

| Existing surface | Network consumer use |
| --- | --- |
| `IMetaSource` / `IMetaType` / `IMetaMember` / `IMetaAttribute` | Read stable schema/field IDs, recursive type expressions and arbitrary custom attribute values. |
| `ReflectionMetaSource.Load` | Read C# `CustomAttributeData`, including custom/assembly attributes, without invoking their constructors. Assembly loading itself is not a sandbox. |
| `IMetaGenerator.Generate` / `Generation.Run` | Produce owned source/JSON artifacts and structured diagnostics. Errors suppress artifact publication; callers stage filesystem writes. |
| `Python/metagen_source.py` | Read constrained passive scalar declarations with literal type/field `tags`; do not import/evaluate Python. |
| `ProjectionMetaSource.Load` | Turn that scalar projection's tags into generic `MetaTag` attributes so the same network consumer can read both providers. |

Do not infer that arbitrary Python decorators already work: they are rejected.
`portable_schema.py` validates the scalar profile and preserves its bounded string
tags, but it is not a network generator. The rich `Python/data_source.py` currently
rejects extra network/tag metadata; `DataSchema.Read` does not carry arbitrary
attributes into its rich output, and `PortableSchema.Read` carries known tags
rather than arbitrary custom attribute types. Read networking policy from the raw
`IMetaSource` before those projections discard it, then join projections by stable
type/field IDs. Rich Python policy needs a tested passive metadata extension;
do not silently claim it is already supported or copy/fork the entire parser.

Proposed C# spelling, **not attributes currently shipped by ICE**:

```csharp
using Metagen;
using Metagen.PortableData;
using Ice.NetworkMetadata; // Proposed ICE-owned attribute assembly.

[MetaSchema("demo.transform", 1, "transform")]
[Replicated("authority", Audience = "relevant")]
public sealed class Transform {
    [MetaField(1, "x"), MetaRange(-300, 300)]
    [WireCodec("signed16_cm", Unit = "m", Interpolation = "linear")]
    public float X { get; set; }
    [MetaField(2, "y"), MetaRange(-300, 300)]
    [WireCodec("signed16_cm", Unit = "m", Interpolation = "linear")]
    public float Y { get; set; }
    [MetaField(3, "z"), MetaRange(-300, 300)]
    [WireCodec("signed16_cm", Unit = "m", Interpolation = "linear")]
    public float Z { get; set; }
}
```

The custom attribute classes expose typed constructor/named arguments; the ICE
consumer normalizes them to its network policy IR. Equivalent Python declarations
can already express the policy as literal namespaced scalar tags, without adding
an executable decorator. This syntax is accepted by the existing passive reader;
the **meaning of the `ice.net.*` tags remains proposed**:

```python
from dataclasses import dataclass, field
from typing import Annotated
from metagen import schema, f32

@schema("demo.transform", version=1, name="transform",
        tags={"ice.net.writer": "authority", "ice.net.audience": "relevant"})
@dataclass
class Transform:
    x: Annotated[float, f32] = field(metadata={"id": 1, "range": [-300, 300],
        "tags": {"ice.net.codec": "signed16_cm", "ice.net.unit": "m",
                 "ice.net.interpolation": "linear"}})
    y: Annotated[float, f32] = field(metadata={"id": 2, "range": [-300, 300],
        "tags": {"ice.net.codec": "signed16_cm", "ice.net.unit": "m",
                 "ice.net.interpolation": "linear"}})
    z: Annotated[float, f32] = field(metadata={"id": 3, "range": [-300, 300],
        "tags": {"ice.net.codec": "signed16_cm", "ice.net.unit": "m",
                 "ice.net.interpolation": "linear"}})
```

Typed C# attributes and Python tags normalize to one validated language-neutral
policy for each field: stable type ID/version, field ID, logical type/range,
writer, audience, codec/version, unit and interpolation. Reject conflicting typed
attributes/tags, unknown `ice.net.*` keys, duplicate IDs and invalid combinations.
Do not let attribute iteration order decide policy. Wire fingerprints cover
semantic IDs/types/versions/codecs/policies, separately from presentation names and
source-file/assembly provenance hashes. Renaming a display/member name with the
same ID must not reorder fields or silently change encoded meaning.

Start with exactly these three definitions before a broad attribute library:

| Stable type ID | Fields and initial policy |
| --- | --- |
| `demo.transform`, version 1 | IDs 1/2/3: x/y/z float32 meters in -300..300, authority-written, relevance-filtered, linear presentation interpolation, `signed16_cm` codec. |
| `demo.health`, version 1 | ID 1: points uint32 in 0..65535, authority-written, reliable change notification, exact uint16 codec, whole-field compare-and-set for authoring. |
| `demo.fire_event`, version 1 | ID 1: weapon entity reference; ID 2: uint32 input sequence; IDs 3/4/5: finite float32 aim x/y/z in -1..1. Client-to-authority, `weapon.fire` grant, reliable event delivery and explicit deduplication. |

The proposed event attribute can be
`[NetworkEvent("client_to_authority", RequiredCapability = "weapon.fire")]`,
with `[EntityReference("owned_weapon")]` on its weapon field. Python uses
equivalent namespaced tags. Principal/player identity comes from the authenticated
transport, not an authored event argument. Reference resolution, weapon ownership,
current room/epoch, aim-vector validity and cooldown require host context; emit
calls to declared validators instead of pretending field ranges prove gameplay
legitimacy. A uint32 sequence is a deliberately small proof profile: use an
explicit sequence-window/epoch rollover rule, and add uint64 support before
claiming it is a permanent room-counter representation. The existing scalar
authoring profile has no uint64 kind or first-class entity-reference type; the
network consumer needs an explicit typed reference projection rather than treating
every string as a handle.

`signed16_cm` is a registered, versioned test codec: signed 16-bit centimeters,
little-endian, round-to-nearest with ties-to-even, no clamping or wraparound.
Derive the representable interval and validate after float32 normalization; test
the boundary rounding cases explicitly. Quantization changes network precision,
not authored units/native storage. The test expects at most half a centimeter of
quantization error, plus the stated float32 conversion tolerance. Larger worlds
need another declared codec, such as sector origin plus local offset.

The first generator consumes that normalized policy once and emits:

1. `network.types.generated.h`: ordinary typed C++ records and stable IDs, or reuse
   an existing generated native record instead of defining it twice.
2. `network.generated.h/.cpp`: replication descriptors, full-state read/write,
   delta diff/apply and event validation/dispatch adapters. Declaration traversal
   generates all field lists; no manually synchronized serializer tables.
3. `network.discovery.generated.json`: domains, field policies, codecs, event
   permissions and host-context requirements for models/editors.
4. A reproducibility manifest: input identities, generator/profile versions and
   semantic fingerprints. A canonical network projection is useful as a derived
   inspection/interop artifact when several generators consume it.

The runtime tests compile the same generated sources in C++20 and C++23. C#/Python
provider fixtures must produce the same normalized policies and wire bytes. Prove
full-state round trips, an empty delta, one changed field, exact health values,
quantization edges, unknown/wrong schemas, truncated/noncanonical/oversized input
and rejection without partial output publication. Test missing delta baselines,
unauthorized health writes, stale weapon references, wrong ownership, duplicate
fire events, missing required capabilities and altered operation reuse. Rename a
field while retaining its ID and verify unchanged wire meaning. Add a field only
through an explicit negotiated version/default rule. Generated discovery must
match the actual validators and descriptors.

Next extend the same consumer to nested records, keyed sequences/maps/sets and
typed references. The existing data merge policy and network conflict/delta
policy are related but distinct: a keyed inventory needs stable element IDs,
authored insert/update/remove semantics, runtime ownership validation and a tested
replication delta. Quantization applies only to declared numeric leaves; reference
traversal supplies dependency/relevance rules. Unsupported rich policies must
diagnose, not fall back to serializing a native container's bytes. Rust or another
network codec generator can later consume the same policy without adding ICE
semantics to Metagen core.

## Script domains and discovery

Domain is part of a script's manifest, not a convention in a function name:

| Domain | Allowed effects |
| --- | --- |
| `authoring` | Prepare a coherent world/prototype edit under the caller's edit grants. |
| `authority` | Tick/event gameplay, spawning, validated damage, modifiers and ownership changes. |
| `predicted` | A declared prediction adapter's temporary local state, reconciled to authority. |
| `presentation` | Local visuals, sound and UI; cannot write replicated gameplay. |
| `shared_pure` | Explicit pure functions and inputs; no ambient time, I/O or implicit determinism claim. |

The current Lua bridge implements one-shot `authoring` only. Persistent event/tick
dispatch, script state schemas and domain grants are a following implementation.
`host.invoke` is not a universal capability in every domain. An authority behavior
will receive typed gameplay bindings; a presentation script will receive different
bindings. Resolve each callback against the current grant set, never a retained
pointer from an earlier call. Native events are copied, then dispatched outside
physics/network callbacks at the simulation boundary.

Model inference prepares content asynchronously; it is not in the simulation tick
or on the network receive path. Ordinary moment-to-moment gameplay runs in scripts
or native systems after publication. This keeps a room playable during model
latency, disconnection or a failed generation. Measured critical script work can
gain a native implementation behind the same component/binding contract.

Discovery must publish network mode, authority, local roles, available domains,
component/binding schemas, exact versions/hashes, active experiment profile and
permissions. It should describe missing features as missing. A model can then
request a new engine capability instead of inventing an RPC or executing the same
spawn loop on every client. Capability changes advance a catalog revision; script
activation checks the revision it was prepared against. The truthful standalone
example and model workflow are in Ignition's `docs/MULTIPLAYER.md`.

## Tick state, durable edits and messages

Use a fixed authoritative simulation tick, initially 60 Hz and configurable per
room. Render interpolation and XR tracking run independently. Simulation tick,
authoring component revisions, durable commit sequence and transport sequence are
different values. Physics motion does not invalidate an author's component edit
every frame. Jolt/Box2D currently promise no cross-platform bit-exact simulation;
replicate authoritative results rather than relying on deterministic lockstep.

| Lane | Contract |
| --- | --- |
| Control/edits | Reliable, ordered join/permissions, edit commits, entity/component creation/removal and activation manifests. |
| Input | Sequenced input with tick/sequence and small redundancy for loss. Authority acknowledges the processed input sequence and validates ownership/time/ranges. |
| State | Replaceable snapshots/deltas with authority epoch, tick, baseline and schema identity. Old updates do not overwrite newer state. |
| Assets | Reliable independent chunks, content hashes and resumable transfer; bandwidth cannot starve input/control. |
| Media | Separately negotiated voice/audio streams and permissions; never implicit fields on every entity. |

The first client interpolates remote dynamic objects. Add prediction first for a
specific player-controller adapter, then reconcile by restoring its authoritative
state and replaying unacknowledged inputs. Do not claim that replaying arbitrary
Jolt/Lua world state is deterministic. Locally predicted shots can show immediate
cosmetics using a request token; the accepted spawn maps that token to a real
entity ID. Failed/rejected predictions retire cleanly.

Delta encoding references an acknowledged per-client baseline. If it is missing,
send a full snapshot instead of applying a delta to whatever state happens to be
present. Quantization is a per-field codec with declared bounds; out-of-range data
must take a documented escape path or reject. Spatial relevance and client
subscriptions determine which entities/fields each peer receives. Relevance exit
is not authoritative destruction. References to an entity outside the interest
set remain unresolved typed identities or cause an explicit dependency inclusion.

An edit request carries principal binding, operation ID, base read-set revisions,
catalog/schema revision and owned proposed changes or script content revision.
Preparation reads an immutable candidate; commit revalidates its read set and
permissions at the chosen tick. Ordinary rejection leaves the active state intact.
Conflicting authors receive structured conflict data; unrelated edits can proceed.
Executing a command list in transport order does not itself make it atomic.

Durable edits need a room journal above `commands::service`: its existing 64-entry
process-local receipt cache is insufficient for reconnect or crash recovery. Store
operation ID, canonical request digest, outcome, commit sequence and assigned IDs.
An exact retry returns the recorded outcome; changed reuse rejects. Couple the
accepted edit and receipt in one journal transaction, then publish. Recovery replays
only committed edits against a checkpoint. Simulation-only spawn receipts can use
an explicitly advertised replay horizon; beyond it, return `outcome_unknown` and
inspect rather than blindly repeat a fire command. The protocol must not promise
unlimited exactly-once delivery.

All fallible native construction precedes the durable commit; active publication
then uses a prepared ownership swap. A crash after journal commit but before that
swap recovers the committed candidate by replay. If a subsystem cannot provide
that publication contract, its edit is explicitly non-atomic/inspection-required
instead of returning an ordinary failure that pretends nothing changed.

## Persistent prototypes and dependency closure

“Make that a prototype” captures an owned component graph, not native memory. Give
it a stable UUID and an immutable content revision. Record exact schema versions,
authored inheritance/overrides, referenced prototype revisions, script source and
inputs, model/texture/audio blobs, provenance and the complete dependency closure.
Resolve mutable URI aliases at publication and pin their content. URI queries
remain meaningful, but an executable URI alone cannot reproduce generated output.
Record the generator revision, parameters/seed and resulting bytes as well.

Persist an immutable manifest and blobs in a local content-addressed catalog;
human-readable JSON is useful for review, and a cooked data encoding is a separate
representation. A small rebuildable index maps UUID/name/tags to revisions. Catalog
root and cloud providers are explicit URI mounts. No cloud account is required to
retain or instantiate locally saved objects.

Write temporary files in the destination filesystem, verify hashes and complete
dependency availability, flush through the platform's durability API, then atomically
publish the immutable paths and update the index/journal. Existing hash paths are
verified or reused, never overwritten with different bytes. Persist the catalog
revision **before** removing the live source entity. A failed room commit may leave
an unreferenced saved revision while the object remains; that is recoverable. It
must never leave an unsaved object deleted. Record the catalog publication and room
edit relationship in the operation receipt so a crash/retry resolves this case.

Default capture preserves authored components and script source/state recipes.
Velocity, contacts, transient input, connection tokens and native VM heaps are
excluded unless a component declares a portable snapshot policy. External live
entity references require an explicit choice: include an owned dependency, convert
to a required spawn parameter, or reject capture. Never persist a dangling room ID
as if it were an offline asset reference. Saving a prototype retains existing
instances' pinned revisions; adopting a newer revision is an explicit edit.

## Joining, replacing code and recovering

A join negotiates protocol/codec/schema capabilities and authenticates the room
grant. Send a pinned dependency manifest, a coherent snapshot at tick T and its
edit commit cursor. The client prepares assets/components off its active world,
buffers the subsequent ordered commits, then activates the snapshot and catches
up. Gameplay-required code/collision data must be ready before participation;
missing visual art can use an advertised placeholder policy. A slow renderer must
not hold every player at a global loading barrier indefinitely.

Reconnect supplies room/epoch, acknowledged commit/snapshot/input cursors and its
participant resumption proof. Resume from retained state or require a fresh
snapshot explicitly. Do not replay stale input after a disconnect. A new epoch
invalidates old gameplay references and requires a declared mapping/snapshot;
split authorities cannot both publish as the same epoch. Seamless host migration
is a later measured feature, not a side effect of P2P transport.

Live code publishes a content-hashed manifest of domain, bindings, state schema
and dependencies. Prepare the replacement separately, validate/migrate an owned
portable state record, then activate at an authority-selected tick. Authority logic
can update independently of client art; shared prediction changes require matching
versions or a temporary authoritative-only fallback. Compile/migration failures
leave the last working instance active. A failed running instance is disabled and
replaced according to an explicit state policy. Arbitrary Lua/C#/Python heaps and
external side effects cannot be rolled back by restoring source text.

Keep per-script diagnostics and receipts, checkpoint/journal recovery and isolated
authoring preparation. A language wrapper still cannot contain arbitrary native
crashes or GPU/driver faults; process boundaries are appropriate where those
failures need independent recovery.

## Transport decision and mobile proof

Evaluate **GameNetworkingSockets** first for native room traffic. It offers
reliable/unreliable messages, encryption and prioritized lanes under BSD-3-Clause;
its public implementation does not require Steam. Entity replication, catalogs
and serialization remain ours. [Upstream README](https://github.com/ValveSoftware/GameNetworkingSockets).

The cost is real: protobuf (and Abseil with newer protobuf) plus a crypto backend.
Its current build documentation includes iOS source builds and simulator smoke
tests. Its CMake recognizes Android, but that is not Quest hardware validation.
First prove pinned source/dependency builds on Mac/Linux, Android arm64/Quest and
iOS; use the correct target libraries and a host `protoc`. Check socket lifecycle,
crypto, loss/MTU behavior and reconnect on real mobile devices. No library binaries
or SDK build products go in Git. [Build documentation](https://github.com/ValveSoftware/GameNetworkingSockets/blob/master/BUILDING.md),
[Android platform detection](https://github.com/ValveSoftware/GameNetworkingSockets/blob/master/CMakeLists.txt).

Start with an explicit LAN/listen-server endpoint and authenticated invitation,
then add Internet rendezvous/relay. GNS supplies pluggable signaling and ICE;
identity/matchmaking and relay hosting still need application services. Its ICE
integration does not speak browser WebRTC data channels, and open-source GNS does
not include access to Steam Datagram Relay. [P2P requirements](https://github.com/ValveSoftware/GameNetworkingSockets/blob/master/README_P2P.md).

Keep a narrow ICE transport seam: owned connection events, negotiated delivery
lanes, message send/receive, pressure/stats and disconnect. The adapter can have
internal threads; dispatch into room state belongs to the simulation owner thread.
Transport encryption alone does not authenticate a friend or grant scene editing;
bind invitations/server identity to the encrypted session. Room logic, entity
codecs and tests must work with an in-memory loss/reorder adapter too.

ENet remains a smaller MIT alternative with reliability/channels, but its upstream
scope omits authentication and encryption; those would require additional work.
[ENet scope](https://github.com/lsalzman/enet/blob/master/docs/mainpage.dox),
[channel design](https://github.com/lsalzman/enet/blob/master/docs/design.dox).
QUIC streams plus DATAGRAM are a credible later native transport; a stack and
application mapping still have to be chosen. [RFC 9221](https://www.rfc-editor.org/rfc/rfc9221).
For browser participation, add a WebRTC adapter/gateway with explicit reliable
control and unordered limited-retransmission state channels. Browser interoperability
is a separate proof. [WebRTC data-channel contract](https://www.w3.org/TR/webrtc/#rtcdatachannel).

## First implementation and evidence

Implement in this order, keeping each step runnable:

1. Introduce portable component/entity recipes and adapters over the current scene;
   publish truthful standalone/domain capabilities. Add copy/delete/ID inspection
   using checked entity references and one prepared edit.
2. Save and instantiate a persistent prototype with exact dependency closure. Restart
   the host and spawn it offline. Inject disk/full-write/hash/missing-reference and
   crash-between-save-and-despawn failures.
3. Add a headless authoritative room reducer, framed codecs and a durable edit/receipt
   journal. Run two clients through a simulated lossy/reordered transport. Verify
   concurrent edits, stale epochs, duplicate operations and late joins.
4. Connect desktop and Quest to that room through the native transport. Both see the
   same entities and edits; closing one surface or reconnecting a headset preserves
   the authoritative world. Prove Mac Metal/OpenGL and Linux rendering against the
   same server state.
5. Add persistent authority behavior/trigger/attachment/modifier components and one
   predicted input adapter. Execute the shark-weapon acceptance case described by
   Ignition before adding Internet hosting, browser participation or host migration.

Use ambitious configurable profiles and report measured failure points. Exercise
500, 10,000 and 100,000 authored copies, then increase them where the host can;
separate total entities, active simulated bodies, relevant replicated entities and
draw instances. A large catalog is not a requirement to simulate or send every
entry every tick. Measure p50/p95/p99 simulation/encode/prepare time, memory,
bytes/client, correction magnitude and recovery latency under several peer counts,
RTTs, packet loss and bandwidth limits. Distinguish selected stress inputs from
achieved performance. Keep finite parsing/allocation limits and backpressure
configurable; instrument and raise them deliberately rather than hard-code tiny
object-model limits or allow one unbounded message to halt all participants.
