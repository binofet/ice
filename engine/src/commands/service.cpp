#include "ice/commands/service.h"
#include "ice/core/utf8.h"
#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <exception>
#include <limits>
#include <random>
#include <set>
#include <utility>

namespace ice::commands {
namespace {
constexpr std::size_t max_fields      = 128;
constexpr std::size_t max_text        = 4096;
constexpr std::size_t max_record_text = 64 * 1024;
constexpr std::size_t max_commands    = 128;
constexpr std::size_t max_receipts    = 64;

bool token(std::string_view text) {
	if (text.empty() || text.size() > 128)
		return false;
	return std::ranges::all_of(text, [](unsigned char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
		       c == '.' || c == '_' || c == ':' || c == '-';
	});
}
bool charge(std::size_t &remaining, std::size_t bytes) {
	if (bytes > remaining)
		return false;
	remaining -= bytes;
	return true;
}
bool text(std::string_view input, std::size_t &remaining, std::size_t limit = max_text) {
	return core::valid_utf8_text(input, limit) && charge(remaining, input.size());
}
bool scalar(const meta::value &input, std::size_t &remaining) {
	if (const auto *value = std::get_if<std::string>(&input))
		return text(*value, remaining);
	if (const auto *value = std::get_if<double>(&input))
		return std::isfinite(*value);
	return !input.valueless_by_exception();
}
bool metadata(const std::map<std::string, std::string> &tags, std::size_t &remaining) {
	if (tags.size() > max_fields)
		return false;
	for (const auto &[name, value] : tags)
		if (name.empty() || !text(name, remaining, 128) || !text(value, remaining))
			return false;
	return true;
}
bool descriptor_budget(const command_descriptor &descriptor, std::size_t &remaining) {
	if (!token(descriptor.id) || descriptor.name.empty() || descriptor.description.empty() ||
	    !text(descriptor.id, remaining, 128) || !text(descriptor.name, remaining, 128) ||
	    !text(descriptor.description, remaining) ||
	    (descriptor.effect != effect::read_only && descriptor.effect != effect::mutation))
		return false;
	if (!descriptor.arguments)
		return true;
	const auto &schema = *descriptor.arguments;
	if (schema.fields.size() > max_fields || !text(schema.id, remaining, 128) ||
	    !text(schema.name, remaining, 128) || !metadata(schema.tags, remaining))
		return false;
	for (const auto &field : schema.fields)
		if (!text(field.name, remaining, 128) || !metadata(field.tags, remaining) ||
		    !scalar(field.default_value, remaining))
			return false;
	return true;
}
bool bounded(const request &operation) {
	// Only ownership budgets are checked here. Bounded malformed values can still be remembered,
	// so exact retries of schema/UTF-8 failures do not change their original outcome.
	if (operation.operation_id.size() > 128 || operation.command_id.size() > 128 ||
	    operation.arguments.size() > max_fields ||
	    (operation.expected_session && operation.expected_session->size() > 128))
		return false;
	std::size_t remaining = max_record_text;
	for (const auto &[id, value] : operation.arguments) {
		(void)id;
		if (const auto *string = std::get_if<std::string>(&value))
			if (string->size() > max_text || !charge(remaining, string->size()))
				return false;
	}
	return true;
}
bool same_value(const meta::value &left, const meta::value &right) {
	if (left.index() != right.index())
		return false;
	// Compare authored floating-point bits: NaNs and signed zero must not make retry identity
	// ambiguous, even when validation will subsequently reject or normalize that scalar.
	if (const auto *number = std::get_if<double>(&left))
		return std::bit_cast<std::uint64_t>(*number) ==
		       std::bit_cast<std::uint64_t>(std::get<double>(right));
	return left == right;
}
bool same_request(const request &left, const request &right) {
	if (left.operation_id != right.operation_id || left.command_id != right.command_id ||
	    left.expected_session != right.expected_session ||
	    left.expected_revision != right.expected_revision ||
	    left.arguments.size() != right.arguments.size())
		return false;
	auto other = right.arguments.begin();
	for (const auto &[id, value] : left.arguments) {
		if (id != other->first || !same_value(value, other->second))
			return false;
		++other;
	}
	return true;
}
bool valid_output(const output &fields) {
	if (fields.size() > max_fields)
		return false;
	std::size_t remaining = max_record_text;
	for (const auto &[name, value] : fields)
		if (name.empty() || !text(name, remaining, 128) || !scalar(value, remaining))
			return false;
	return true;
}
std::string new_identity() {
	// The random process nonce avoids ordinary restart collisions; the non-wrapping counter
	// guarantees different service lifetimes within this process. This is not authentication.
	static const std::uint64_t nonce = [] {
		std::random_device random;
		return (std::uint64_t(random()) << 32) ^ std::uint64_t(random());
	}();
	static std::atomic<std::uint64_t> next{1};
	auto                              sequence = next.load(std::memory_order_relaxed);
	for (;;) {
		if (sequence == std::numeric_limits<std::uint64_t>::max())
			return {};
		if (next.compare_exchange_weak(sequence, sequence + 1, std::memory_order_relaxed))
			break;
	}
	constexpr std::string_view hex = "0123456789abcdef";
	std::string                result(32, '0');
	for (std::size_t i = 0; i < 16; ++i) {
		result[i]      = hex[(nonce >> ((15 - i) * 4)) & 15];
		result[16 + i] = hex[(sequence >> ((15 - i) * 4)) & 15];
	}
	return result;
}
} // namespace

struct service::implementation {
	struct remembered {
		request                  operation;
		std::shared_ptr<receipt> result;
	};
	std::string                     identity;
	std::uint64_t                   revision = 0;
	std::vector<command_descriptor> descriptors;
	std::vector<handler>            handlers;
	std::vector<remembered>         cache;
	bool                            invoking = false;
};

service::service(std::unique_ptr<implementation> state) : state_(std::move(state)) {}
service::~service()                                   = default;
service::service(service &&other) noexcept            = default;
service &service::operator=(service &&other) noexcept = default;

ice::expected<service, diagnostic> service::create(std::vector<registration> registrations) {
	if (registrations.size() > max_commands)
		return ice::unexpected(diagnostic{"Command registry exceeds 128 entries"});
	auto state = std::make_unique<implementation>();
	state->descriptors.reserve(registrations.size());
	state->handlers.reserve(registrations.size());
	state->cache.reserve(max_receipts);
	std::set<std::string> ids, names;
	std::size_t           remaining = 1024 * 1024;
	for (auto &entry : registrations) {
		if (!entry.execute || !descriptor_budget(entry.descriptor, remaining) ||
		    !ids.insert(entry.descriptor.id).second || !names.insert(entry.descriptor.name).second)
			return ice::unexpected(
			    diagnostic{"Invalid/duplicate command descriptor or registry budget"});
		if (entry.descriptor.arguments) {
			auto checked = meta::validate_definition(*entry.descriptor.arguments);
			if (!checked)
				return ice::unexpected(
				    diagnostic{std::move(checked.error().message), checked.error().field});
		}
		state->descriptors.push_back(std::move(entry.descriptor));
		state->handlers.push_back(std::move(entry.execute));
	}
	try {
		state->identity = new_identity();
	} catch (...) {
		return ice::unexpected(diagnostic{"Could not create a session lifetime identity"});
	}
	if (state->identity.empty())
		return ice::unexpected(diagnostic{"Session lifetime identity counter exhausted"});
	return service(std::move(state));
}
std::span<const command_descriptor> service::discover() const {
	return state_ ? std::span<const command_descriptor>(state_->descriptors)
	              : std::span<const command_descriptor>{};
}
const std::string &service::identity() const {
	static const std::string empty;
	return state_ ? state_->identity : empty;
}
std::uint64_t service::revision() const { return state_ ? state_->revision : 0; }

std::shared_ptr<const receipt> service::invoke(const request &operation) {
	auto result = std::make_shared<receipt>();
	if (token(operation.operation_id))
		result->operation_id = operation.operation_id;
	result->session  = identity();
	result->revision = revision();
	auto reject      = [&](status outcome, const char *message) -> std::shared_ptr<const receipt> {
        result->status     = outcome;
        result->diagnostic = diagnostic{message};
        return result;
	};
	if (!state_)
		return reject(status::service_unavailable, "Service was moved from");
	auto &state = *state_;
	if (state.invoking)
		return reject(status::service_busy, "Reentrant invocation is not permitted");
	if (!bounded(operation) || !token(operation.operation_id))
		return reject(status::invalid_request, "Request exceeds envelope or retry identity budget");
	for (const auto &entry : state.cache)
		if (entry.operation.operation_id == operation.operation_id) {
			if (same_request(entry.operation, operation))
				return entry.result;
			return reject(status::operation_conflict,
			              "Operation ID was already used for a different request");
		}
	// Reserve both the immutable receipt and its exact request fingerprint before calling host
	// code. Publishing the result after a successful host effect needs no allocation or copying.
	implementation::remembered entry{operation, result};
	if (state.cache.size() == max_receipts)
		state.cache.erase(state.cache.begin());
	state.cache.push_back(std::move(entry));
	struct preparation_guard {
		implementation &state;
		int             exceptions = std::uncaught_exceptions();
		~preparation_guard() {
			// Allocation can fail during validation/defaulting. Do not save a half-built receipt.
			// Host exceptions are caught below and publish a complete conservative receipt.
			if (std::uncaught_exceptions() > exceptions)
				state.cache.pop_back();
		}
	} preparation{state};
	if (!token(operation.command_id) ||
	    operation.expected_session.has_value() != operation.expected_revision.has_value() ||
	    (operation.expected_session && !core::valid_utf8_text(*operation.expected_session, 128)))
		return reject(status::invalid_request,
		              "Invalid command identity or expected-session guard pair");
	auto found =
	    std::ranges::find(state.descriptors, operation.command_id, &command_descriptor::id);
	if (found == state.descriptors.end())
		return reject(status::unknown_command, "Unknown command ID");
	if (found->effect == effect::mutation && !operation.expected_session)
		return reject(status::invalid_request,
		              "Mutation requires expected session identity and revision");
	if (operation.expected_session && *operation.expected_session != state.identity)
		return reject(status::stale_session,
		              "Expected session belongs to a different service lifetime");
	if (operation.expected_revision && *operation.expected_revision != state.revision)
		return reject(status::stale_revision, "Expected session revision is stale");
	// Even a read-only handler can violate its contract by throwing. Keep one revision available
	// for conservative invalidation before entering any host code.
	if (state.revision == std::numeric_limits<std::uint64_t>::max())
		return reject(status::revision_exhausted, "Session revision is exhausted");
	meta::record arguments;
	if (found->arguments) {
		auto checked = meta::validate(*found->arguments, operation.arguments);
		if (!checked) {
			result->status = status::invalid_request;
			result->diagnostic =
			    diagnostic{std::move(checked.error().message), checked.error().field};
			return result;
		}
		arguments = std::move(*checked);
	} else if (!operation.arguments.empty()) {
		return reject(status::invalid_request, "Command accepts no arguments");
	}
	// Defaults count too: a small authored request must not expand past the execution budget.
	std::size_t remaining = max_record_text;
	for (const auto &[id, value] : arguments) {
		(void)id;
		if (!scalar(value, remaining))
			return reject(status::invalid_request,
			              "Normalized arguments exceed scalar/text budget");
	}
	result->diagnostic =
	    diagnostic{"Host result was not trustworthy; inspect state before retrying"};
	state.invoking = true;
	struct invocation_guard {
		bool &active;
		~invocation_guard() { active = false; }
	} guard{state.invoking};
	auto uncertain = [&](status outcome) -> std::shared_ptr<const receipt> {
		result->status           = outcome;
		result->inspect_required = true;
		result->revision         = ++state.revision;
		return result;
	};
	try {
		const auto index    = static_cast<std::size_t>(found - state.descriptors.begin());
		auto       executed = state.handlers[index](arguments);
		if (!executed) {
			if (core::valid_utf8_text(executed.error().message, max_text) &&
			    !executed.error().message.empty()) {
				result->status     = status::handler_failed;
				result->diagnostic = std::move(executed.error());
				return result;
			}
			return uncertain(status::invalid_output);
		}
		if (!valid_output(executed->fields) ||
		    (found->effect == effect::read_only && executed->changed))
			return uncertain(status::invalid_output);
		result->fields.swap(executed->fields);
		result->changed = executed->changed;
		if (executed->changed)
			++state.revision;
		result->revision = state.revision;
		result->diagnostic.reset();
		result->status = status::succeeded;
		return result;
	} catch (...) {
		return uncertain(status::handler_exception);
	}
}

} // namespace ice::commands
