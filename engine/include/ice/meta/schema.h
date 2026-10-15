#pragma once

#include "ice/core/expected.h"
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace ice::meta {

/// Stable field identity within one schema; zero is reserved and names are not wire identities.
using field_id = std::uint32_t;
/// Scalar kinds in the initial portable profile; object graphs/containers are not yet supported.
enum class scalar_kind : std::uint8_t { boolean, int32, uint32, float32, float64, string };
/// Owned scalar data. monostate is explicit null; an absent map key is a missing field.
using value = std::variant<std::monostate, bool, std::int64_t, std::uint64_t, double, std::string>;
/// Owned instance data keyed by stable field IDs, independent of native member names/layout.
using record = std::map<field_id, value>;
/// Structured validation failure; field zero denotes a whole-object/schema error.
struct diagnostic {
	/// Human-readable failure context, owned by the result.
	std::string message;
	/// Related field identity, or zero for an object/schema failure.
	field_id field = 0;
};
/// Owned field contract; numeric bounds apply after conversion to the declared precision.
struct field_descriptor {
	/// Stable nonzero identity; retain this across renames.
	field_id id = 0;
	/// Human-readable/generated native member name.
	std::string name;
	/// Explicit portable scalar representation, independent of language runtime types.
	scalar_kind kind = scalar_kind::boolean;
	/// Whether explicit null is allowed; missing is controlled separately by has_default.
	bool nullable = false;
	/// Whether a missing field may be populated with default_value.
	bool has_default = false;
	/// Literal default; ignored when has_default is false.
	value default_value;
	/// Optional inclusive numeric lower bound.
	std::optional<double> minimum;
	/// Optional inclusive numeric upper bound.
	std::optional<double> maximum;
	/// Namespaced metadata retained for consumers such as editors and RTTR.
	std::map<std::string, std::string> tags;
};
/// Owned portable schema. This first profile is bounded to 128 scalar fields and 4096-byte strings.
struct schema_descriptor {
	/// Stable type identity; native class names are not persistent identities.
	std::string id;
	/// Explicit schema version; incompatible changes require an explicit migration.
	std::uint32_t version = 0;
	/// Generated native type name used by reflection/editor consumers.
	std::string name;
	/// Field contracts, each with a unique name and nonzero ID.
	std::vector<field_descriptor> fields;
	/// Namespaced type metadata; the schema owns its strings.
	std::map<std::string, std::string> tags;
};

/// Validate descriptor identities, limits, constraints and literal defaults without changing it.
ice::expected<void, diagnostic> validate_definition(const schema_descriptor &schema);
/// Borrow a field descriptor by ID; null means unknown and is never an invitation to add a field.
const field_descriptor *find_field(const schema_descriptor &schema, field_id id);
/// Validate and normalize a scalar, including float32 rounding and finite/range checks.
ice::expected<value, diagnostic> validate_value(const field_descriptor &field, const value &input);
/// Validate all fields transactionally; fill declared defaults for missing fields and reject
/// unknowns.
ice::expected<record, diagnostic> validate(const schema_descriptor &schema, const record &input);
/// Serialize validated data using the bounded, little-endian ICM1 test format, never native layout.
ice::expected<std::vector<std::byte>, diagnostic> serialize(const schema_descriptor &schema,
                                                            const record            &input);
/// Decode a complete ICM1 document; reject truncation, trailing bytes, duplicates and schema
/// mismatch.
ice::expected<record, diagnostic> deserialize(const schema_descriptor   &schema,
                                              std::span<const std::byte> bytes);
/// Migrate one version forward by stable IDs. Changed representations and unacknowledged removals
/// fail. Renames retain IDs; added fields require defaults. The input and descriptors remain
/// unchanged.
ice::expected<record, diagnostic> migrate(const schema_descriptor &previous,
                                          const schema_descriptor &next, const record &input,
                                          std::span<const field_id> removed_fields = {});

/// Host-scoped generational object identity; handles never expose native pointers.
struct object_handle {
	/// One-based slot in the owning store; zero denotes no object.
	std::uint32_t slot = 0;
	/// Generation incremented when a slot is retired; stale handles are rejected.
	std::uint64_t generation = 0;
};
/// Owned object snapshot for inspectors/agents; editing it cannot mutate the active object.
struct object_snapshot {
	/// Revision required by the next edit or undo operation.
	std::uint64_t revision = 0;
	/// Owned validated field values.
	record fields;
};
/// One field change. nullopt removes a field and reapplies its default; value{} means explicit
/// null.
struct field_edit {
	/// Existing field to change; unknown and duplicate IDs reject the whole batch.
	field_id id = 0;
	/// Replacement value, or nullopt to remove the authored override.
	std::optional<value> replacement;
};
/// Single-host-thread live data store, with 64 slots and 32 undo snapshots per object.
/// Call mutation methods at an application-owned frame boundary. No implicit threading or
/// persistence. Ordinary validation failures return diagnostics; allocation exceptions are not a
/// crash sandbox.
class object_store {
public:
	/// Own a schema copy; create validates it before allowing any live instance.
	explicit object_store(schema_descriptor schema);
	/// Create validated data in a free slot; the returned generation identifies this lifetime only.
	ice::expected<object_handle, diagnostic> create(const record &initial);
	/// Retire an instance and its history; the old handle can never address its replacement.
	ice::expected<void, diagnostic> erase(object_handle handle);
	/// Copy live state for inspection; retained snapshots do not change under subsequent edits.
	ice::expected<object_snapshot, diagnostic> inspect(object_handle handle) const;
	/// Validate a bounded batch against an expected revision, then commit it atomically with undo.
	/// Errors preserve values, revision and history. Even an equivalent successful edit advances
	/// revision.
	ice::expected<std::uint64_t, diagnostic>
	apply(object_handle handle, std::uint64_t expected_revision, std::span<const field_edit> edits);
	/// Restore the previous supported object state; successful undo advances revision rather than
	/// rewinding it.
	ice::expected<std::uint64_t, diagnostic> undo(object_handle handle,
	                                              std::uint64_t expected_revision);

private:
	struct slot {
		bool                alive      = false;
		std::uint64_t       generation = 1;
		std::uint64_t       revision   = 1;
		record              fields;
		std::vector<record> history;
	};
	ice::expected<std::size_t, diagnostic> resolve(object_handle handle) const;
	schema_descriptor                      schema_;
	std::vector<slot>                      slots_;
};

} // namespace ice::meta
