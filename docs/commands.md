# Headless command sessions

`ice::commands` owns discovery, schema validation, session guards, structured
receipts and a bounded retry cache. It has no renderer, network transport, language
model, application command names or authorization policy. `ICE_BUILD_COMMANDS=OFF`
omits the optional module; standalone consumers may include
`cmake/ICECommands.cmake` after `ICEStandards.cmake` and link `ice::commands`.

Create one `service` for the application session, then keep it alive across
renderer, shader and surface replacement. Its opaque session identity belongs to
that lifetime, and its revision starts at zero. Moving the service transfers its
identity and cached receipts. Starting another service creates another identity.
The identity is a revision guard, not a credential or durable persistence key.

## Discovery and registration

`service::create` takes owned descriptors and callbacks. Discovery is a borrowed
`span<const command_descriptor>` that remains immutable until service ownership
changes. A descriptor includes a stable command ID, unique display name,
description, scalar argument schema and declared `read_only` or `mutation` effect.
`arguments = nullopt` describes a command that accepts no fields. Other commands
use the existing Metagen scalar schema, including field IDs, defaults, nullability,
precision, numeric constraints and namespaced metadata. Command IDs and operation
IDs accept 1–128 ASCII letters, digits, `.`, `_`, `:` and `-`.

Callbacks are retained until service destruction. Captures must own dependencies
or borrow host objects that outlive the service. There is no dynamic registration
or unregistration within a session. Discovery does not grant permission to invoke
a command: access policy belongs to the application/transport.

## Invocation and revisions

The host invokes one request synchronously between frame submissions. The service
has no worker, lock, queue or implicit thread marshalling. Every operation on a
service, including discovery and destruction, belongs to the same host thread.
Do not move or destroy the service during a callback. Reentrant invocation returns
`service_busy` without entering another callback.

Mutations require both `expected_session` and `expected_revision`. Read-only
requests may omit both or supply the pair. Unknown commands, invalid schemas or
arguments, missing guards and stale guards return structured rejection receipts
without entering host code. Argument validation applies declared defaults and
float32 normalization before invocation. There is one operation per invocation;
there is no cross-command atomicity.

A successful handler returns `{changed, fields}`. Successful `changed=false`
operations retain the revision, including successful mutation no-ops. A successful
change advances it exactly once. Read-only handlers must report `changed=false`.
Session revisions never derive from a renderer-local revision, rewind on undo or
reset on renderer replacement.

The host remains responsible for transactional execution: prepare fallible
resources and metadata before committing, and return an ordinary error only if
host state is preserved. The service cannot roll back arbitrary application code.
Handler errors produce `handler_failed` without advancing revision. A thrown
exception, invalid output, or a read-only callback reporting a change produces an
inspection-required receipt and conservatively advances the revision. Such a
receipt has `changed=false` because no successful change was confirmed; the actual
host effect is unknown. Inspect state before choosing another operation.
This boundary catches C++ exceptions; it does not contain native crashes, driver
faults, hangs, malicious callbacks or data races.

## Exact bounded retries

A new bounded request with a valid operation ID on an active, non-reentrant service
receives a cached immutable receipt, including validation and host failures.
Reentrant calls, moved-from services and conflicting reuse return transient rejections.
Repeating the same operation ID
and exact authored request returns the same shared receipt without rerunning the
callback. Retry comparison precedes revision checks, so the receipt for a
successful mutation remains available after it advances the revision. Receipt
revisions describe that operation's completion, not necessarily current state.
Changing the command, any scalar representation/value, authored default omission
or guard while reusing an operation ID returns `operation_conflict`. Floating
point retry identity compares bits, including signed zero and NaN payloads.

The most recent 64 requests are retained in insertion order; lookup does not extend
their lifetime. An evicted ID is forgotten and can execute again. An old mutation
still needs current session guards, so a previously successful state change
cannot replay with its old revision. This is bounded deduplication, not durable or
unlimited exactly-once delivery. Operation IDs must remain unique for new work.
Oversized envelopes and malformed operation IDs are rejected without caching.

The service allocates the request/receipt cache entry and fallback diagnostic
before invoking the host. Publishing a successful result transfers its owned
scalar map without allocation or copying. Allocation failure during preparation
may propagate; it does not invoke the handler or retain a half-built receipt.
Retained `shared_ptr<const receipt>` values outlive both cache eviction and service
destruction.

## Bounds and scalar transport

The registry holds at most 128 commands and 1 MiB of descriptor text, including
schema metadata/defaults. Requests and outputs hold at most 128 scalar fields,
4096 bytes per scalar string and 64 KiB aggregate scalar text. Output field names
have a 128-byte UTF-8 budget and count against the output text budget. Scalar
strings are valid NUL-free UTF-8; floating point output must be finite. Outputs
retain signed/unsigned 64-bit integers exactly; adapters must not silently round
them through a JavaScript number.

Output fields provide ordered iteration, named access, node transfers and independent value copies.
Output moves and swaps transfer owned storage without allocating, including on standard libraries whose map move
constructor allocates a sentinel. An empty or moved-from output is a usable empty value. Construct
all fields before committing a host mutation; a prepared handler result can then move through the
service and into its receipt without allocating. Copying or inserting fields can still allocate.

Transport codecs and application-defined output semantics are separate concerns.
There are no pointers, shared host objects or arbitrary nested data in this first
profile. Metagen tags such as `command.choices` describe application policy;
the host must enforce enumerated values where scalar constraints are insufficient.

## Verification

`ice_command_tests` exercises owned immutable discovery, default/precision
normalization, rejection without handler entry, session and revision guards,
successful no-ops, exact retry identity, conflicting reuse, cache eviction,
ordinary host failures, exception/invalid-output invalidation, callback lifetime,
reentrant rejection and service movement. It is a CPU-only CTest in both supported
C++ language modes.
