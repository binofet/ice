#pragma once

#include "ice/meta/schema.h"
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

namespace ice::commands {

/// Bounded named scalar results; names are UTF-8 labels, never native pointers or object handles.
class output {
	using storage = std::map<std::string, meta::value, std::less<>>;
	std::unique_ptr<storage> fields_;
	storage                 &writable() {
        if (!fields_)
            fields_ = std::make_unique<storage>();
        return *fields_;
	}

public:
	/// Named field and owned scalar pair.
	using value_type = storage::value_type;
	/// Owned scalar-record bridge type; copies are ordinary fallible preparation.
	using map_type = storage;
	/// Owning extracted field node.
	using node_type = storage::node_type;
	/// Node insertion outcome, including unconsumed duplicate nodes.
	using insert_return_type = storage::insert_return_type;
	/// Mutable ordered field iterator.
	using iterator = storage::iterator;
	/// Immutable ordered field iterator.
	using const_iterator = storage::const_iterator;
	/// Prepare an empty owned field collection; allocation failure propagates.
	output() : fields_(std::make_unique<storage>()) {}
	/// Prepare owned fields; allocation failure leaves the caller's state unchanged.
	output(std::initializer_list<value_type> fields) : fields_(std::make_unique<storage>(fields)) {}
	/// Copy fields independently; allocation failure propagates.
	output(const output &other)
	    : fields_(other.fields_ ? std::make_unique<storage>(*other.fields_)
	                            : std::make_unique<storage>()) {}
	/// Transfer ownership without allocation; the source becomes empty.
	output(output &&other) noexcept = default;
	/// Copy transactionally, preserving the destination on allocation failure.
	output &operator=(const output &other) {
		output copy(other);
		swap(copy);
		return *this;
	}
	/// Transfer ownership without allocation; the source becomes empty.
	output &operator=(output &&other) noexcept = default;
	/// Replace fields transactionally from named scalar pairs.
	output &operator=(std::initializer_list<value_type> fields) {
		output copy(fields);
		swap(copy);
		return *this;
	}
	/// Number of owned fields.
	std::size_t size() const noexcept { return fields_ ? fields_->size() : 0; }
	/// Whether no fields are owned.
	bool empty() const noexcept { return size() == 0; }
	/// First mutable field, or end for an empty result.
	iterator begin() noexcept { return fields_ ? fields_->begin() : iterator{}; }
	/// Mutable end sentinel; empty and moved-from results are iterable.
	iterator end() noexcept { return fields_ ? fields_->end() : iterator{}; }
	/// First immutable field, or end for an empty result.
	const_iterator begin() const noexcept { return fields_ ? fields_->begin() : const_iterator{}; }
	/// Immutable end sentinel.
	const_iterator end() const noexcept { return fields_ ? fields_->end() : const_iterator{}; }
	/// Find a field without allocating a key.
	template <class Key> iterator find(const Key &key) {
		return fields_ ? fields_->find(key) : iterator{};
	}
	/// Find an immutable field without allocating a key.
	template <class Key> const_iterator find(const Key &key) const {
		return fields_ ? fields_->find(key) : const_iterator{};
	}
	/// Checked mutable access; a missing field throws out_of_range.
	meta::value &at(const std::string &key) {
		if (!fields_)
			throw std::out_of_range("Missing output field");
		return fields_->at(key);
	}
	/// Checked immutable access; a missing field throws out_of_range.
	const meta::value &at(const std::string &key) const {
		if (!fields_)
			throw std::out_of_range("Missing output field");
		return fields_->at(key);
	}
	/// Insert a default scalar if absent; allocation failure propagates.
	meta::value &operator[](const std::string &key) { return writable()[key]; }
	/// Insert a default scalar using an owned key; allocation failure propagates.
	meta::value &operator[](std::string &&key) { return writable()[std::move(key)]; }
	/// Construct a named field with ordinary map insertion semantics.
	template <class... Args> auto emplace(Args &&...args) {
		return writable().emplace(std::forward<Args>(args)...);
	}
	/// Copy a scalar record into independently owned fields.
	output(const map_type &fields) : fields_(std::make_unique<storage>(fields)) {}
	/// Prepare fields from a scalar record; native map move may allocate on some hosts.
	output(map_type &&fields) : fields_(std::make_unique<storage>(std::move(fields))) {}
	/// Copy the scalar record for an explicit, fallible interface bridge.
	map_type copy_fields() const { return fields_ ? *fields_ : map_type{}; }
	/// Insert a field only when absent, retaining associative duplicate semantics.
	template <class... Args> auto try_emplace(Args &&...args) {
		return writable().try_emplace(std::forward<Args>(args)...);
	}
	/// Whether a named field exists, with transparent key lookup.
	template <class Key> bool contains(const Key &key) const { return find(key) != end(); }
	/// Insert a field range, retaining existing values for duplicate names.
	template <class Input> void insert(Input first, Input last) { writable().insert(first, last); }
	/// Insert a field list, retaining existing values for duplicate names.
	void insert(std::initializer_list<value_type> fields) { writable().insert(fields); }
	/// Remove a named field; missing fields leave the container unchanged.
	template <class Key> std::size_t erase(const Key &key) {
		auto found = find(key);
		if (found == end())
			return 0;
		fields_->erase(found);
		return 1;
	}
	/// Remove an existing mutable iterator and return its successor.
	iterator erase(iterator position) { return fields_->erase(position); }
	/// Remove an existing iterator and return its successor.
	iterator erase(const_iterator position) { return fields_->erase(position); }
	/// Extract an existing mutable iterator without copying its scalar value.
	node_type extract(iterator position) { return fields_->extract(position); }
	/// Extract an existing iterator without copying its scalar value.
	node_type extract(const_iterator position) { return fields_->extract(position); }
	/// Extract a named node; a missing field yields an empty node.
	template <class Key> node_type extract(const Key &key) {
		auto found = find(key);
		return found == end() ? node_type{} : fields_->extract(found);
	}
	/// Insert an owning node, returning duplicate ownership to the caller.
	insert_return_type insert(node_type &&node) { return writable().insert(std::move(node)); }
	/// Transfer nonduplicate nodes; duplicate keys remain in the source.
	void merge(output &other) {
		if (!other.empty())
			writable().merge(*other.fields_);
	}
	/// Transfer nonduplicate nodes from a temporary field collection.
	void merge(output &&other) { merge(other); }
	/// Filter fields explicitly; unlike std::erase_if, this accepts the owned wrapper.
	template <class Predicate> std::size_t erase_if(Predicate predicate) {
		if (!fields_)
			return 0;
		return std::erase_if(*fields_, std::move(predicate));
	}
	/// Remove all fields without allocating.
	void clear() noexcept {
		if (fields_)
			fields_->clear();
	}
	/// Exchange owned field storage without allocating or moving a native map.
	void swap(output &other) noexcept { fields_.swap(other.fields_); }
};
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
