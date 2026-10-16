#pragma once
#include "ice/audio/capture.h"
#include <memory>

#ifdef __ANDROID__
namespace ice::audio {
/// Owner-thread AAudio mono PCM16 microphone capture. The host requests Android
/// RECORD_AUDIO permission and calls start only for explicit foreground PTT intent.
/// Poll regularly; stop on release, focus/pause, permission loss or host shutdown.
/// No background capture, callback thread, automatic restart or external sharing.
class android_capture {
public:
	/// Construct without opening a microphone or allocating PCM storage.
	android_capture();
	/// Stop and close any owned stream before destroying its retained PCM buffer.
	~android_capture();
	/// A stream has a single owner; copying is unavailable.
	android_capture(const android_capture &) = delete;
	/// A stream has a single owner; copy assignment is unavailable.
	android_capture &operator=(const android_capture &) = delete;
	/// Open input only now, verify negotiated mono PCM16/rate, preallocate at most
	/// ten seconds and start. Android enforces permission; failed start closes its stream.
	ice::expected<void, diagnostic> start(std::uint32_t duration_ms = 10000);
	/// Nonblocking read of at most 16384 mono frames; capture stops/closes at the frame
	/// cap or the first poll after its elapsed duration. The host must keep polling.
	/// A device/read error also closes it and returns a native diagnostic. No retry.
	ice::expected<void, diagnostic> poll();
	/// Stop/close now without reading more queued audio. Idempotent; retains PCM until
	/// discard/next start. A driver close may block; this is not a real-time deadline API.
	ice::expected<void, diagnostic> stop();
	/// Stop capture, zero/discard retained PCM and clear counters. Close errors are
	/// ignored here; call stop first if its native diagnostic must be observed.
	void discard();
	/// Whether this owner currently holds an active input stream.
	bool recording() const;
	/// Retained bounded capture, or null before start/after discard. Borrow lasts until
	/// next start/discard/destruction; inspect only on this owner's thread.
	const capture_buffer *buffer() const;
	/// Last observed AAudio xrun count, or zero before capture. Zero does not prove
	/// an acoustic signal, permission grant or absence of all OS-level audio loss.
	std::int32_t xruns() const;

private:
	struct implementation;
	std::unique_ptr<implementation> state_;
};
} // namespace ice::audio
#endif
