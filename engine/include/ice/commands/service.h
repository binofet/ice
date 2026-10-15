#pragma once

#include "ice/meta/schema.h"
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace ice::commands {

/// Bounded named scalar results; names are UTF-8 labels, never native pointers or object handles.
using output = std::map<std::string, meta::value, std::less<>>;
/// Declared host effect. Read-only callbacks must not change observable session state.
enum class effect : std::uint8_t { read_only, mutation };
/// Owned discovery data, frozen for the entire service lifetime.
struct command_descriptor {
	/// Stable command identity; renaming the display name does not change this identity.
	std::string id;
	/// Unique human-readable command name.
	std::string name;
	/// Purpose and host-specific semantics, suitable for tool discovery.
	std::string description;
	/// Scalar argument contract; nullopt means exactly zero arguments.
	std::optional<meta::schema_descriptor> arguments;
	/// Whether successful invocation may change session state.
	commands::effect effect = effect::read_only;
};
/// Owned failure context; field zero denotes an envelope, command or whole-result failure.
struct diagnostic {
	/// Bounded UTF-8 explanation, at most 4096 bytes.
	std::string message;
	/// Related argument's stable field ID, when available.
	meta::field_id field = 0;
};
/// Successful host execution. Ordinary failure must preserve host state before returning an error.
struct handler_result {
	/// True only when this invocation successfully changed observable host state.
	bool changed = false;
	/// At most 128 scalar fields, 4096 bytes per string and 64 KiB aggregate text.
	output fields;
};
/// Synchronous owned callback. Arguments are validated/defaulted before entry; never retain them.
/// Prepare fallible resources before committing. An error must leave host state unchanged.
/// Exceptions cannot be rolled back: the service invalidates revisions and requests inspection.
using handler = std::function<ice::expected<handler_result, diagnostic>(const meta::record &)>;
/// Immutable registration; captures must own dependencies or borrow objects that outlive the
/// service.
struct registration {
	/// Owned discovery contract, validated once at service creation.
	command_descriptor descriptor;
	/// Callback retained until service destruction; no unregistration during a session.
	handler execute;
};
/// Owned single-operation request. This is not a multi-command transaction or an authenticated
/// token.
struct request {
	/// Caller-generated retry identity, 1..128 ASCII letters, digits, '.', '_', ':' or '-'.
	std::string operation_id;
	/// Stable registered command ID, with the same syntax/budget as operation_id.
	std::string command_id;
	/// Authored scalar arguments keyed by stable field IDs, before defaulting/normalization.
	meta::record arguments;
	/// Required for mutations; optional read-only guards must be supplied as a pair.
	std::optional<std::string> expected_session;
	/// Expected service revision, not a renderer-local or shader-local revision.
	std::optional<std::uint64_t> expected_revision;
};
/// Structured invocation outcome. Rejected envelopes never enter the host callback.
enum class status : std::uint8_t {
	succeeded,
	invalid_request,
	unknown_command,
	stale_session,
	stale_revision,
	operation_conflict,
	handler_failed,
	handler_exception,
	invalid_output,
	service_busy,
	revision_exhausted,
	service_unavailable
};
/// Immutable owned execution receipt; retained receipts survive cache eviction and service
/// destruction.
struct receipt {
	/// Request retry identity; oversized malformed identities are omitted.
	std::string operation_id;
	/// Opaque session lifetime identity; not a credential or a promise of cross-process
	/// persistence.
	std::string session;
	/// Service revision after this operation. Cached retries retain their original revision.
	std::uint64_t revision = 0;
	/// Machine-readable outcome independent of the diagnostic wording.
	commands::status status = status::invalid_request;
	/// Host-confirmed successful state change; inspect_required means the actual effect is unknown.
	bool changed = false;
	/// An exception or invalid host result prevented a trustworthy effect report; inspect before
	/// retry.
	bool inspect_required = false;
	/// Failure context, absent on success.
	std::optional<commands::diagnostic> diagnostic;
	/// Bounded owned host output, present only on successful execution.
	output fields;
};
/// Single-host-thread command/session boundary with 128 commands and 64 retained retry receipts.
/// Invoke synchronously between host frames; no thread marshalling, locks, network or rendering.
/// Validation/unknown/stale/conflict failures cannot call the host. Handler errors rely on the
/// host's transactional contract; thrown exceptions and invalid outputs conservatively advance
/// revision and require inspection. Allocation failure before invocation may propagate.
/// Deduplication is bounded, process-local and not durable: an evicted operation ID is forgotten.
class service {
public:
	/// Validate and own all registrations; an empty command set is permitted.
	static ice::expected<service, diagnostic> create(std::vector<registration> registrations);
	/// Release callbacks and the bounded retry cache; previously retained receipts remain valid.
	~service();
	/// Transfer the session identity, callbacks and receipts; the source becomes unavailable.
	service(service &&other) noexcept;
	/// Replace this service lifetime with the source session; previous borrowed discovery expires.
	service &operator=(service &&other) noexcept;
	/// A session has exactly one owner.
	service(const service &) = delete;
	/// A session cannot be duplicated through assignment.
	service &operator=(const service &) = delete;
	/// Borrow frozen discovery data until this service is destroyed or moved from/replaced.
	std::span<const command_descriptor> discover() const;
	/// Borrow the opaque lifetime identity, unchanged by renderer/device replacement.
	const std::string &identity() const;
	/// Read the monotonic session revision; initial revision is zero.
	std::uint64_t revision() const;
	/// Validate, deduplicate and synchronously execute one command at a host frame boundary.
	/// Exact retries return the same immutable receipt, even after revision changes. Different
	/// reuse is rejected. Bounded rejected requests are cached too; oversized envelopes are not
	/// retained.
	std::shared_ptr<const receipt> invoke(const request &operation);

private:
	struct implementation;
	explicit service(std::unique_ptr<implementation> state);
	std::unique_ptr<implementation> state_;
};

} // namespace ice::commands
