#pragma once
#include "ice/audio/capture.h"
#include <functional>

namespace ice::audio {
/// Bound owner-thread operations for one explicitly requested microphone lifetime.
/// Callbacks and referenced owners outlive capture_intent. They must not reenter it.
struct capture_intent_callbacks {
	/// Prepare native input and bounded storage without starting capture.
	std::function<ice::expected<void, diagnostic>()> prepare;
	/// Synchronously retire presentation/output before native start. Rejection prevents start.
	std::function<ice::expected<void, diagnostic>()> before_start;
	/// Start the already prepared input. No automatic retry is performed.
	std::function<ice::expected<void, diagnostic>()> start;
	/// Stop/close prepared or started input, including partial preparation/start failures.
	std::function<ice::expected<void, diagnostic>()> stop;
	/// Balance each attempted before_start exactly once, after native stop is attempted.
	/// Also called when before_start rejects or throws; exceptions are contained. Receives
	/// native_stop_confirmed only when native cleanup returned success without throwing.
	/// False requires keeping output suppressed until the native uncertainty is resolved.
	std::function<void(bool native_stop_confirmed)> after_stop;
};
/// Portable synchronous ordering shared by native capture hosts and quiet fake-driver tests.
/// Permission and explicit foreground intent are host prerequisites, not supplied by this class.
/// Destruction ends any open lifetime; no callbacks, devices or threads are created implicitly.
class capture_intent {
public:
	/// Bind all five callbacks once. Missing callbacks throw std::invalid_argument.
	explicit capture_intent(capture_intent_callbacks callbacks);
	/// Stop any prepared/started input and balance notification before releasing callbacks.
	~capture_intent();
	/// A capture lifetime has one stable owner; copying is unavailable.
	capture_intent(const capture_intent &) = delete;
	/// A capture lifetime has one stable owner; copy assignment is unavailable.
	capture_intent &operator=(const capture_intent &) = delete;
	/// Run prepare, before_start, start in order. Any rejection/exception stops prepared input
	/// and balances an attempted before_start. A second begin while open fails without calls.
	ice::expected<void, diagnostic> begin();
	/// Idempotently stop input before ending notification, even if either callback throws.
	/// Returns the first cleanup failure. The lifetime is retired even on cleanup failure;
	/// this does not prove native hardware stopped when a driver reports failure.
	ice::expected<void, diagnostic> end();
	/// Whether native start succeeded and this owner has not ended that lifetime.
	bool active() const noexcept;

private:
	capture_intent_callbacks callbacks_;
	bool                     prepared_ = false, notification_owed_ = false, active_ = false;
};
} // namespace ice::audio
