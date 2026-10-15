#pragma once
#include "ice/math/types.h"
#include <cstdint>
#include <string>

namespace ice::physics {
/// Engine-owned body identity, independent of a native pointer or renderer lifetime.
struct body_id {
	/// Unique process-local world lifetime; zero is invalid.
	std::uint64_t owner = 0;
	/// One-based body slot in that world; zero is invalid.
	std::uint32_t slot = 0;
	/// Non-wrapping slot lifetime, checked before every operation.
	std::uint64_t generation = 0;
	/// Compare identity without looking up mutable simulation state.
	bool operator==(const body_id &other) const = default;
};
/// Structured validation, stale-handle or simulation-budget failure.
struct diagnostic {
	/// Owned, human-readable explanation.
	std::string message;
};
/// Kinematic bodies move explicitly and do not respond to impulses.
enum class motion_type { static_body, kinematic, dynamic };
/// Native manifold begin/end; a backend may retire manifolds when bodies sleep.
enum class contact_phase { begin, end };
/// Owned contact/sensor transition identities, never native shape pointers.
struct contact_event {
	/// Whether contact began or ended.
	contact_phase phase = contact_phase::begin;
	/// First body; it may have been retired since this transition occurred.
	body_id first;
	/// Second body; ordering and persistent manifold indices are not promised.
	body_id second;
	/// Whether a sensor overlap produced this transition without collision response.
	bool sensor = false;
};
/// Result of one configured fixed step; inspect transitions through contacts().
struct step_report {
	/// Monotonic completed simulation tick, independent of authored scene revisions.
	std::uint64_t tick = 0;
	/// Transitions exceeded the configured contact-event budget.
	bool contacts_truncated = false;
};
} // namespace ice::physics
