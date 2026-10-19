#include "ice/script/lua_program.h"
#include <algorithm>
#include <cstdio>

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::fprintf(stderr, "Lua environment check %d: %s\n", __LINE__, #condition);          \
			return 1;                                                                              \
		}                                                                                          \
	} while (false)

namespace {
struct source_handler final : ice::io::virtual_scheme_handler {
	std::string                   source;
	ice::io::resource_read_result read(const ice::io::resource_uri &,
	                                   std::vector<unsigned char> &bytes, std::string &) override {
		bytes.assign(source.begin(), source.end());
		return ice::io::resource_read_result::success;
	}
};
} // namespace
int main() {
	using namespace ice::script;
	auto description = describe_lua_environment();
	CHECK(description && description->host_scope == "entry_call_only");
	CHECK(description->release.starts_with(description->version + "."));
	CHECK(description->libraries.size() == 5);
	CHECK(description->libraries.at("table").at("remove") == "function");
	CHECK(description->guarded.contains("table.remove"));
	for (const auto *name :
	     {"table.sort", "table.insert", "table.move", "string.find", "string.match",
	      "string.gmatch", "string.gsub", "io", "os", "package", "require"})
		CHECK(std::ranges::find(description->removed, name) != description->removed.end());
	// Compare every advertised type AND every actual member, before user functions
	// or call-scoped host entries can add globals. The inventory is not a golden list.
	std::string source     = "local expected={";
	std::size_t advertised = 0;
	for (const auto &[library, members] : description->libraries) {
		source += "['" + library + "']={";
		for (const auto &[name, type] : members) {
			source += "['" + name + "']='" + type + "',";
			++advertised;
		}
		source += "},";
	}
	source += R"lua(}
local checked=0
for library,members in pairs(expected) do
  local actual=_G[library]
  for name,kind in pairs(members) do assert(type(actual[name])==kind,library..'.'..name) end
  for name,member in pairs(actual) do
    assert(members[name]==type(member),'unadvertised '..library..'.'..name)
    checked=checked+1
  end
end
assert(host==nil,'host exposed during source initialization')
)lua";
	for (const auto &name : description->removed)
		source += "assert(" + name + "==nil,'removed " + name + "')\n";
	source += "assert(_VERSION=='" + description->version + "')\n";
	source += R"lua(
function run(input)
  assert(type(host.echo)=='function')
  local values={3,1,2}
  assert(table.remove(values,2)==1 and #values==2 and values[2]==2)
  assert(table.concat({'a','b'})=='ab' and string.upper('ok')=='OK')
  local echoed=host.echo{count=checked}
  return {count=echoed.count,version=_VERSION}
end
)lua";
	auto provider    = std::make_shared<source_handler>();
	provider->source = source;
	ice::io::resource_resolver resolver;
	resolver.register_handler("memory", provider);
	auto program = lua_program::prepare(resolver, "memory://environment.lua");
	if (!program)
		std::fprintf(stderr, "%s\n", program.error().message.c_str());
	CHECK(program);
	unsigned            calls = 0;
	const host_function echo{
	    "echo", [&](const scalar_record &input) -> ice::expected<scalar_record, diagnostic> {
		    ++calls;
		    return input;
	    }};
	auto result = program->call("run", {}, std::span(&echo, 1));
	CHECK(result && calls == 1 &&
	      std::get<std::int64_t>(result->at("count")) == std::int64_t(advertised));
	CHECK(std::get<std::string>(result->at("version")) == description->version);
	// These are actual attempted invocations, not just type checks.
	for (const auto *expression :
	     {"table.sort({2,1})", "table.insert({},1)", "table.move({},1,1,1)", "string.find('a','a')",
	      "io.open('forbidden')"}) {
		provider->source = std::string("function run(i) ") + expression + " return {} end";
		auto denied      = lua_program::prepare(resolver, "memory://denied.lua");
		CHECK(denied);
		auto failed = denied->call("run", {});
		CHECK(!failed && failed.error().code == error_code::runtime && denied->faulted());
	}
	// Host closure names resolve against the current call; prior capabilities expire.
	provider->source = "assert(host==nil) local saved function run(i) if saved then return saved{} "
	                   "else saved=host.echo return {} end end";
	auto retained    = lua_program::prepare(resolver, "memory://retained.lua");
	CHECK(retained && retained->call("run", {}, std::span(&echo, 1)));
	CHECK(!retained->call("run", {}) && calls == 1);
	std::printf(
	    "Lua environment passed: %zu actual members, removals, guarded operations and host scope\n",
	    advertised);
}
