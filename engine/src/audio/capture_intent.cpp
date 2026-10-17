#include "ice/audio/capture_intent.h"
#include <stdexcept>
#include <utility>

namespace ice::audio {
capture_intent::capture_intent(capture_intent_callbacks callbacks)
    : callbacks_(std::move(callbacks)) {
	if (!callbacks_.prepare || !callbacks_.before_start || !callbacks_.start || !callbacks_.stop ||
	    !callbacks_.after_stop)
		throw std::invalid_argument("Capture intent requires all lifecycle callbacks");
}
capture_intent::~capture_intent() {
	try {
		(void)end();
	} catch (...) {
		// Destruction must still release callback owners if allocating a diagnostic fails.
	}
}
ice::expected<void, diagnostic> capture_intent::begin() {
	if (prepared_ || notification_owed_)
		return ice::unexpected(diagnostic{"Capture intent is already open"});
	try {
		prepared_   = true;
		auto result = callbacks_.prepare();
		if (result) {
			notification_owed_ = true;
			result             = callbacks_.before_start();
		}
		if (result)
			result = callbacks_.start();
		if (!result) {
			auto cleanup = end();
			if (!cleanup) {
				result.error().message += "; cleanup: " + cleanup.error().message;
				if (!result.error().native_code)
					result.error().native_code = cleanup.error().native_code;
			}
			return result;
		}
		active_ = true;
		return {};
	} catch (...) {
		auto       cleanup = end();
		diagnostic failure{"Capture preparation/start callback threw"};
		if (!cleanup) {
			failure.message += "; cleanup: " + cleanup.error().message;
			failure.native_code = cleanup.error().native_code;
		}
		return ice::unexpected(std::move(failure));
	}
}
ice::expected<void, diagnostic> capture_intent::end() {
	const bool stop   = std::exchange(prepared_, false);
	const bool notify = std::exchange(notification_owed_, false);
	active_           = false;
	ice::expected<void, diagnostic> result;
	bool                            stop_threw = false, notification_threw = false;
	if (stop) {
		try {
			result = callbacks_.stop();
		} catch (...) {
			stop_threw = true;
		}
	}
	if (notify) {
		try {
			callbacks_.after_stop(stop && !stop_threw && bool(result));
		} catch (...) {
			notification_threw = true;
		}
	}
	// Build diagnostics only after both cleanup operations have been attempted.
	if (stop_threw)
		result = ice::unexpected(diagnostic{"Capture stop callback threw"});
	if (notification_threw) {
		if (result)
			result = ice::unexpected(diagnostic{"Capture ending notification threw"});
		else
			result.error().message += "; capture ending notification threw";
	}
	return result;
}
bool capture_intent::active() const noexcept { return active_; }
} // namespace ice::audio
