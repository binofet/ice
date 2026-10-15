#pragma once

#include "ice/core/expected.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace ice::data {

/// Structured consumer diagnostic; path identifies the schema/document/field or operation.
struct diagnostic {
	/// Explanation suitable for an inspector or agent response.
	std::string message;
	/// URI/identity/field path when available; empty denotes the entire request.
	std::string path;
	/// Compare structured diagnostic text/path without exception identity.
	bool operator==(const diagnostic &other) const = default;
};
/// Serialized object identity and required target type; this is not a runtime pointer or handle.
struct object_reference {
	/// Canonical document URI including its query, without a fragment.
	std::string document;
	/// Decoded document-local identity.
	std::string identity;
	/// Required portable type ID.
	std::string type;
	/// Compare persistent identity and type; no object traversal is performed.
	bool operator==(const object_reference &other) const = default;
};
/// Fully owned portable data. Recursive containers copy by value, with no mutable shared aliases.
struct value {
	/// Ordered sequence, or explicit map key/value pairs as dictated by a schema.
	using sequence = std::vector<value>;
	/// UTF-8 field/control names; this is not the logical map-container representation.
	using record = std::map<std::string, value, std::less<>>;
	/// Explicit numeric alternatives keep booleans, signed values and unsigned values distinct.
	using storage = std::variant<std::monostate, bool, std::int64_t, std::uint64_t, double,
	                             std::string, sequence, record, object_reference>;
	/// Null is monostate; a missing record key is a separate condition.
	storage data;
	/// Compare owned values recursively; references compare addresses without following cycles.
	bool operator==(const value &other) const = default;
};
/// SHA-256 projection identity supplied by a trusted generated catalog/host contract.
using schema_fingerprint = std::array<std::byte, 32>;
/// Validate UTF-8, finite numbers and shared depth/node/string/byte bounds; reject runtime refs.
ice::expected<void, diagnostic> validate_tree(const value &input);
/// Write the experimental canonical MDB1 authored-tree format; references must use authored tags.
ice::expected<std::vector<std::byte>, diagnostic>
encode_binary(const value &input, const schema_fingerprint &fingerprint);
/// Read a complete MDB1 tree, checking fingerprint, canonical encoding, bounds and trailing bytes.
ice::expected<value, diagnostic> decode_binary(std::span<const std::byte> input,
                                               const schema_fingerprint  &fingerprint);

/// Logical kinds in ICE's portable-data consumer; Metagen's generic core imposes none of these.
enum class kind {
	boolean,
	int32,
	uint32,
	float32,
	float64,
	string,
	record,
	enumeration,
	sequence,
	array,
	map,
	set
};
/// Allowed serialized forms for object leaves, independently of nullability or native ownership.
enum class reference_policy { inline_value, local, external, any };
/// Opt-in prototype merge behavior; ordinary sequences replace by default.
enum class merge_policy { replace, record, keyed, map, set };
/// Recursive portable type expression; references to named types stop structural recursion.
struct type_expression {
	/// Logical value/container kind.
	kind type = kind::boolean;
	/// Whether explicit null is permitted at this occurrence.
	bool nullable = false;
	/// Named record/enum ID, empty for other kinds.
	std::string identity;
	/// Element type, or map key/value types; empty for scalars/named types.
	std::vector<type_expression> arguments;
};
/// Stable enum symbol identity is independent of its numeric value and display spelling.
struct enum_symbol {
	/// Nonzero stable symbol ID.
	std::uint32_t id = 0;
	/// Human-readable symbol spelling.
	std::string name;
	/// Closed int32 enum value in this profile.
	std::int32_t number = 0;
};
/// Field semantics consumed from author attributes through the portable-data projection.
struct field_schema {
	/// Nonzero persistent field ID; names may change without changing this identity.
	std::uint32_t id = 0;
	/// Human-readable field name in authored JSON/XML/tree data.
	std::string name;
	/// Value type and occurrence nullability.
	type_expression type;
	/// Serialized object occurrence policy, including object leaves of containers.
	reference_policy storage = reference_policy::inline_value;
	/// Prototype merge behavior explicitly selected by the author.
	merge_policy merge = merge_policy::replace;
	/// Stable identity field for an opt-in keyed sequence; empty otherwise.
	std::string key;
	/// Missing means no default; value{monostate} is an explicit null default.
	std::optional<value> default_value;
};
/// One named record or enum definition owned by a catalog.
struct type_schema {
	/// Stable portable type identity.
	std::string identity;
	/// Explicit nonzero schema version; migration is required for a mismatch.
	std::uint32_t version = 0;
	/// Human-readable/generator-facing type name.
	std::string name;
	/// True for an enum; otherwise this is a record definition.
	bool enumeration = false;
	/// Whether objects of this type may explicitly select a prototype.
	bool prototype = false;
	/// Record field definitions, empty for an enum.
	std::vector<field_schema> fields;
	/// Enum symbols, empty for a record.
	std::vector<enum_symbol> symbols;
};
/// Validated immutable catalog; parser/attribute policy belongs to this ICE consumer.
class catalog {
public:
	/// Validate and own a portable-data projection, including named dependencies and defaults.
	static ice::expected<catalog, diagnostic> read(const value &projection);
	/// Borrow all definitions; their lifetime is the lifetime of this catalog.
	std::span<const type_schema> types() const;
	/// Borrow a definition, or null for an unknown ID; no implicit schema creation occurs.
	const type_schema *find(const std::string &identity) const;

private:
	catalog() = default;
	std::vector<type_schema> types_;
};
/// Canonical document URI and local object ID; ordering is independent of object-table position.
using object_address = std::pair<std::string, std::string>;
/// Immutable effective graph plus separately retained authored prototype/operation documents.
struct graph_data {
	/// Validated root object reference.
	object_reference root;
	/// Materialized owned object values; reference cycles contain IDs, not recursive ownership.
	std::map<object_address, value> objects;
	/// Original authored trees keyed by canonical document URI, preserving operation intent.
	std::map<std::string, value> authored;
};
/// Application-supplied document reader; calls occur during preparation, never during activation.
using document_reader = std::function<ice::expected<value, diagnostic>(const std::string &uri)>;
/// Sealed, detached graph prepared successfully by this consumer; cannot be forged from raw data.
class prepared_graph {
public:
	/// Retain the immutable candidate for inspection; null only after moving from this object.
	std::shared_ptr<const graph_data> data() const;

private:
	prepared_graph() = default;
	std::shared_ptr<const graph_data> data_;
	std::shared_ptr<const catalog>    definitions_;
	friend class graph_store;
};
/// Retain this snapshot to inspect a particular immutable revision across later replacements.
struct graph_snapshot {
	/// Host-store identity; revisions/generations from a different store are unrelated.
	std::uint64_t owner = 0;
	/// Monotonic successful activation/undo revision.
	std::uint64_t revision = 0;
	/// Lifetime token; every activation/undo invalidates handles from earlier active generations.
	std::uint64_t generation = 0;
	/// Shared immutable graph storage, never a mutable alias supplied by the caller.
	std::shared_ptr<const graph_data> data;
};
/// Host-local checked object handle; persistent addresses alone do not confer a runtime lifetime.
struct graph_handle {
	/// Owning host store; prevents using a coincident generation in another store.
	std::uint64_t owner = 0;
	/// Generation of the graph against which this handle was acquired.
	std::uint64_t generation = 0;
	/// Persistent object address and required type.
	object_reference object;
};
/// Call all live-state methods on the host thread; retained immutable snapshots may be read
/// elsewhere. Preparation may run separately with a thread-safe reader. Publish only at the
/// host's chosen safe boundary; this class has no frame-loop integration.
/// Validation failures preserve active state/history; allocation exceptions are not a sandbox.
class graph_store {
public:
	/// Own an immutable catalog used by subsequent preparations.
	explicit graph_store(catalog definitions);
	/// Stores have one publication owner and cannot be copied.
	graph_store(const graph_store &other) = delete;
	/// Publication ownership cannot be reassigned by copy.
	graph_store &operator=(const graph_store &other) = delete;
	/// Resolve/validate the whole discovered graph off the active state, with explicit budgets.
	ice::expected<prepared_graph, diagnostic> prepare(const std::string &document_uri,
	                                                  const value       &document,
	                                                  document_reader    reader = {}) const;
	/// Publish against an expected revision (zero initially); stage allocations before state swap.
	ice::expected<std::uint64_t, diagnostic> activate(const prepared_graph &candidate,
	                                                  std::uint64_t         expected_revision);
	/// Restore one of eight retained graph states; revision and generation still advance.
	ice::expected<std::uint64_t, diagnostic> undo(std::uint64_t expected_revision);
	/// Retain the active immutable snapshot; null before the first successful activation.
	std::shared_ptr<const graph_snapshot> snapshot() const;
	/// Acquire a generation-checked handle for a currently active validated reference.
	ice::expected<graph_handle, diagnostic> handle(const object_reference &object) const;
	/// Copy an object's effective value; stale handles fail and the result cannot edit live state.
	ice::expected<value, diagnostic> inspect(const graph_handle &handle) const;

private:
	std::shared_ptr<const catalog>                 definitions_;
	std::uint64_t                                  owner_ = 0;
	std::shared_ptr<const graph_snapshot>          active_;
	std::vector<std::shared_ptr<const graph_data>> history_;
};

} // namespace ice::data
