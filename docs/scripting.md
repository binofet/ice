# Interpreted scripting and runtime adapters

`ice::script_lua` is the first runtime adapter. Enable `ICE_BUILD_SCRIPTING`
(on by default), include `ice/script/lua_program.h`, and link `ice::script_lua`.
It has no renderer, window, network, app component registry or mandatory managed SDK.
`ICE_BUILD_SCRIPTING=OFF` omits the interpreter and its source download, including
for a small headless/watch host. C++20 and C++23 use the same public API.

## Source and capabilities

The build fetches official [Lua 5.5.1](https://www.lua.org/download.html), released
2026-07-24, from `https://www.lua.org/ftp/lua-5.5.1.tar.gz`, SHA-256
`1c4b4068d67061f2a2231ad2b5422e77acea1487ea9890f6320af614f4373dce`.
Lua is [MIT licensed](https://www.lua.org/license.html); its source archive retains
the copyright/license notice. No interpreter binaries, CLI, bytecode compiler or
external native modules are downloaded. `ICE_LUA_SOURCE_DIR` deliberately selects
a local 5.5.1 source tree for offline/dependency development; that bypasses archive
verification and is reported by CMake. API/version and numeric-layout checks remain.

Lua compiles as portable C99, statically linked with position-independent code.
It interprets its internal bytecode and allocates no executable code pages. Only
UTF-8 source text enters this API; binary chunks are rejected by Lua's text-only
loader. This is suitable for building on restrictive mobile targets, subject to
their distribution rules about downloaded behavior/content. It does not establish
store approval or imply that every platform has already been hardware tested.

The shared adapter contract is owned data, stable handles, explicit lifecycle and
capability discovery. Planned adapters must declare the following separately:

| Runtime | Execution profile | Integration boundary and restrictions |
| --- | --- | --- |
| Lua 5.5 | Implemented interpreter | Private VM, heap/instruction budgets, scalar calls; no JIT |
| LuaJIT | Planned, distinct Lua 5.1-family adapter | Not a drop-in Lua 5.5 replacement; interpreter/JIT capability and language compatibility must be explicit |
| .NET, C# focus | Planned | Platform-dependent JIT/AOT; runtime ownership, managed heap and host permissions need their own contract |
| Python | Planned | Runtime/GIL/module policy, heap/native-extension isolation and cancellation need their own contract |
| V8 | Planned | Isolate/heap limits, interrupts and platform-dependent JIT/JIT-less availability need their own contract |

No planned adapter is linked or available through this milestone. C#/Python
Metagen authoring is separate from executing those languages inside the engine.
Adapters should exchange validated records and generational object handles, never
raw app pointers. Do not pretend a .NET/V8/Python heap limit is equivalent to a
Lua instruction limit. Native extensions, hard wall-clock deadlines and untrusted
third-party runtimes may require worker/process isolation; a same-process language
wrapper cannot contain VM/compiler bugs or arbitrary native code.

## Program ownership and publication

```cpp
auto candidate = ice::script::lua_program::prepare(resolver, "scripts://game/spin.lua");
if (!candidate)
    return; // Existing program and host scene have not been touched.

ice::script::scalar_record input{{"dt", 1.0 / 60.0},
                                 {"object", std::string("entity:42:7")}};
auto output = candidate->call("update", input);
if (output) {
    // Validate against the component schema, resolve handles and stage host edits.
    // Publish only when the entire host transaction can be accepted.
}
```

```lua
local elapsed = 0
function update(input)
    elapsed = elapsed + input.dt
    return { object = input.object, rotation_y = elapsed }
end
```

One move-only `lua_program` owns one VM. Preparation reads the URI exactly once,
compiles and runs the top-level chunk in that new VM. The resolver remains
responsible for the cost/size of its provider read before the source limit can be
checked; URI selection grants the script no ability to read more resources.
Canonical requested URI/query/fragment identify diagnostics. The provider reads
the document according to existing URI resolver rules. No implicit HTTP transport
is introduced.

The host serializes preparation, calls, moves and destruction. Execute at an
explicit application/frame boundary; this module schedules no threads and has no
hidden engine callbacks. A call performs `global_function(input_table)` and accepts
one returned scalar table. Every output is copied into an owned record before it
is returned; no Lua stack view or table reference escapes. The host still validates
component-specific types/ranges and object-handle generations before committing.
This API alone never mutates the scene or increments its revision.

VM globals/upvalues can mutate before a runtime, budget or output-contract error.
Such errors fault the instance, and later calls return `faulted` without executing.
There is no claim to roll back arbitrary Lua state. A rejected host name/input is
checked before Lua and does not fault the program. A missing/nonfunction entry is
a VM call failure and faults it. Replace a failed instance with a successfully
prepared new owner; do not clear its fault flag and reuse partially changed state.
Failed reload candidates leave the old owner available. State migration, shared
VM modules and transactional multi-component execution are not implemented here.

## Scalar and resource limits

Input/output use `meta::value` in string-keyed records, bounded to 128 fields,
128-byte nonempty UTF-8 field names, 4096-byte UTF-8 string values and 64 KiB charged
record bytes (name bytes + eight bytes per scalar + string bytes). NUL, malformed
UTF-8 and nonfinite numbers are rejected. Lua integers preserve signed 64-bit
values exactly. Unsigned inputs are allowed only through `INT64_MAX` and return
as signed integers; larger identifiers must be strings. Lua float results remain
double values. No numeric string conversion occurs at the host boundary.

Explicit null is rejected: Lua `nil` removes a table field, so absent keys stay
absent. Nested tables, sequences, functions, threads and userdata cannot cross this
first profile. Component-schema validation is a separate host step.

Defaults are an 8 MiB Lua allocation cap, 100,000 instructions per top-level chunk
execution/call and 256 KiB source. Allowed configurations are 64 KiB..64 MiB heap,
1..10,000,000 instructions and 1 byte..1 MiB source. The allocator charges its own
alignment headers and retained allocation capacity; shrinking cannot accidentally
free accounting budget while retaining the old physical block. Values are process
allocator request sizes, excluding allocator-internal overhead and process RSS.
Output-copy and resolver host allocations are outside the VM cap. Host allocation
failure is reported where possible; allocating a diagnostic can itself fail under
process-wide exhaustion, so this API does not promise `noexcept`.

The instruction hook runs for every Lua VM instruction, including top-level
execution. It does not measure the parser, garbage collector, C library functions
or elapsed time. Source/heap caps bound their inputs; this is not a hard real-time
deadline. Host applications that need such a deadline require a stronger isolation
or scheduling design. No cross-platform bit-for-bit floating-point determinism is
promised. The VM's fixed string-hash seed is not an adversarial hash-collision defense.

## Restricted library surface and error handling

The module opens base, math, string, table and UTF-8 libraries only. It removes:

- `load`, `loadfile`, `dofile`, `collectgarbage`, `print`, `warn`, `pcall`, `xpcall`,
  `getmetatable` and `setmetatable`.
- String bytecode dumping, formatting, pattern matching (`find`, `match`, `gmatch`,
  `gsub`) and binary packing/unpacking.
- `math.random` and `math.randomseed`; a future deterministic random service should
  have explicit host state instead of ambient entropy.
- `table.move`, `insert`, `remove` and `sort`. Sparse table boundaries can drive
  very large native loops without proportional allocation or bytecode hook calls;
  scripts can express these algorithms with instruction-budgeted Lua loops.

`tostring` accepts only scalar values, avoiding table/function pointer formatting.
The `io`, `os`, `debug`, `package` and coroutine libraries are not linked/opened;
`require` is absent. There are no filesystem, process, network or native host calls.
The profile intentionally excludes metatable/finalizer construction and Lua-level
error-catching, so script code cannot swallow a budget error and keep retrying.
Internal tables/functions still exist as normal Lua implementation objects.

Lua is compiled as C and may `longjmp` on errors. All allocation-capable C API work,
including library initialization, compilation and input/output marshalling, runs
below protected trampolines with only trivially destructible automatic state.
The allocator and hook neither allocate C++ owners nor throw C++ exceptions.
Borrowed result strings remain rooted until C++ copies them after the protected
call. Diagnostics carry source URI and a bounded UTF-8 message; nonstring/binary or
oversized Lua error objects do not invoke user `tostring` or leak native addresses.

`tests/script_lua.cpp` exercises repeated state, signed/unsigned boundaries, restricted
libraries, source-only loading, compile/runtime errors, infinite loops, allocation
exhaustion, malformed/large/nonfinite outputs, rejected host inputs, failed reload
candidates and recovery by ownership replacement. CTest needs no GPU or device.

## Lua distribution notice

Retain this upstream notice with products distributing the linked Lua interpreter:

```text
Copyright (C) 1994-2026 Lua.org, PUC-Rio.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```
