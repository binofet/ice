#include "ice/script/lua_program.h"
#include "ice/core/utf8.h"
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace ice::script {
namespace {
constexpr std::size_t max_fields = 128, max_name = 128, max_string = 4096,
                      max_record_bytes = 64 * 1024;
static_assert(sizeof(lua_Integer) == sizeof(std::int64_t));
static_assert(std::numeric_limits<lua_Integer>::is_signed);
static_assert(std::numeric_limits<lua_Number>::digits == 53);
static_assert(LUA_VERSION_RELEASE_NUM == 50501);

diagnostic failure(error_code code, std::string_view uri, std::string_view message) {
	return {code, ice::core::valid_utf8_text(uri, 2048) ? std::string(uri) : "<invalid Lua URI>",
	        ice::core::valid_utf8_text(message, 4096) ? std::string(message)
	                                                  : "Lua diagnostic exceeded text limits"};
}
bool identifier(std::string_view name) {
	if (name.empty() || name.size() > max_name)
		return false;
	for (std::size_t index = 0; index < name.size(); ++index) {
		const auto c = name[index];
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
		      (index && c >= '0' && c <= '9')))
			return false;
	}
	return true;
}
bool valid_input(const scalar_record &input) {
	if (input.size() > max_fields)
		return false;
	std::size_t bytes = 0;
	for (const auto &[name, value] : input) {
		if (name.empty() || !ice::core::valid_utf8_text(name, max_name) || value.index() == 0 ||
		    value.valueless_by_exception())
			return false;
		bytes += name.size() + sizeof(double);
		if (const auto *number = std::get_if<double>(&value); number && !std::isfinite(*number))
			return false;
		if (const auto *integer = std::get_if<std::uint64_t>(&value);
		    integer && *integer > std::uint64_t(std::numeric_limits<std::int64_t>::max()))
			return false;
		if (const auto *text = std::get_if<std::string>(&value)) {
			if (!ice::core::valid_utf8_text(*text, max_string))
				return false;
			bytes += text->size();
		}
		if (bytes > max_record_bytes)
			return false;
	}
	return true;
}

// Everything below a Lua protected trampoline uses only trivially destructible
// automatic state. Lua's C longjmp must never cross a C++ string/container owner.
struct memory_budget {
	std::size_t used = 0, limit = 0;
	bool        denied = false;
};
struct alignas(std::max_align_t) allocation_header {
	std::size_t bytes;
};
void *allocate(void *context, void *pointer, std::size_t old_lua_size, std::size_t new_size) {
	(void)old_lua_size; // New allocations pass a type tag; retained capacity uses our header.
	auto             *budget   = static_cast<memory_budget *>(context);
	auto             *previous = pointer ? static_cast<allocation_header *>(pointer) - 1 : nullptr;
	const std::size_t old_size = previous ? previous->bytes : 0;
	if (!new_size) {
		std::free(previous);
		budget->used -= old_size;
		return nullptr;
	}
	if (new_size > std::numeric_limits<std::size_t>::max() - sizeof(allocation_header)) {
		budget->denied = true;
		return nullptr;
	}
	new_size += sizeof(allocation_header);
	if (new_size > old_size && new_size - old_size > budget->limit - budget->used) {
		budget->denied = true;
		return nullptr;
	}
	auto *result = static_cast<allocation_header *>(std::realloc(previous, new_size));
	if (!result) {
		// Lua requires shrinking allocations to succeed. Retain/account the larger
		// original block if the host realloc cannot shrink it; later frees use this
		// header rather than Lua's now-smaller logical size.
		if (new_size <= old_size)
			return pointer;
		budget->denied = true;
		return nullptr;
	}
	result->bytes = new_size;
	budget->used  = budget->used - old_size + new_size;
	return result + 1;
}
struct instruction_budget {
	std::uint64_t remaining = 0;
	bool          exhausted = false;
};
struct call_context;
struct vm_context {
	instruction_budget instructions;
	call_context      *active = nullptr;
};
void instruction_hook(lua_State *vm, lua_Debug *debug) {
	(void)debug;
	auto *budget = &(*static_cast<vm_context **>(lua_getextraspace(vm)))->instructions;
	if (!budget->remaining) {
		budget->exhausted = true;
		luaL_error(vm, "Lua instruction budget exhausted");
		return;
	}
	--budget->remaining;
}
int bounded_table_remove(lua_State *vm) {
	luaL_checktype(vm, 1, LUA_TTABLE);
	const auto raw_size = lua_rawlen(vm, 1);
	if (raw_size > static_cast<std::size_t>(std::numeric_limits<lua_Integer>::max()))
		return luaL_error(vm, "Table border exceeds the supported integer range");
	const auto size     = static_cast<lua_Integer>(raw_size);
	auto       position = luaL_optinteger(vm, 2, size);
	// Match Lua 5.5: the usual range is [1,size+1], while position==size
	// also permits the default 0 on an empty sequence. Unsigned subtraction
	// keeps both negative arguments and size==maxinteger free of overflow.
	if (position != size &&
	    static_cast<lua_Unsigned>(position) - lua_Unsigned{1} > static_cast<lua_Unsigned>(size))
		return luaL_argerror(vm, 2, "position out of bounds");
	const auto shifts = position < size ? static_cast<std::uint64_t>(size - position) : 0;
	auto      *budget = &(*static_cast<vm_context **>(lua_getextraspace(vm)))->instructions;
	// Native raw operations do not run the VM hook. Reserve one unit for
	// length/result/clear and two per relocation before any indexed mutation.
	// Compare by division before multiplication: sparse borders can reach the largest signed64-bit
	// integer.
	if (budget->remaining < 3 || shifts > (budget->remaining - 3) / 2) {
		budget->exhausted = true;
		return luaL_error(vm, "Lua instruction budget exhausted by table.remove");
	}
	budget->remaining -= 3 + shifts * 2;
	lua_rawgeti(vm, 1, position);
	for (; position < size; ++position) {
		lua_rawgeti(vm, 1, position + 1);
		lua_rawseti(vm, 1, position);
	}
	lua_pushnil(vm);
	lua_rawseti(vm, 1, position);
	return 1;
}
int scalar_tostring(lua_State *vm) {
	switch (lua_type(vm, 1)) {
	case LUA_TNIL:
		lua_pushliteral(vm, "nil");
		break;
	case LUA_TBOOLEAN:
		lua_pushstring(vm, lua_toboolean(vm, 1) ? "true" : "false");
		break;
	case LUA_TNUMBER:
	case LUA_TSTRING:
		lua_pushvalue(vm, 1);
		lua_tolstring(vm, -1, nullptr);
		break;
	default:
		return luaL_error(vm, "tostring accepts scalar values only");
	}
	return 1;
}
int explicit_randomseed(lua_State *vm) {
	const int arguments = lua_gettop(vm);
	if (arguments < 1 || arguments > 2)
		return luaL_error(vm, "randomseed requires one or two explicit integer seeds");
	for (int index = 1; index <= arguments; ++index)
		if (!lua_isinteger(vm, index))
			return luaL_error(vm, "randomseed accepts integer seeds only");
	lua_pushvalue(vm, lua_upvalueindex(1));
	lua_insert(vm, 1);
	lua_call(vm, arguments, 2);
	return 2;
}
int scalar_format(lua_State *vm) {
	const int arguments = lua_gettop(vm);
	if (arguments < 1 || arguments > 33 || lua_type(vm, 1) != LUA_TSTRING)
		return luaL_error(vm, "format requires a string and at most 32 scalar values");
	for (int index = 1; index <= arguments; ++index) {
		const auto type = lua_type(vm, index);
		if (type == LUA_TSTRING) {
			if (lua_rawlen(vm, index) > max_string)
				return luaL_error(vm, "format string arguments exceed 4096 bytes");
		} else if (type != LUA_TNUMBER)
			return luaL_error(vm, "format accepts only strings and numbers");
	}
	std::size_t size   = 0;
	const char *format = lua_tolstring(vm, 1, &size);
	// In particular, Lua's %p also exposes the address of a STRING. Scalar-only
	// arguments alone do not prevent native pointer disclosure. Let upstream check
	// the complete width/precision grammar after this bounded conversion scan.
	for (std::size_t index = 0; index < size; ++index) {
		if (format[index] != '%')
			continue;
		if (++index < size && format[index] == '%')
			continue;
		while (index < size &&
		       (format[index] == '-' || format[index] == '+' || format[index] == ' ' ||
		        format[index] == '#' || format[index] == '.' ||
		        (format[index] >= '0' && format[index] <= '9')))
			++index;
		if (index == size ||
		    !(format[index] == 'c' || format[index] == 'd' || format[index] == 'i' ||
		      format[index] == 'u' || format[index] == 'o' || format[index] == 'x' ||
		      format[index] == 'X' || format[index] == 'a' || format[index] == 'A' ||
		      format[index] == 'f' || format[index] == 'e' || format[index] == 'E' ||
		      format[index] == 'g' || format[index] == 'G' || format[index] == 'q' ||
		      format[index] == 's'))
			return luaL_error(vm, "Unsupported format conversion");
	}
	lua_pushvalue(vm, lua_upvalueindex(1));
	lua_insert(vm, 1);
	// The original formatter is C code, and our automatic state is trivial. Any
	// format/allocation error unwinds directly to the outer protected Lua entry.
	lua_call(vm, arguments, 1);
	if (lua_rawlen(vm, -1) > max_string)
		return luaL_error(vm, "Formatted result exceeds 4096 bytes");
	return 1;
}
void remove_fields(lua_State *vm, const char *const *names) {
	for (std::size_t index = 0; names[index]; ++index) {
		lua_pushnil(vm);
		lua_setfield(vm, -2, names[index]);
	}
}
int open_libraries(lua_State *vm) {
	luaL_requiref(vm, "_G", luaopen_base, 1);
	static const char *const removed_base[]{"collectgarbage", "dofile",       "load",  "loadfile",
	                                        "print",          "warn",         "pcall", "xpcall",
	                                        "getmetatable",   "setmetatable", nullptr};
	remove_fields(vm, removed_base);
	lua_pushcfunction(vm, scalar_tostring);
	lua_setfield(vm, -2, "tostring");
	lua_pop(vm, 1);
	luaL_requiref(vm, LUA_MATHLIBNAME, luaopen_math, 1);
	// Upstream initially seeds from ambient entropy. Replace that state before
	// executing any source, then retain its shared PRNG in both original closures.
	lua_getfield(vm, -1, "randomseed");
	lua_pushinteger(vm, 0);
	lua_pushinteger(vm, 0);
	lua_call(vm, 2, 0);
	lua_getfield(vm, -1, "randomseed");
	lua_pushcclosure(vm, explicit_randomseed, 1);
	lua_setfield(vm, -2, "randomseed");
	lua_pop(vm, 1);
	luaL_requiref(vm, LUA_STRLIBNAME, luaopen_string, 1);
	lua_getfield(vm, -1, "format");
	lua_pushcclosure(vm, scalar_format, 1);
	lua_setfield(vm, -2, "format");
	static const char *const removed_string[]{"dump", "find",   "match",    "gmatch", "gsub",
	                                          "pack", "unpack", "packsize", nullptr};
	remove_fields(vm, removed_string);
	lua_pop(vm, 1);
	luaL_requiref(vm, LUA_TABLIBNAME, luaopen_table, 1);
	// Keep only the native loop whose indexed work is charged to our shared budget.
	// Other potentially large native table loops remain unavailable.
	static const char *const removed_table[]{"move", "insert", "sort", nullptr};
	remove_fields(vm, removed_table);
	lua_pushcfunction(vm, bounded_table_remove);
	lua_setfield(vm, -2, "remove");
	lua_pop(vm, 1);
	luaL_requiref(vm, LUA_UTF8LIBNAME, luaopen_utf8, 1);
	lua_pop(vm, 1);
	return 0;
}
struct compile_context {
	const char *source = nullptr, *name = nullptr;
	std::size_t size        = 0;
	int         load_status = LUA_OK;
};
int compile_execute(lua_State *vm) {
	auto *context        = static_cast<compile_context *>(lua_touserdata(vm, 1));
	context->load_status = luaL_loadbufferx(vm, context->source, context->size, context->name, "t");
	if (context->load_status != LUA_OK)
		return lua_error(vm);
	lua_call(vm, 0, 0);
	return 0;
}
struct input_field {
	const char             *name      = nullptr;
	std::size_t             name_size = 0;
	const ice::meta::value *value     = nullptr;
};
struct output_field {
	const char *name = nullptr, *text = nullptr;
	std::size_t name_size = 0, text_size = 0;
	int         type    = LUA_TNIL;
	bool        boolean = false, integer = false;
	lua_Integer signed_value = 0;
	lua_Number  float_value  = 0;
};
struct call_context {
	const char                    *function      = nullptr;
	std::size_t                    function_size = 0, input_count = 0, output_count = 0;
	const input_field             *input          = nullptr;
	output_field                  *output         = nullptr;
	bool                           invalid_output = false;
	std::span<const host_function> hosts;
	std::uint32_t                  remaining_host_calls = 0;
	bool                           host_failed = false, host_limit = false;
	std::string                    host_message;
	scalar_record                  host_output;
};
void push_record(lua_State *vm, const input_field *fields, std::size_t count) {
	lua_createtable(vm, 0, static_cast<int>(count));
	for (std::size_t index = 0; index < count; ++index) {
		const auto *field = &fields[index];
		const auto *value = field->value;
		lua_pushlstring(vm, field->name, field->name_size);
		switch (value->index()) {
		case 1:
			lua_pushboolean(vm, std::get<bool>(*value));
			break;
		case 2:
			lua_pushinteger(vm, static_cast<lua_Integer>(std::get<std::int64_t>(*value)));
			break;
		case 3:
			lua_pushinteger(vm, static_cast<lua_Integer>(std::get<std::uint64_t>(*value)));
			break;
		case 4:
			lua_pushnumber(vm, std::get<double>(*value));
			break;
		case 5:
			lua_pushlstring(vm, std::get<std::string>(*value).data(),
			                std::get<std::string>(*value).size());
			break;
		default:
			luaL_error(vm, "Invalid host scalar");
			return;
		}
		lua_rawset(vm, -3);
	}
}
const char *read_record(lua_State *vm, int table, output_field *fields, std::size_t *count) {
	if (lua_type(vm, table) != LUA_TTABLE)
		return "Expected a scalar record table";
	table             = lua_absindex(vm, table);
	*count            = 0;
	std::size_t bytes = 0;
	lua_pushnil(vm);
	while (lua_next(vm, table)) {
		if (*count == max_fields || lua_type(vm, -2) != LUA_TSTRING)
			return "Record must have at most 128 string-named fields";
		auto *field = &fields[(*count)++];
		field->name = lua_tolstring(vm, -2, &field->name_size);
		if (!field->name_size ||
		    !ice::core::valid_utf8_text({field->name, field->name_size}, max_name))
			return "Invalid record field name";
		bytes += field->name_size + sizeof(double);
		field->type = lua_type(vm, -1);
		switch (field->type) {
		case LUA_TBOOLEAN:
			field->boolean = lua_toboolean(vm, -1) != 0;
			break;
		case LUA_TNUMBER:
			field->integer = lua_isinteger(vm, -1) != 0;
			if (field->integer)
				field->signed_value = lua_tointeger(vm, -1);
			else {
				field->float_value = lua_tonumber(vm, -1);
				if (!std::isfinite(field->float_value))
					return "Record numbers must be finite";
			}
			break;
		case LUA_TSTRING:
			field->text = lua_tolstring(vm, -1, &field->text_size);
			if (!ice::core::valid_utf8_text({field->text, field->text_size}, max_string))
				return "Record strings must be bounded UTF-8 without NUL";
			bytes += field->text_size;
			break;
		default:
			return "Record values must be scalars";
		}
		if (bytes > max_record_bytes)
			return "Record exceeds 64 KiB";
		lua_pop(vm, 1);
	}
	return nullptr;
}
scalar_record copy_record(const output_field *fields, std::size_t count) {
	scalar_record result;
	for (std::size_t index = 0; index < count; ++index) {
		const auto      &field = fields[index];
		ice::meta::value value;
		if (field.type == LUA_TBOOLEAN)
			value = field.boolean;
		else if (field.type == LUA_TSTRING)
			value = std::string(field.text, field.text_size);
		else if (field.integer)
			value = static_cast<std::int64_t>(field.signed_value);
		else
			value = static_cast<double>(field.float_value);
		result.emplace(std::string(field.name, field.name_size), std::move(value));
	}
	return result;
}
std::size_t input_references(const scalar_record &record, input_field *fields) {
	std::size_t index = 0;
	for (const auto &[name, value] : record)
		fields[index++] = {name.data(), name.size(), &value};
	return index;
}
// No Lua API is called in this helper. Every C++ owner is destroyed before it
// returns to the C-compatible trampoline, which may subsequently longjmp.
void execute_host(call_context *context, std::size_t host_index, const output_field *fields,
                  std::size_t count) noexcept {
	try {
		auto input  = copy_record(fields, count);
		auto output = context->hosts[host_index].invoke(input);
		if (!output) {
			context->host_failed  = true;
			context->host_message = ice::core::valid_utf8_text(output.error().message, 4096)
			                            ? output.error().message
			                            : "Host callback returned an invalid diagnostic";
			return;
		}
		if (!valid_input(*output)) {
			context->host_failed  = true;
			context->host_message = "Host callback returned an invalid scalar record";
			return;
		}
		context->host_output = std::move(*output);
	} catch (...) {
		context->host_failed = true; // Catch without allocating another diagnostic here.
	}
}
int dispatch_host(lua_State *vm) {
	auto *context = (*static_cast<vm_context **>(lua_getextraspace(vm)))->active;
	if (!context)
		return luaL_error(vm, "No active host call scope");
	if (!context->remaining_host_calls) {
		context->host_limit = true;
		return luaL_error(vm, "Lua host call budget exhausted");
	}
	--context->remaining_host_calls;
	std::size_t size  = 0;
	const char *name  = lua_tolstring(vm, lua_upvalueindex(1), &size);
	std::size_t index = 0;
	while (index < context->hosts.size() &&
	       context->hosts[index].name != std::string_view(name, size))
		++index;
	if (index == context->hosts.size()) {
		context->host_failed = true;
		return luaL_error(vm, "Host capability is absent from this call");
	}
	std::array<output_field, max_fields> fields{};
	std::size_t                          count = 0;
	const char *error = lua_gettop(vm) == 1 ? read_record(vm, 1, fields.data(), &count)
	                                        : "Host function requires one scalar table";
	if (error) {
		context->host_failed = true;
		return luaL_error(vm, "%s", error);
	}
	execute_host(context, index, fields.data(), count);
	if (context->host_failed)
		return luaL_error(vm, "Host callback failed");
	std::array<input_field, max_fields> output{};
	const auto output_count = input_references(context->host_output, output.data());
	push_record(vm, output.data(), output_count);
	return 1;
}
static_assert(std::is_trivially_destructible_v<std::array<output_field, max_fields>>);
static_assert(std::is_trivially_destructible_v<std::array<input_field, max_fields>>);
int invoke(lua_State *vm) {
	auto *context = static_cast<call_context *>(lua_touserdata(vm, 1));
	lua_pushglobaltable(vm);
	lua_createtable(vm, 0, static_cast<int>(context->hosts.size()));
	for (std::size_t index = 0; index < context->hosts.size(); ++index) {
		const auto *host = &context->hosts[index];
		lua_pushlstring(vm, host->name.data(), host->name.size());
		lua_pushlstring(vm, host->name.data(), host->name.size());
		lua_pushcclosure(vm, dispatch_host, 1); // Only a Lua-owned name, never a callback pointer.
		lua_rawset(vm, -3);
	}
	lua_pushliteral(vm, "host");
	lua_pushvalue(vm, -2);
	lua_rawset(vm, -4);
	lua_pop(vm, 1);
	lua_pushlstring(vm, context->function, context->function_size);
	lua_rawget(vm, -2);
	if (lua_type(vm, -1) != LUA_TFUNCTION)
		return luaL_error(vm, "Requested global is not a function");
	push_record(vm, context->input, context->input_count);
	lua_call(vm, 1, 1);
	if (const char *error = read_record(vm, -1, context->output, &context->output_count)) {
		context->invalid_output = true;
		return luaL_error(vm, "%s", error);
	}
	// Keep the result table rooted until C++ has copied all borrowed string bytes.
	return 1;
}
int protected_call(lua_State *vm, lua_CFunction function, void *context, int results) {
	// These pushes cannot allocate: a zero-upvalue C function is a light function,
	// and the main stack always has at least LUA_MINSTACK free slots after a call.
	lua_pushcfunction(vm, function);
	lua_pushlightuserdata(vm, context);
	return lua_pcall(vm, 1, results, 0);
}
std::string_view lua_message(lua_State *vm) {
	if (lua_type(vm, -1) != LUA_TSTRING)
		return "Lua raised a non-string error";
	std::size_t size = 0;
	const char *text = lua_tolstring(vm, -1, &size); // Already a string: no conversion/allocation.
	return {text, size};
}
} // namespace

struct lua_program::implementation {
	memory_budget memory;
	vm_context    runtime;
	lua_State    *vm = nullptr;
	std::string   uri;
	lua_limits    limits;
	bool          faulted = false;
	~implementation() {
		if (vm)
			lua_close(vm);
	}
	void start_budget() {
		memory.denied        = false;
		runtime.instructions = {limits.instructions, false};
		lua_sethook(vm, instruction_hook, LUA_MASKCOUNT, 1);
	}
	diagnostic error(error_code fallback) const {
		return failure(runtime.instructions.exhausted ? error_code::instruction_limit
		               : memory.denied                ? error_code::memory_limit
		                                              : fallback,
		               uri, lua_message(vm));
	}
};

lua_program::lua_program(std::unique_ptr<implementation> state) : state_(std::move(state)) {}
lua_program::~lua_program()                                       = default;
lua_program::lua_program(lua_program &&other) noexcept            = default;
lua_program &lua_program::operator=(lua_program &&other) noexcept = default;

ice::expected<lua_program, diagnostic>
lua_program::prepare(const ice::io::resource_resolver &resolver, const std::string &uri,
                     lua_limits limits) {
	if (limits.memory_bytes < 64 * 1024 || limits.memory_bytes > 1024 * 1024 * 1024 ||
	    !limits.instructions || limits.instructions > 1000000000 || !limits.source_bytes ||
	    limits.source_bytes > 1024 * 1024 || limits.host_calls > 1000000)
		return ice::unexpected(
		    failure(error_code::invalid_limits, uri, "Invalid Lua resource limits"));
	try {
		ice::io::resource_uri canonical;
		std::string           error;
		if (uri.empty() || !ice::core::valid_utf8_text(uri, 2048) ||
		    !resolver.canonical(uri, canonical, error))
			return ice::unexpected(failure(error_code::source, uri, "Invalid Lua source URI"));
		auto state = std::make_unique<implementation>();
		state->uri = canonical.string();
		if (state->uri.size() > 2048)
			return ice::unexpected(
			    failure(error_code::source, uri, "Canonical Lua URI exceeds 2048 bytes"));
		state->limits = limits;
		std::vector<unsigned char> bytes;
		if (!resolver.read(state->uri, bytes, error))
			return ice::unexpected(failure(error_code::source, state->uri, error));
		if (bytes.empty() || bytes.size() > limits.source_bytes ||
		    !ice::core::valid_utf8_text(
		        {reinterpret_cast<const char *>(bytes.data()), bytes.size()}))
			return ice::unexpected(failure(error_code::source, state->uri,
			                               "Lua source must be bounded UTF-8 text without NUL"));
		state->memory.limit = limits.memory_bytes;
		// Fixed hash seed keeps this profile independent of clocks/OS entropy. It is not
		// a defense against adversarial hash collisions; use a process boundary for that.
		state->vm = lua_newstate(allocate, &state->memory, 0x494345u);
		if (!state->vm)
			return ice::unexpected(
			    failure(error_code::memory_limit, state->uri, "Cannot allocate Lua VM"));
		*static_cast<vm_context **>(lua_getextraspace(state->vm)) = &state->runtime;
		if (protected_call(state->vm, open_libraries, nullptr, 0) != LUA_OK)
			return ice::unexpected(state->error(error_code::runtime));
		state->start_budget();
		const std::string chunk_name = "@" + state->uri;
		compile_context   context{reinterpret_cast<const char *>(bytes.data()), chunk_name.c_str(),
                                bytes.size()};
		if (protected_call(state->vm, compile_execute, &context, 0) != LUA_OK)
			return ice::unexpected(state->error(
			    context.load_status == LUA_ERRSYNTAX ? error_code::compile : error_code::runtime));
		return lua_program(std::move(state));
	} catch (const std::bad_alloc &) {
		return ice::unexpected(
		    failure(error_code::host_memory, uri, "Host allocation failed while preparing Lua"));
	}
}
ice::expected<scalar_record, diagnostic> lua_program::call(std::string_view               function,
                                                           const scalar_record           &input,
                                                           std::span<const host_function> hosts) {
	if (!state_ || state_->faulted)
		return ice::unexpected(
		    failure(error_code::faulted, source_uri(), "Lua instance requires replacement"));
	if (state_->runtime.active)
		return ice::unexpected(
		    failure(error_code::invalid_input, state_->uri, "Lua call is already executing"));
	if (!identifier(function) || !valid_input(input) || hosts.size() > 32)
		return ice::unexpected(failure(error_code::invalid_input, state_->uri,
		                               "Invalid function name or scalar input record"));
	for (std::size_t index = 0; index < hosts.size(); ++index) {
		if (!identifier(hosts[index].name) || !hosts[index].invoke)
			return ice::unexpected(
			    failure(error_code::invalid_input, state_->uri, "Invalid host function"));
		for (std::size_t previous = 0; previous < index; ++previous)
			if (hosts[previous].name == hosts[index].name)
				return ice::unexpected(failure(error_code::invalid_input, state_->uri,
				                               "Duplicate host function name"));
	}
	std::array<input_field, max_fields> inputs{};
	input_references(input, inputs.data());
	std::array<output_field, max_fields> outputs{};
	call_context                         context;
	context.function             = function.data();
	context.function_size        = function.size();
	context.input_count          = input.size();
	context.input                = inputs.data();
	context.output               = outputs.data();
	context.hosts                = hosts;
	context.remaining_host_calls = state_->limits.host_calls;
	state_->start_budget();
	state_->runtime.active = &context;
	const int status       = protected_call(state_->vm, invoke, &context, 1);
	state_->runtime.active = nullptr; // No callback/borrowed context survives the protected call.
	// Sticky independently of Lua error handling: a richer future profile must
	// never turn a caught failed host operation into successful publication.
	if (status == LUA_OK && (context.host_failed || context.host_limit)) {
		state_->faulted = true;
		return ice::unexpected(failure(
		    context.host_limit ? error_code::host_call_limit : error_code::host_error, state_->uri,
		    context.host_limit             ? "Lua host call budget exhausted"
		    : context.host_message.empty() ? "Host callback failed"
		                                   : context.host_message));
	}
	if (status != LUA_OK) {
		state_->faulted = true;
		auto error      = state_->error(context.host_limit       ? error_code::host_call_limit
		                                : context.host_failed    ? error_code::host_error
		                                : context.invalid_output ? error_code::invalid_output
		                                                         : error_code::runtime);
		if (context.host_failed && error.code == error_code::host_error &&
		    !context.host_message.empty())
			error.message = std::move(context.host_message);
		return ice::unexpected(std::move(error));
	}
	try {
		auto result = copy_record(outputs.data(), context.output_count);
		// This slot cannot be to-be-closed: only a returned Lua table is on the C stack.
		lua_pop(state_->vm, 1);
		return result;
	} catch (const std::bad_alloc &) {
		state_->faulted = true;
		return ice::unexpected(failure(error_code::host_memory, state_->uri,
		                               "Host allocation failed while copying Lua output"));
	}
}
bool             lua_program::faulted() const { return !state_ || state_->faulted; }
std::string_view lua_program::source_uri() const {
	return state_ ? state_->uri : std::string_view{};
}
std::size_t lua_program::memory_bytes() const { return state_ ? state_->memory.used : 0; }
} // namespace ice::script
