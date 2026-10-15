#include "ice/commands/service.h"
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

#define CHECK(expression)                                                                          \
	do {                                                                                           \
		if (!(expression)) {                                                                       \
			std::fprintf(stderr, "Commands failed %d: %s\n", __LINE__, #expression);               \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

namespace {
using namespace ice::commands;
command_descriptor descriptor(std::string id, effect behavior = effect::read_only) {
	return {id, id, "Test command", std::nullopt, behavior};
}
ice::meta::schema_descriptor number_schema() {
	ice::meta::schema_descriptor schema;
	schema.id      = "test.number";
	schema.name    = "number";
	schema.version = 1;
	ice::meta::field_descriptor value;
	value.id      = 1;
	value.name    = "value";
	value.kind    = ice::meta::scalar_kind::float32;
	value.minimum = 0;
	value.maximum = 10;
	schema.fields.push_back(value);
	ice::meta::field_descriptor label;
	label.id            = 2;
	label.name          = "label";
	label.kind          = ice::meta::scalar_kind::string;
	label.has_default   = true;
	label.default_value = std::string("default");
	schema.fields.push_back(label);
	return schema;
}
request mutation(const service &owner, std::string id, std::string command) {
	return {std::move(id), std::move(command), {}, owner.identity(), owner.revision()};
}
} // namespace

int main() {
	using namespace ice::commands;
	static_assert(!std::is_copy_constructible_v<service>);
	static_assert(std::is_nothrow_move_constructible_v<service>);
	static_assert(std::is_same_v<decltype(std::declval<const service &>().discover()),
	                             std::span<const command_descriptor>>);
	int      calls             = 0;
	int      host              = 0;
	bool     fail              = false;
	bool     throws            = false;
	bool     noop              = false;
	bool     bad_output        = false;
	bool     bad_error         = false;
	bool     read_changes      = false;
	service *owner             = nullptr;
	status   recursive_status  = status::succeeded;
	auto     write_descriptor  = descriptor("set", effect::mutation);
	write_descriptor.arguments = number_schema();
	std::vector<registration> registrations;
	registrations.push_back(
	    {descriptor("read"),
	     [&](const ice::meta::record &) -> ice::expected<handler_result, diagnostic> {
		     ++calls;
		     return handler_result{
		         read_changes, {{"host", std::int64_t(host)}, {"unicode", std::string("玩家 🎮")}}};
	     }});
	registrations.push_back(
	    {write_descriptor,
	     [&](const ice::meta::record &arguments) -> ice::expected<handler_result, diagnostic> {
		     ++calls;
		     if (fail)
			     return ice::unexpected(
			         diagnostic{bad_error ? std::string(4097, 'x') : "Rejected by host", 1});
		     if (throws) {
			     ++host; // Deliberately violate transactional host behavior to prove invalidation.
			     throw std::runtime_error("not exposed through the service");
		     }
		     if (!noop)
			     ++host;
		     if (bad_output)
			     return handler_result{!noop,
			                           {{"invalid", std::numeric_limits<double>::quiet_NaN()}}};
		     return handler_result{!noop, {{"value", arguments.at(1)}, {"label", arguments.at(2)}}};
	     }});
	registrations.push_back(
	    {descriptor("recursive"),
	     [&](const ice::meta::record &) -> ice::expected<handler_result, diagnostic> {
		     ++calls;
		     recursive_status = owner->invoke({"inside", "read", {}, {}, {}})->status;
		     return handler_result{};
	     }});
	auto created = service::create(registrations);
	CHECK(created);
	service live = std::move(*created);
	owner        = &live;
	CHECK(live.identity().size() == 32 && live.revision() == 0 && live.discover().size() == 3);
	CHECK(created->identity().empty() && created->discover().empty());
	CHECK(created->invoke({"moved", "read", {}, {}, {}})->status == status::service_unavailable);
	registrations[0].descriptor.description = "Changed after creation";
	CHECK(live.discover()[0].description == "Test command");
	auto other = service::create({});
	CHECK(other && other->identity() != live.identity());

	const request read{"read1", "read", {}, {}, {}};
	auto          initial = live.invoke(read);
	CHECK(initial->status == status::succeeded && initial->fields.size() == 2 && calls == 1);
	CHECK(!initial->diagnostic && !initial->changed && initial->revision == 0);
	CHECK(live.invoke(read) == initial && calls == 1);
	auto conflict       = read;
	conflict.command_id = "set";
	CHECK(live.invoke(conflict)->status == status::operation_conflict && calls == 1);
	CHECK(live.invoke({"unknown", "missing", {}, {}, {}})->status == status::unknown_command);
	CHECK(live.invoke({"bad token", "read", {}, {}, {}})->status == status::invalid_request);
	CHECK(live.invoke({"pair", "read", {}, live.identity(), {}})->status ==
	      status::invalid_request);
	CHECK(live.invoke({"noargs", "read", {{1, true}}, {}, {}})->status == status::invalid_request);
	CHECK(live.invoke({"unguarded", "set", {{1, 2.8}}, {}, {}})->status == status::invalid_request);
	auto wrong_session      = mutation(*other, "foreign", "set");
	wrong_session.arguments = {{1, 2.8}};
	CHECK(live.invoke(wrong_session)->status == status::stale_session && calls == 1);
	auto missing         = mutation(live, "missing", "set");
	auto missing_receipt = live.invoke(missing);
	CHECK(missing_receipt->status == status::invalid_request &&
	      missing_receipt->diagnostic->field == 1);
	CHECK(live.invoke(missing) == missing_receipt && calls == 1);
	auto invalid      = mutation(live, "invalid", "set");
	invalid.arguments = {{1, 11.0}};
	CHECK(live.invoke(invalid)->status == status::invalid_request && calls == 1);
	invalid.operation_id = "wrong-type";
	invalid.arguments    = {{1, std::int64_t(2)}};
	CHECK(live.invoke(invalid)->status == status::invalid_request && calls == 1);
	invalid.operation_id = "nan";
	invalid.arguments    = {{1, std::numeric_limits<double>::quiet_NaN()}};
	auto nan_result      = live.invoke(invalid);
	CHECK(nan_result->status == status::invalid_request && live.invoke(invalid) == nan_result);
	invalid.operation_id = "utf8";
	invalid.arguments    = {{1, 2.8}, {2, std::string("x\0y", 3)}};
	CHECK(live.invoke(invalid)->status == status::invalid_request && calls == 1);
	invalid.operation_id = "unknown-field";
	invalid.arguments    = {{1, 2.8}, {3, true}};
	CHECK(live.invoke(invalid)->status == status::invalid_request && calls == 1);

	auto set      = mutation(live, "set1", "set");
	set.arguments = {{1, 2.8}};
	auto applied  = live.invoke(set);
	CHECK(applied->status == status::succeeded && applied->changed && applied->revision == 1);
	CHECK(calls == 2 && host == 1 && live.revision() == 1);
	CHECK(std::get<double>(applied->fields.at("value")) == double(2.8f));
	CHECK(std::get<std::string>(applied->fields.at("label")) == "default");
	CHECK(live.invoke(set) == applied && calls == 2); // Deduplication precedes stale guards.
	CHECK(live.invoke(read) == initial && initial->revision == 0);
	set.operation_id = "stale";
	CHECK(live.invoke(set)->status == status::stale_revision && calls == 2);
	auto current      = mutation(live, "noop", "set");
	current.arguments = {{1, 3.0}};
	noop              = true;
	auto unchanged    = live.invoke(current);
	CHECK(unchanged->status == status::succeeded && !unchanged->changed && live.revision() == 1);
	noop                 = false;
	fail                 = true;
	current.operation_id = "host-failed";
	auto failed          = live.invoke(current);
	CHECK(failed->status == status::handler_failed && failed->diagnostic->field == 1);
	CHECK(!failed->inspect_required && live.revision() == 1 && host == 1);
	CHECK(live.invoke(current) == failed && calls == 4);
	fail                 = false;
	throws               = true;
	current.operation_id = "host-threw";
	auto threw           = live.invoke(current);
	CHECK(threw->status == status::handler_exception && threw->inspect_required);
	CHECK(!threw->changed && threw->revision == 2 && host == 2 && calls == 5);
	CHECK(live.invoke(current) == threw && calls == 5);
	throws               = false;
	current.operation_id = "after-exception-stale";
	CHECK(live.invoke(current)->status == status::stale_revision && calls == 5);
	current           = mutation(live, "bad-output", "set");
	current.arguments = {{1, 3.0}};
	bad_output        = true;
	auto untrusted    = live.invoke(current);
	CHECK(untrusted->status == status::invalid_output && untrusted->inspect_required);
	CHECK(untrusted->fields.empty() && live.revision() == 3 && host == 3);
	bad_output = false;
	fail = bad_error  = true;
	current           = mutation(live, "bad-error", "set");
	current.arguments = {{1, 3.0}};
	CHECK(live.invoke(current)->inspect_required && live.revision() == 4);
	fail = bad_error = false;
	read_changes     = true;
	CHECK(live.invoke({"bad-read", "read", {}, {}, {}})->status == status::invalid_output);
	CHECK(live.revision() == 5);
	read_changes   = false;
	auto recursive = live.invoke({"recursive1", "recursive", {}, {}, {}});
	CHECK(recursive->status == status::succeeded && recursive_status == status::service_busy);
	CHECK(live.revision() == 5);

	// The bounded cache forgets old IDs, but stale mutation guards still prevent old effects.
	for (unsigned index = 0; index < 64; ++index)
		CHECK(live.invoke({"cache" + std::to_string(index), "read", {}, {}, {}})->status ==
		      status::succeeded);
	auto previous_calls = calls;
	CHECK(live.invoke(read) != initial && calls == previous_calls + 1);
	CHECK(live.invoke(set)->status == status::stale_revision && calls == previous_calls + 1);
	CHECK(initial->fields.size() == 2 && initial->revision == 0);
	const auto identity = live.identity();
	service    moved    = std::move(live);
	CHECK(moved.identity() == identity && moved.revision() == 5);
	// The moved-from query is an explicit part of the public ownership contract.
	CHECK(live.identity().empty()); // NOLINT(bugprone-use-after-move)

	// Reject ownership budgets before copying large authored requests into the bounded cache.
	request large{"large", "read", {}, {}, {}};
	large.operation_id.assign(129, 'x');
	CHECK(moved.invoke(large)->status == status::invalid_request);
	large.operation_id = "large";
	for (ice::meta::field_id id = 1; id <= 17; ++id)
		large.arguments[id] = std::string(4096, 'x');
	CHECK(moved.invoke(large)->status == status::invalid_request);

	// Discovery and callback ownership are fixed at session creation.
	CHECK(!service::create({registrations[0], registrations[0]}));
	auto bad    = registrations[0];
	bad.execute = {};
	CHECK(!service::create({bad}));
	bad = registrations[0];
	bad.descriptor.description.assign(4097, 'x');
	CHECK(!service::create({bad}));
	bad                                    = registrations[1];
	bad.descriptor.arguments->fields[0].id = 0;
	CHECK(!service::create({bad}));
	bad                                   = registrations[1];
	bad.descriptor.arguments->tags["bad"] = std::string("\xFF");
	CHECK(!service::create({bad}));
	std::weak_ptr<int>             lifetime;
	std::shared_ptr<const receipt> retained;
	{
		auto dependency = std::make_shared<int>(42);
		lifetime        = dependency;
		auto scoped     = service::create(
            {{descriptor("owned"),
		          [dependency](const ice::meta::record &) -> ice::expected<handler_result, diagnostic> {
                  return handler_result{false, {{"answer", std::int64_t(*dependency)}}};
              }}});
		CHECK(scoped);
		dependency.reset();
		CHECK(!lifetime.expired());
		retained = scoped->invoke({"retained", "owned", {}, {}, {}});
	}
	CHECK(lifetime.expired() && std::get<std::int64_t>(retained->fields.at("answer")) == 42);
	// Host output has an independently checked, bounded named scalar contract.
	output   emitted;
	unsigned output_calls = 0;
	auto     outputs      = service::create(
        {{descriptor("output"),
	               [&](const ice::meta::record &) -> ice::expected<handler_result, diagnostic> {
              ++output_calls;
              return handler_result{false, emitted};
          }}});
	CHECK(outputs);
	emitted   = {{"signed", std::int64_t(INT64_MIN)},
	             {"unsigned", std::uint64_t(UINT64_MAX)},
	             {"null", {}},
	             {"boolean", true},
	             {"float", 1e100}};
	auto wide = outputs->invoke({"wide", "output", {}, {}, {}});
	CHECK(wide->status == status::succeeded && wide->revision == 0);
	CHECK(std::get<std::uint64_t>(wide->fields.at("unsigned")) == UINT64_MAX);
	emitted = {{"nul", std::string("x\0y", 3)}};
	CHECK(outputs->invoke({"nul", "output", {}, {}, {}})->inspect_required);
	emitted = {{"long", std::string(4097, 'x')}};
	CHECK(outputs->invoke({"long", "output", {}, {}, {}})->inspect_required);
	emitted = {{"", true}};
	CHECK(outputs->invoke({"empty", "output", {}, {}, {}})->inspect_required);
	emitted.clear();
	for (unsigned index = 0; index < 129; ++index)
		emitted[std::to_string(index)] = true;
	CHECK(outputs->invoke({"fields", "output", {}, {}, {}})->inspect_required);
	emitted.clear();
	for (unsigned index = 0; index < 17; ++index)
		emitted[std::to_string(index)] = std::string(4096, 'x');
	CHECK(outputs->invoke({"bytes", "output", {}, {}, {}})->inspect_required);
	CHECK(output_calls == 6 && outputs->revision() == 5);
	CHECK(wide->fields.size() == 5 && wide->revision == 0);
	request zero{"zero", "output", {{1, 0.0}}, {}, {}};
	auto    zero_rejected = outputs->invoke(zero);
	CHECK(zero_rejected->status == status::invalid_request);
	zero.arguments[1] = -0.0;
	CHECK(outputs->invoke(zero)->status == status::operation_conflict && output_calls == 6);

	// Default expansion must fit the invocation budget too, before callback entry.
	auto defaults_descriptor      = descriptor("defaults");
	defaults_descriptor.arguments = number_schema();
	defaults_descriptor.arguments->fields.clear();
	for (ice::meta::field_id id = 1; id <= 17; ++id) {
		ice::meta::field_descriptor field;
		field.id            = id;
		field.name          = "text" + std::to_string(id);
		field.kind          = ice::meta::scalar_kind::string;
		field.has_default   = true;
		field.default_value = std::string(4096, 'x');
		defaults_descriptor.arguments->fields.push_back(std::move(field));
	}
	bool defaults_called = false;
	auto expanded        = service::create(
        {{std::move(defaults_descriptor),
	             [&](const ice::meta::record &) -> ice::expected<handler_result, diagnostic> {
              defaults_called = true;
              return handler_result{};
          }}});
	CHECK(expanded);
	CHECK(expanded->invoke({"defaults", "defaults", {}, {}, {}})->status ==
	      status::invalid_request);
	CHECK(!defaults_called && expanded->revision() == 0);
	std::vector<registration> too_many(129, registrations[0]);
	CHECK(!service::create(std::move(too_many)));
	std::puts(
	    "Commands passed: immutable discovery, scalar/default validation, session guards, "
	    "no-op/revision semantics, bounded exact retries, host failures/exceptions and lifetime");
}
