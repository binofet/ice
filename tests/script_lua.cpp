#include "ice/script/lua_program.h"
#include <cmath>
#include <cstdio>
#include <limits>
#include <type_traits>
#include <utility>

#define CHECK(expression)                                                                          \
	do {                                                                                           \
		if (!(expression)) {                                                                       \
			std::fprintf(stderr, "Lua failed %d: %s\n", __LINE__, #expression);                    \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

namespace {
class memory_source final : public ice::io::virtual_scheme_handler {
public:
	std::string                   source;
	unsigned                      reads = 0;
	ice::io::resource_read_result read(const ice::io::resource_uri &,
	                                   std::vector<unsigned char> &bytes, std::string &) override {
		++reads;
		bytes.assign(source.begin(), source.end());
		return ice::io::resource_read_result::success;
	}
};
} // namespace

int main() {
	using namespace ice::script;
	static_assert(!std::is_copy_constructible_v<lua_program>);
	static_assert(std::is_nothrow_move_constructible_v<lua_program>);
	static_assert(std::is_nothrow_move_assignable_v<lua_program>);
	ice::io::resource_resolver resolver;
	auto                       memory = std::make_shared<memory_source>();
	resolver.register_handler("memory", memory);
	const std::string uri     = "memory://scripts/test.lua?revision=1#component";
	auto              prepare = [&](std::string source, lua_limits limits = {}) {
        memory->source = std::move(source);
        return lua_program::prepare(resolver, uri, limits);
	};
	auto program = prepare(R"lua(
local calls = 0
function update(input)
  calls = calls + 1
  return { count = calls, text = string.upper(input.text), id = input.id,
           signed = input.signed, unsigned = input.unsigned,
           floating = input.floating + 0.5, enabled = not input.enabled,
           unicode = '玩家 🎮', sine = math.sin(0), codepoints = utf8.len('🎮') }
end
function libraries(input)
  local names = {'io','os','debug','package','coroutine','require','load','loadfile',
                 'dofile','print','warn','collectgarbage','pcall','xpcall',
                 'getmetatable','setmetatable'}
  for _, name in ipairs(names) do assert(_G[name] == nil, name) end
  assert(string.dump == nil and string.format == nil and string.find == nil)
  assert(string.match == nil and string.gmatch == nil and string.gsub == nil)
  assert(string.pack == nil and math.random == nil and math.randomseed == nil)
  assert(table.move == nil and table.insert == nil and table.remove == nil and table.sort == nil)
  return { value = tostring(input.value), length = #table.concat({'a','b'}) }
end
function spin(input) while true do end end
function allocate(input) local t={} while true do t[#t+1]=string.rep('x',4096) end end
function broken(input) calls = calls + 1; error('intentional failure') end
function bad_output(input) return {bad = {nested = 1}} end
function not_record(input) return 42 end
function numeric_key(input) return {[1] = 'wrong'} end
function nonfinite(input) return {bad = 0/0} end
function infinity(input) return {bad = 1/0} end
function binary(input) return {bad = string.char(0)} end
function malformed(input) return {bad = string.char(255)} end
function long_string(input) return {bad = string.rep('x',4097)} end
function many_fields(input) local t={} for i=1,129 do t[tostring(i)] = i end return t end
function many_bytes(input) local t={} for i=1,16 do t[tostring(i)] = string.rep('x',4096) end return t end
function long_name(input) return {[string.rep('x',129)] = 1} end
function empty_name(input) return {[''] = 1} end
function pointer(input) return {bad = tostring({})} end
function error_object(input) error({}) end
function echo(input) return input end
function empty(input) return {} end
)lua");
	CHECK(program && memory->reads == 1);
	CHECK(program->source_uri() == uri && !program->faulted());
	CHECK(program->memory_bytes() > 0 && program->memory_bytes() <= lua_limits{}.memory_bytes);
	const scalar_record input{{"text", std::string("hello")},
	                          {"id", std::string("18446744073709551615")},
	                          {"signed", std::numeric_limits<std::int64_t>::min()},
	                          {"unsigned", std::uint64_t(std::numeric_limits<std::int64_t>::max())},
	                          {"floating", 2.5},
	                          {"enabled", true}};
	auto                result = program->call("update", input);
	CHECK(result && result->size() == 10 && memory->reads == 1);
	CHECK(std::get<std::int64_t>(result->at("count")) == 1);
	CHECK(std::get<std::string>(result->at("text")) == "HELLO");
	CHECK(std::get<std::string>(result->at("id")) == "18446744073709551615");
	CHECK(std::get<std::int64_t>(result->at("signed")) == std::numeric_limits<std::int64_t>::min());
	CHECK(std::get<std::int64_t>(result->at("unsigned")) ==
	      std::numeric_limits<std::int64_t>::max());
	CHECK(std::get<double>(result->at("floating")) == 3.0);
	CHECK(!std::get<bool>(result->at("enabled")));
	CHECK(std::get<std::string>(result->at("unicode")) == "玩家 🎮");
	CHECK(std::get<std::int64_t>(result->at("codepoints")) == 1);
	for (int index = 0; index < 100; ++index) {
		result = program->call("update", input);
		CHECK(result && std::get<std::int64_t>(result->at("count")) == index + 2);
	}
	result = program->call("libraries", {{"value", false}});
	CHECK(result && std::get<std::string>(result->at("value")) == "false");
	CHECK(program->call("empty", {})->empty());

	// Host validation does not enter Lua or fault/mutate the existing VM.
	for (const auto &name : {"", "a.b", "1function", "has space"}) {
		auto rejected = program->call(name, input);
		CHECK(!rejected && rejected.error().code == error_code::invalid_input &&
		      !program->faulted());
	}
	for (const auto &value :
	     {ice::meta::value{}, ice::meta::value{std::numeric_limits<std::uint64_t>::max()},
	      ice::meta::value{std::numeric_limits<double>::infinity()},
	      ice::meta::value{std::numeric_limits<double>::quiet_NaN()},
	      ice::meta::value{std::string(4097, 'x')}, ice::meta::value{std::string("a\0b", 3)},
	      ice::meta::value{std::string(1, char(255))}}) {
		auto rejected = program->call("echo", {{"bad", value}});
		CHECK(!rejected && rejected.error().code == error_code::invalid_input &&
		      !program->faulted());
	}
	CHECK(!program->call("echo", {{"", true}}));
	CHECK(!program->call("echo", {{std::string(129, 'x'), true}}));
	scalar_record oversized;
	for (int index = 0; index < 129; ++index)
		oversized.emplace(std::to_string(index), true);
	CHECK(!program->call("echo", oversized));
	oversized.clear();
	for (int index = 0; index < 16; ++index)
		oversized.emplace(std::to_string(index), std::string(4096, 'x'));
	CHECK(!program->call("echo", oversized));
	result = program->call("update", input);
	CHECK(result && std::get<std::int64_t>(result->at("count")) == 102);

	// Preparation is isolated: every bad candidate leaves the old program callable.
	const std::string working_source = memory->source;
	auto              syntax         = prepare("function unfinished(");
	CHECK(!syntax && syntax.error().code == error_code::compile && syntax.error().uri == uri);
	CHECK(syntax.error().message.find("test.lua") != std::string::npos);
	auto startup = prepare("error('startup failure')");
	CHECK(!startup && startup.error().code == error_code::runtime);
	auto loop = prepare("while true do end", {64 * 1024, 1000, 256 * 1024});
	CHECK(!loop && loop.error().code == error_code::instruction_limit);
	auto binary = prepare(std::string("\x1bLua", 4));
	CHECK(!binary && binary.error().code == error_code::compile);
	CHECK(!prepare(std::string("--\0", 3)));
	CHECK(!prepare(std::string(1024 * 1024 + 1, ' ')));
	CHECK(!prepare("return", {63 * 1024, 100, 100}));
	CHECK(!prepare("return", {64 * 1024, 0, 100}));
	CHECK(!prepare("return", {64 * 1024, 10000001, 100}));
	CHECK(!prepare("return", {64 * 1024, 100, 0}));
	CHECK(!prepare("return", {64 * 1024, 100, 2}));
	auto compiler_memory =
	    prepare("local value = '" + std::string(70000, 'x') + "'", {64 * 1024, 1000, 256 * 1024});
	CHECK(!compiler_memory && compiler_memory.error().code == error_code::memory_limit);
	CHECK(!lua_program::prepare(resolver, "missing://script/a.lua"));
	CHECK(!lua_program::prepare(resolver, std::string(2049, 'x')));
	CHECK(program->call("update", input));

	for (const auto &function :
	     {"bad_output", "not_record", "numeric_key", "nonfinite", "infinity", "binary", "malformed",
	      "long_string", "many_fields", "many_bytes", "long_name", "empty_name"}) {
		auto candidate = prepare(working_source);
		CHECK(candidate);
		auto failed = candidate->call(function, {});
		CHECK(!failed && failed.error().code == error_code::invalid_output &&
		      failed.error().uri == uri);
		CHECK(candidate->faulted());
		CHECK(candidate->call("empty", {}).error().code == error_code::faulted);
	}
	for (const auto &function : {"broken", "missing", "pointer", "error_object"}) {
		auto candidate = prepare(working_source);
		CHECK(candidate);
		auto failed = candidate->call(function, {});
		CHECK(!failed && failed.error().code == error_code::runtime && candidate->faulted());
	}
	auto exhausted = prepare(working_source, {64 * 1024, 10000, 256 * 1024});
	CHECK(exhausted);
	auto allocation = exhausted->call("allocate", {});
	CHECK(!allocation && allocation.error().code == error_code::memory_limit &&
	      exhausted->faulted());
	CHECK(exhausted->memory_bytes() <= 64 * 1024);
	// Input allocation also belongs inside the protected boundary, before user code.
	auto marshalling =
	    prepare("function echo(input) return input end", {64 * 1024, 1000, 256 * 1024});
	CHECK(marshalling);
	scalar_record large_input;
	for (int index = 0; index < 15; ++index)
		large_input.emplace(std::to_string(index), std::string(4096, char('a' + index)));
	auto marshalled = marshalling->call("echo", large_input);
	CHECK(!marshalled && marshalled.error().code == error_code::memory_limit &&
	      marshalling->faulted());
	auto spinning = prepare(working_source, {256 * 1024, 1000, 256 * 1024});
	CHECK(spinning);
	auto stopped = spinning->call("spin", {});
	CHECK(!stopped && stopped.error().code == error_code::instruction_limit && spinning->faulted());
	// Recovery is fresh ownership, rather than clearing a flag on a partially mutated VM.
	auto replacement = prepare("function empty(input) return {recovered = true} end");
	CHECK(replacement);
	*spinning = std::move(*replacement);
	CHECK(!spinning->faulted() && replacement->faulted() && replacement->memory_bytes() == 0);
	CHECK(replacement->source_uri().empty());
	CHECK(replacement->call("empty", {}).error().code == error_code::faulted);
	CHECK(std::get<bool>(spinning->call("empty", {})->at("recovered")));
	CHECK(program->call("update", input));
	std::puts(
	    "Lua passed: protected source/calls, scalar boundaries, budgets, fault/reload ownership");
	return 0;
}
