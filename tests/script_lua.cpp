#include "ice/script/lua_program.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <set>
#include <stdexcept>
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
  assert(string.dump == nil and type(string.format) == 'function' and string.find == nil)
  assert(string.match == nil and string.gmatch == nil and string.gsub == nil)
  assert(string.pack == nil and type(math.random) == 'function' and type(math.randomseed) == 'function')
  assert(table.move == nil and table.insert == nil and type(table.remove) == 'function' and table.sort == nil)
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
	const std::string working_source = memory->source;
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
	const std::string format_source = R"lua(
function run(input)
  local values = {}
  for i=1,input.count or 0 do values[i] = input.value end
  return {text=string.format(input.format,table.unpack(values))}
end
function rgb(input) return {text=string.format('#%02x%02x%02x',0,127,255)} end
function scalars(input)
  return {text=string.format('%s %d %.2f %q %%',42,-7,1.25,'a"b')}
end
function binary(input)
  return {size=#string.format('%s',string.char(0,255))}
end
function bad_table(input) return {text=string.format('%s',{})} end
function bad_function(input) return {text=string.format('%s',function() end)} end
function bad_boolean(input) return {text=string.format('%s',true)} end
function bad_nil(input) return {text=string.format('%s',nil)} end
function no_format(input) return {text=string.format()} end
function numeric_format(input) return {text=string.format(42)} end
function max_values(input)
  local t={} for i=1,32 do t[i]=1 end
  return {text=string.format(string.rep('%d',32),table.unpack(t))}
end
function too_many(input)
  local t={} for i=1,33 do t[i]=1 end
  return {text=string.format(string.rep('%d',33),table.unpack(t))}
end
function long_format(input) return {text=string.format(string.rep('x',4097))} end
function long_value(input) return {text=string.format('%s',string.rep('x',4097))} end
function long_output(input) return {text=string.format('x%s',string.rep('x',4096))} end
function quoted_output(input) return {text=string.format('%q',string.rep('\0',4096))} end
function allocate(input)
  local t={} for i=1,32 do t[i]=string.rep('x',4096) end
  return {text=string.format(string.rep('%s',32),table.unpack(t))}
end
)lua";
	auto              formatting    = prepare(format_source);
	CHECK(formatting);
	CHECK(std::get<std::string>(formatting->call("rgb", {})->at("text")) == "#007fff");
	CHECK(std::get<std::string>(formatting->call("scalars", {})->at("text")) ==
	      "42 -7 1.25 \"a\\\"b\" %");
	CHECK(std::get<std::int64_t>(formatting->call("binary", {})->at("size")) == 2);
	CHECK(std::get<std::string>(formatting->call("max_values", {})->at("text")) ==
	      std::string(32, '1'));
	CHECK(std::get<std::string>(
	          formatting->call("run", {{"format", std::string(4096, 'x')}})->at("text")) ==
	      std::string(4096, 'x'));
	CHECK(std::get<std::string>(formatting
	                                ->call("run", {{"format", std::string("%s")},
	                                               {"count", std::int64_t(1)},
	                                               {"value", std::string(4096, 'x')}})
	                                ->at("text")) == std::string(4096, 'x'));
	CHECK(std::get<std::string>(formatting
	                                ->call("run", {{"format", std::string(64, '%')},
	                                               {"count", std::int64_t(32)},
	                                               {"value", std::int64_t(1)}})
	                                ->at("text")) == std::string(32, '%'));
	for (const auto &format : {"%p", "%99p", "%100d", "%.100f", "%2147483647d", "%n", "%"}) {
		auto candidate = prepare(format_source, {64 * 1024, 10000, 256 * 1024});
		CHECK(candidate);
		auto rejected = candidate->call("run", {{"format", std::string(format)},
		                                        {"count", std::int64_t(1)},
		                                        {"value", std::string("private")}});
		CHECK(!rejected && rejected.error().code == error_code::runtime && candidate->faulted());
	}
	for (const auto &function :
	     {"bad_table", "bad_function", "bad_boolean", "bad_nil", "no_format", "numeric_format",
	      "too_many", "long_format", "long_value", "long_output", "quoted_output"}) {
		auto candidate = prepare(format_source, {64 * 1024, 10000, 256 * 1024});
		CHECK(candidate);
		auto rejected = candidate->call(function, {});
		CHECK(!rejected && rejected.error().code == error_code::runtime && candidate->faulted());
	}
	auto format_memory = prepare(format_source, {64 * 1024, 10000, 256 * 1024});
	CHECK(format_memory);
	auto format_allocation = format_memory->call("allocate", {});
	CHECK(!format_allocation && format_allocation.error().code == error_code::memory_limit &&
	      format_memory->faulted() && format_memory->memory_bytes() <= 64 * 1024);

	const std::string remove_source = R"lua(
function normal(input)
  local t = {'a','b','c','d','e'}
  assert(table.remove(t,3) == 'c' and table.concat(t) == 'abde')
  assert(table.remove(t,1) == 'a' and table.concat(t) == 'bde')
  assert(table.remove(t,#t) == 'e' and table.concat(t) == 'bd')
  assert(table.remove(t) == 'd' and table.remove(t,nil) == 'b' and #t == 0)
  assert(table.remove(t) == nil and table.remove(t,1) == nil and table.remove(t,0) == nil)
  t[0] = 'zero'
  assert(table.remove(t) == 'zero' and t[0] == nil)
  t = {'a','b'}
  assert(table.remove(t,3) == nil and table.concat(t) == 'ab')
  assert(table.remove(t,'2') == 'b' and table.remove(t,1.0,'ignored') == 'a')
  local value = {}
  t = {false,value,17}
  assert(table.remove(t,2) == value and t[2] == 17)
  assert(table.remove(t,1) == false and t[1] == 17)
  local particles = {{life=0},{life=2},{life=0},{life=4}}
  for i=#particles,1,-1 do
    if particles[i].life == 0 then table.remove(particles,i) end
  end
  return {count=#particles,first=particles[1].life,last=particles[2].life}
end
function remove(input) return {value=table.remove({'a','b','c'},input.position)} end
function bad_table(input) table.remove('not a table'); return {} end
local function sparse()
  -- The pinned Lua 5.5 hash-border search follows this fixed VM hash seed.
  -- A real maxinteger border is represented by at most 64 entries, not a large heap.
  local t = table.create(0,128)
  t[1] = true
  local index,bits = 2,0x494345
  while true do
    t[index] = true
    if index == math.maxinteger then break end
    if index <= math.maxinteger//2-1 then index=index*2+(bits&1); bits=bits>>1
    else index=math.maxinteger end
  end
  assert(#t == math.maxinteger)
  return t
end
function huge(input)
  table.remove(sparse(),1)
  return {escaped=true}
end
function huge_last(input)
  assert(table.remove(sparse()) == true)
  return {removed=true}
end
function repeated(input)
  local t={} for i=1,200 do t[i]=i end
  for i=1,200 do table.remove(t,1) end
  return {count=#t}
end
function memory(input)
  local retained = {}
  while true do
    local t = {string.rep('x',4096),string.rep('y',4096),string.rep('z',4096)}
    retained[#retained+1] = table.remove(t,2)
  end
end
)lua";
	auto              removing      = prepare(remove_source);
	CHECK(removing);
	auto removed = removing->call("normal", {});
	CHECK(removed && std::get<std::int64_t>(removed->at("count")) == 2 &&
	      std::get<std::int64_t>(removed->at("first")) == 2 &&
	      std::get<std::int64_t>(removed->at("last")) == 4);
	for (const auto &position :
	     {ice::meta::value{std::int64_t(-1)}, ice::meta::value{std::int64_t(0)},
	      ice::meta::value{std::int64_t(5)},
	      ice::meta::value{std::numeric_limits<std::int64_t>::min()},
	      ice::meta::value{std::numeric_limits<std::int64_t>::max()}, ice::meta::value{1.5},
	      ice::meta::value{true}, ice::meta::value{std::string("invalid")}}) {
		auto candidate = prepare(remove_source);
		CHECK(candidate);
		auto rejected = candidate->call("remove", {{"position", position}});
		CHECK(!rejected && rejected.error().code == error_code::runtime && candidate->faulted());
	}
	auto wrong_remove = prepare(remove_source);
	CHECK(wrong_remove);
	CHECK(wrong_remove->call("bad_table", {}).error().code == error_code::runtime);
	auto sparse_last = prepare(remove_source, {64 * 1024, 10000, 256 * 1024});
	CHECK(sparse_last);
	auto last_removed = sparse_last->call("huge_last", {});
	CHECK(last_removed && std::get<bool>(last_removed->at("removed")));
	auto startup_remove = prepare(remove_source + "\nhuge({})", {64 * 1024, 10000, 256 * 1024});
	CHECK(!startup_remove && startup_remove.error().code == error_code::instruction_limit &&
	      startup_remove.error().message.find("table.remove") != std::string::npos);
	for (const auto *entry : {"huge", "repeated"}) {
		auto limited = prepare(remove_source, {64 * 1024, 10000, 256 * 1024});
		CHECK(limited);
		auto rejected = limited->call(entry, {});
		CHECK(!rejected && rejected.error().code == error_code::instruction_limit &&
		      limited->faulted());
		CHECK(rejected.error().message.find("table.remove") != std::string::npos);
		CHECK(limited->memory_bytes() <= 64 * 1024);
	}
	auto remove_memory = prepare(remove_source, {64 * 1024, 1000000, 256 * 1024});
	CHECK(remove_memory);
	auto memory_rejected = remove_memory->call("memory", {});
	CHECK(!memory_rejected && memory_rejected.error().code == error_code::memory_limit &&
	      remove_memory->faulted() && remove_memory->memory_bytes() <= 64 * 1024);
	CHECK(remove_memory->call("normal", {}).error().code == error_code::faulted);
	auto remove_replacement = prepare(remove_source);
	CHECK(remove_replacement && remove_replacement->call("normal", {}));

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
	auto syntax = prepare("function unfinished(");
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
	CHECK(!prepare("return", {64 * 1024, 1000000001, 100}));
	CHECK(!prepare("return", {1024 * 1024 * 1024 + 1, 100, 100}));
	CHECK(prepare("return", {1024 * 1024 * 1024, 1000000000, 100, 1000000}));
	CHECK(lua_limits{}.memory_bytes == 64 * 1024 * 1024 && lua_limits{}.instructions == 10000000 &&
	      lua_limits{}.host_calls == 65536);
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
	// Host functions are generic scalar capabilities. This collector knows commands;
	// the VM knows nothing about shapes, circles, colors or scene publication.
	std::vector<scalar_record>         events;
	const std::array<host_function, 1> collecting{
	    {{"invoke",
	      [&](const scalar_record &arguments) -> ice::expected<scalar_record, diagnostic> {
		      events.push_back(arguments);
		      return scalar_record{{"accepted", true}, {"index", std::int64_t(events.size())}};
	      }}}};
	const std::string host_source = R"lua(
local saved
function circle(input)
  for i=0,19 do
    local angle = i*math.pi*2/20
    local result = host.invoke{command='drop', shape='teapot', x=3*math.cos(angle), y=3, z=3*math.sin(angle)}
    assert(result.accepted)
    local r=math.floor(127.5+127.5*math.sin(angle))
    local g=math.floor(127.5+127.5*math.sin(angle+2.094395102))
    local b=math.floor(127.5+127.5*math.sin(angle+4.188790205))
    host.invoke{command='color', value=string.format('#%02x%02x%02x',r,g,b)}
  end
  return {count=20}
end
function save(input) saved=host.invoke; return saved(input) end
function retained(input) return saved(input) end
function once(input) return host.invoke(input) end
function many(input) for i=1,input.count do host.invoke{index=i} end return {} end
function invalid(input) return host.invoke{nested={}} end
function extras(input) return host.invoke({}, {}) end
function bad_final(input) host.invoke(input); return {bad={}} end
function empty(input) return {} end
)lua";
	auto              hosted      = prepare(host_source);
	CHECK(hosted && hosted->call("circle", {}, collecting));
	CHECK(events.size() == 40);
	std::set<std::string> colors;
	for (std::size_t index = 0; index < 20; ++index) {
		const auto &drop = events[index * 2];
		CHECK(std::get<std::string>(drop.at("command")) == "drop");
		CHECK(std::get<std::string>(drop.at("shape")) == "teapot");
		const auto x = std::get<double>(drop.at("x")), z = std::get<double>(drop.at("z"));
		CHECK(std::abs(x * x + z * z - 9) < 1e-9 && std::get<std::int64_t>(drop.at("y")) == 3);
		colors.insert(std::get<std::string>(events[index * 2 + 1].at("value")));
	}
	CHECK(colors.size() == 20);
	// A retained closure resolves the new call's grant, never the old callback.
	int first_calls = 0, second_calls = 0;
	{
		const std::array<host_function, 1> first{
		    {{"invoke",
		      [&](const scalar_record &arguments) -> ice::expected<scalar_record, diagnostic> {
			      ++first_calls;
			      return arguments;
		      }}}};
		CHECK(hosted->call("save", {{"value", std::int64_t(7)}}, first));
	}
	const std::array<host_function, 1> second{
	    {{"invoke",
	      [&](const scalar_record &arguments) -> ice::expected<scalar_record, diagnostic> {
		      ++second_calls;
		      return arguments;
	      }}}};
	CHECK(hosted->call("retained", {{"value", std::int64_t(8)}}, second));
	CHECK(first_calls == 1 && second_calls == 1);
	auto absent = hosted->call("retained", {});
	CHECK(!absent && absent.error().code == error_code::host_error && hosted->faulted());
	CHECK(first_calls == 1 && second_calls == 1);

	auto registry = prepare(host_source);
	CHECK(registry);
	const auto echo_host =
	    [](const scalar_record &arguments) -> ice::expected<scalar_record, diagnostic> {
		return arguments;
	};
	const std::array<host_function, 2> duplicate{{{"invoke", echo_host}, {"invoke", echo_host}}};
	CHECK(registry->call("empty", {}, duplicate).error().code == error_code::invalid_input);
	const std::array<host_function, 1> bad_name{{{"not.valid", echo_host}}};
	CHECK(registry->call("empty", {}, bad_name).error().code == error_code::invalid_input);
	const std::array<host_function, 1> no_handler{{{"invoke", {}}}};
	CHECK(registry->call("empty", {}, no_handler).error().code == error_code::invalid_input);
	std::vector<host_function> too_many;
	for (unsigned index = 0; index < 33; ++index)
		too_many.push_back({"function_" + std::to_string(index), echo_host});
	CHECK(registry->call("empty", {}, too_many).error().code == error_code::invalid_input);
	CHECK(!registry->faulted() && registry->call("empty", {}));
	for (const auto &function : {"invalid", "extras"}) {
		auto candidate = prepare(host_source);
		CHECK(candidate);
		auto invalid_host = candidate->call(function, {}, second);
		CHECK(!invalid_host && invalid_host.error().code == error_code::host_error &&
		      candidate->faulted());
	}
	CHECK(second_calls == 1); // Bad Lua records never invoke the host.
	const std::array<host_function, 1> throwing{
	    {{"invoke", [](const scalar_record &) -> ice::expected<scalar_record, diagnostic> {
		      throw std::runtime_error("private exception");
	      }}}};
	auto exception = prepare(host_source);
	CHECK(exception);
	auto exception_result = exception->call("once", {}, throwing);
	CHECK(!exception_result && exception_result.error().code == error_code::host_error &&
	      exception_result.error().message.find("private exception") == std::string::npos);
	const std::array<host_function, 1> rejected{
	    {{"invoke", [](const scalar_record &) -> ice::expected<scalar_record, diagnostic> {
		      return ice::unexpected(
		          diagnostic{error_code::invalid_input, "ignored://host", "Rejected candidate"});
	      }}}};
	auto rejected_program = prepare(host_source);
	CHECK(rejected_program);
	auto rejected_result = rejected_program->call("once", {}, rejected);
	CHECK(!rejected_result && rejected_result.error().code == error_code::host_error &&
	      rejected_result.error().uri == uri &&
	      rejected_result.error().message == "Rejected candidate");
	const std::array<host_function, 1> bad_record{
	    {{"invoke", [](const scalar_record &) -> ice::expected<scalar_record, diagnostic> {
		      return scalar_record{{"bad", std::monostate{}}};
	      }}}};
	auto bad_host = prepare(host_source);
	CHECK(bad_host &&
	      bad_host->call("once", {}, bad_record).error().code == error_code::host_error);
	int                                quota_calls = 0;
	const std::array<host_function, 1> counted{
	    {{"invoke", [&](const scalar_record &) -> ice::expected<scalar_record, diagnostic> {
		      ++quota_calls;
		      return scalar_record{};
	      }}}};
	auto quota = prepare(host_source, {256 * 1024, 100000, 256 * 1024, 128});
	CHECK(quota && quota->call("many", {{"count", std::int64_t(128)}}, counted));
	CHECK(quota_calls == 128);
	auto exceeded = quota->call("many", {{"count", std::int64_t(129)}}, counted);
	CHECK(!exceeded && exceeded.error().code == error_code::host_call_limit && quota_calls == 256);
	CHECK(quota->faulted());
	auto disabled = prepare(host_source, {256 * 1024, 100000, 256 * 1024, 0});
	CHECK(disabled &&
	      disabled->call("once", {}, counted).error().code == error_code::host_call_limit);
	CHECK(quota_calls == 256);
	CHECK(!prepare(host_source, {256 * 1024, 100000, 256 * 1024, 1000001}));
	auto generous = prepare(host_source);
	CHECK(generous && generous->call("many", {{"count", std::int64_t(129)}}, counted));
	CHECK(quota_calls == 385);
	// A failed final result cannot erase collector side effects. The app owns and
	// discards that candidate rather than publishing it on this failed call.
	events.clear();
	auto staged = prepare(host_source);
	CHECK(staged &&
	      staged->call("bad_final", {}, collecting).error().code == error_code::invalid_output);
	CHECK(events.size() == 1 && staged->faulted());
	bool                               reentry_rejected = false;
	auto                               reentrant        = prepare(host_source);
	const std::array<host_function, 1> recursive{
	    {{"invoke", [&](const scalar_record &) -> ice::expected<scalar_record, diagnostic> {
		      reentry_rejected = !reentrant->call("empty", {});
		      return scalar_record{};
	      }}}};
	CHECK(reentrant && reentrant->call("once", {}, recursive) && reentry_rejected &&
	      !reentrant->faulted());
	// Output marshalling can allocate in Lua but must not longjmp across callback owners.
	const std::array<host_function, 1> large_output{
	    {{"invoke", [&](const scalar_record &) -> ice::expected<scalar_record, diagnostic> {
		      return large_input;
	      }}}};
	auto host_memory =
	    prepare("function run(input) return host.invoke({}) end", {64 * 1024, 10000, 256 * 1024});
	CHECK(host_memory);
	auto host_allocation = host_memory->call("run", {}, large_output);
	CHECK(!host_allocation && host_allocation.error().code == error_code::memory_limit &&
	      host_memory->faulted());
	const std::string random_source = R"lua(
local startup = math.random()
function sample(input)
  if input.seed ~= nil then
    if input.second ~= nil then math.randomseed(input.seed,input.second)
    else math.randomseed(input.seed) end
  end
  return {startup=startup,unit=math.random(),die=math.random(6),byte=math.random(0,255),
          signed=math.random(0),full=math.random(math.mininteger,math.maxinteger)}
end
function seeds(input)
  local first,second=math.randomseed(input.first,input.second)
  return {first=first,second=second}
end
function extremes(input)
  return {low=math.random(math.mininteger,math.mininteger),
          high=math.random(math.maxinteger,math.maxinteger),one=math.random(1),
          negative=math.random(-17,-3)}
end
function generate(input)
  for i=1,input.count do
    host.invoke{command='drop',shape='teapot',x=math.random(-1000,1000)/100,
                y=2+5*math.random(),z=math.random(-1000,1000)/100}
    host.invoke{command='color',value=string.format('#%02x%02x%02x',
                math.random(0,255),math.random(0,255),math.random(0,255))}
  end
  return {count=input.count}
end
function empty(input) return {} end
)lua";
	auto              random_first = prepare(random_source), random_second = prepare(random_source);
	CHECK(random_first && random_second);
	auto default_first  = random_first->call("sample", {});
	auto default_second = random_second->call("sample", {});
	CHECK(default_first && default_second && *default_first == *default_second);
	const double unit = std::get<double>(default_first->at("unit"));
	CHECK(unit >= 0 && unit < 1);
	CHECK(std::get<std::int64_t>(default_first->at("die")) >= 1 &&
	      std::get<std::int64_t>(default_first->at("die")) <= 6);
	CHECK(std::get<std::int64_t>(default_first->at("byte")) >= 0 &&
	      std::get<std::int64_t>(default_first->at("byte")) <= 255);
	auto advanced_first  = random_first->call("sample", {});
	auto advanced_second = random_second->call("sample", {});
	CHECK(advanced_first && advanced_second && *advanced_first == *advanced_second &&
	      *advanced_first != *default_first);
	auto zero_seed =
	    random_first->call("sample", {{"seed", std::int64_t(0)}, {"second", std::int64_t(0)}});
	CHECK(zero_seed && std::get<double>(zero_seed->at("unit")) ==
	                       std::get<double>(default_first->at("startup")));
	const scalar_record seed{{"seed", std::int64_t(12345)}, {"second", std::int64_t(-6789)}};
	auto                seeded_first  = random_first->call("sample", seed);
	auto                seeded_again  = random_first->call("sample", seed);
	auto                seeded_second = random_second->call("sample", seed);
	CHECK(seeded_first && seeded_again && seeded_second && *seeded_first == *seeded_again &&
	      *seeded_first == *seeded_second);
	auto different = random_second->call("sample", {{"seed", std::int64_t(54321)}});
	CHECK(different && *different != *seeded_first);
	auto single_seed = random_first->call("sample", {{"seed", std::int64_t(12345)}});
	auto zero_second =
	    random_first->call("sample", {{"seed", std::int64_t(12345)}, {"second", std::int64_t(0)}});
	CHECK(single_seed && zero_second && *single_seed == *zero_second);
	auto returned_seeds =
	    random_first->call("seeds", {{"first", std::numeric_limits<std::int64_t>::min()},
	                                 {"second", std::numeric_limits<std::int64_t>::max()}});
	CHECK(returned_seeds &&
	      std::get<std::int64_t>(returned_seeds->at("first")) ==
	          std::numeric_limits<std::int64_t>::min() &&
	      std::get<std::int64_t>(returned_seeds->at("second")) ==
	          std::numeric_limits<std::int64_t>::max());
	auto extremes = random_first->call("extremes", {});
	CHECK(extremes &&
	      std::get<std::int64_t>(extremes->at("low")) == std::numeric_limits<std::int64_t>::min() &&
	      std::get<std::int64_t>(extremes->at("high")) ==
	          std::numeric_limits<std::int64_t>::max() &&
	      std::get<std::int64_t>(extremes->at("one")) == 1);
	CHECK(std::get<std::int64_t>(extremes->at("negative")) >= -17 &&
	      std::get<std::int64_t>(extremes->at("negative")) <= -3);
	events.clear();
	auto random_scene = prepare(random_source);
	CHECK(random_scene &&
	      random_scene->call("generate", {{"count", std::int64_t(500)}}, collecting));
	CHECK(events.size() == 1000);
	colors.clear();
	std::set<std::array<double, 3>> positions;
	for (std::size_t index = 0; index < 500; ++index) {
		const auto &drop = events[index * 2];
		CHECK(std::get<std::string>(drop.at("command")) == "drop" &&
		      std::get<std::string>(drop.at("shape")) == "teapot");
		const double x = std::get<double>(drop.at("x")), y = std::get<double>(drop.at("y")),
		             z = std::get<double>(drop.at("z"));
		CHECK(x >= -10 && x <= 10 && y >= 2 && y < 7 && z >= -10 && z <= 10);
		positions.insert({x, y, z});
		const auto &color = std::get<std::string>(events[index * 2 + 1].at("value"));
		CHECK(color.size() == 7 && color[0] == '#');
		colors.insert(color);
	}
	CHECK(positions.size() == 500 && colors.size() > 490);
	// Invalid native calls fault only their private VM; a separate working owner
	// keeps its stream/state and can still execute. Error handling stays protected.
	for (const auto &expression :
	     {"math.randomseed()", "math.randomseed(1,2,3)", "math.randomseed({})",
	      "math.randomseed(true)", "math.randomseed('1')", "math.randomseed(1.0)",
	      "math.randomseed(1,1.5)", "math.randomseed(1,nil)", "math.randomseed(1,{})",
	      "math.random(1,2,3)", "math.random({})", "math.random(true)", "math.random(1.5)",
	      "math.random(-1)", "math.random(3,2)"}) {
		auto invalid_random =
		    prepare(std::string("function run(input) ") + expression + " return {} end",
		            {64 * 1024, 1000, 4096});
		CHECK(invalid_random);
		auto invalid = invalid_random->call("run", {});
		CHECK(!invalid && invalid.error().code == error_code::runtime &&
		      invalid.error().uri == uri && invalid_random->faulted());
		CHECK(invalid_random->call("run", {}).error().code == error_code::faulted);
		CHECK(random_first->call("empty", {}) && !random_first->faulted());
	}
	std::puts(
	    "Lua passed: protected source/calls, scalar boundaries, budgets, fault/reload ownership");
	return 0;
}
