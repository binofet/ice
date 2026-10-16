#pragma once
#include "ice/core/expected.h"
#include "ice/io/resource_uri.h"
#include "ice/meta/schema.h"
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace ice::script {
/// Owned, named scalar boundary: at most 128 fields, 128-byte nonempty UTF-8 names,
/// 4096-byte UTF-8 strings and 64 KiB total names/string/scalar bytes. No NUL or
/// nonfinite numbers. Null is unsupported (Lua nil is absence); uint64 must fit
/// int64 and returns as int64. Stable object identities should be strings.
using scalar_record = std::map<std::string, ice::meta::value, std::less<>>;
/// Distinguish rejected host requests from source, VM and output-contract failures.
enum class error_code {
	invalid_limits,
	source,
	compile,
	invalid_input,
	runtime,
	memory_limit,
	instruction_limit,
	invalid_output,
	faulted,
	host_memory
};
/// Owned failure; uri identifies the requested/canonical source, message is bounded
/// UTF-8 without NUL. Source-provider diagnostics are sanitized, never executed.
struct diagnostic {
	/// Failure category; memory_limit includes allocation failure below the configured cap.
	error_code code = error_code::runtime;
	/// Source identity, at most 2048 bytes; empty after a move, placeholder for invalid URIs.
	std::string uri;
	/// Human-readable context, at most 4096 bytes.
	std::string message;
};
/// Per-program Lua heap and per-prepare/call bytecode limits. They do not cap host
/// resolver allocations, compiler/native-library wall time or the process C stack.
struct lua_limits {
	/// Lua allocation bytes; allowed 64 KiB..64 MiB. Includes libraries and allocator headers.
	std::size_t memory_bytes = 8 * 1024 * 1024;
	/// Lua VM instructions per top-level execution/call, allowed 1..10000000.
	std::uint64_t instructions = 100000;
	/// Source bytes accepted after the provider read, allowed 1..1 MiB.
	std::size_t source_bytes = 256 * 1024;
};
/// Interpreted Lua 5.5 instance with no native host pointers or ambient I/O exposed.
/// Single-threaded: the host serializes prepare/call/destruction at its chosen boundary.
/// Call returns owned outputs only on complete success. VM globals/upvalues can change
/// before an error; execution/budget/output errors permanently fault this instance.
/// Prepare a separate candidate and replace ownership after success for safe reload.
/// This limits cooperative script resources; it is not process isolation against VM bugs.
class lua_program {
public:
	/// Read once through the resolver, compile text only and execute the source chunk in
	/// a fresh restricted VM. Failure publishes no candidate and touches no existing VM.
	static ice::expected<lua_program, diagnostic>
	prepare(const ice::io::resource_resolver &resolver, const std::string &uri,
	        lua_limits limits = {});
	/// Destroy the private VM. No caller-owned native resources are reachable from it.
	~lua_program();
	/// Transfer VM ownership without changing its allocator address or state.
	lua_program(lua_program &&other) noexcept;
	/// Replace this VM with another owner; the previous private state is destroyed.
	lua_program &operator=(lua_program &&other) noexcept;
	/// Each program owns one VM; copying is unavailable.
	lua_program(const lua_program &) = delete;
	/// Each program owns one VM; copy assignment is unavailable.
	lua_program &operator=(const lua_program &) = delete;
	/// Call a named global function(input_table) -> scalar_table. Names are ASCII Lua
	/// identifiers of 1..128 bytes. Invalid host input is rejected before entering Lua;
	/// other failed calls fault the VM. No Lua tables, pointers or borrowed strings escape.
	ice::expected<scalar_record, diagnostic> call(std::string_view     function,
	                                              const scalar_record &input);
	/// Whether a call disabled this VM, or ownership has been moved away.
	bool faulted() const;
	/// Canonical source identity; empty after a move. Borrow lasts until move/destruction.
	std::string_view source_uri() const;
	/// Current Lua allocation bytes, or zero after a move; excludes host output copies.
	std::size_t memory_bytes() const;

private:
	struct implementation;
	explicit lua_program(std::unique_ptr<implementation> state);
	std::unique_ptr<implementation> state_;
};
} // namespace ice::script
