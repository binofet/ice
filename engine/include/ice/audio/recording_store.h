#pragma once
#include "ice/audio/capture.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace ice::audio {
/// Explicit storage quotas. Exhaustion rejects new captures; existing clips are never evicted.
struct recording_limits {
	/// Total regular-file bytes, including incomplete writes and acknowledgment markers.
	std::uint64_t max_bytes = 256u * 1024u * 1024u;
	/// Committed and interrupted/reserved recording directories share this count.
	std::size_t max_recordings = 10000;
};
/// Host-clock capture interval, not an acoustic sample timestamp or an OpenXR predicted time.
struct recording_timing {
	/// std::chrono::steady_clock nanoseconds sampled immediately before stream start.
	std::uint64_t started_monotonic_ns = 0;
	/// Same process clock, sampled after stream stop; must not precede start.
	std::uint64_t ended_monotonic_ns = 0;
	/// Observed nonnegative native input xrun count; zero does not prove lossless audio.
	std::int32_t xruns = 0;
	/// Bounded UTF-8 host reason, for example release or duration_limit.
	std::string stop_reason = "release";
};
/// Immutable committed clip description. Paths are relative to the store directory.
struct recording_receipt {
	/// Random 128-bit owner session plus monotonically increasing reservation counter.
	std::string recording_id;
	/// Relative owned PCM16 WAV and JSON receipt paths; no host absolute path is serialized.
	std::string wav_path, metadata_path;
	/// Content digest of the complete WAV; integrity identification, not authentication.
	std::string wav_sha256;
	/// Mono source frequency and recorded frames; no resampling or transcription is implied.
	std::uint32_t sample_rate = 0;
	/// Retained mono PCM frame count.
	std::uint64_t frames = 0;
	/// Frames omitted after the capture buffer filled.
	std::uint64_t dropped_frames = 0;
	/// Host stream-start/stop bracket on the same steady clock as the originating frame samples.
	std::uint64_t started_monotonic_ns = 0, ended_monotonic_ns = 0;
	/// Native input xrun count copied without claiming acoustic quality.
	std::int32_t xruns = 0;
	/// Explicit durable acknowledgment; the WAV and receipt are still retained.
	bool acknowledged = false;
};
/// Recovery inventory. Incomplete directories consume quota and require explicit removal.
struct recording_inventory {
	/// Fully committed, structurally/hash-validated recordings, including acknowledged clips.
	std::vector<recording_receipt> recordings;
	/// Interrupted reservations/writes with no committed marker; never returned as playable clips.
	std::vector<std::string> incomplete_ids;
	/// Bytes in all regular owned files, excluding the fixed writer lock.
	std::uint64_t bytes_used = 0;
};
/// Owner-thread, single-writer, app-private recording store. The caller supplies a trusted private
/// parent directory. This class creates or opens its final directory without following a symlink,
/// retains a native lock, and uses private exclusive file creation. No recording is overwritten,
/// uploaded, transcribed, acknowledged or removed implicitly. Reserved IDs are never reused by an
/// owner; collisions with any existing directory are rejected. Moved-from owners reject operations.
class recording_store {
public:
	/// Open/recover the store, validating quota, metadata, exact WAV layout and content hashes.
	/// POSIX requires owner-only directory access; Windows creates an owner-only DACL. Concurrent
	/// writer opens fail. Native I/O and allocation exceptions become diagnostics.
	static ice::expected<recording_store, diagnostic> open(const std::filesystem::path &path,
	                                                       recording_limits limits = {});
	/// Retire native lock and handles; committed/incomplete disk data remain untouched.
	~recording_store();
	/// Transfer the directory lock and reservation identity; no disk data are copied.
	recording_store(recording_store &&other) noexcept;
	/// Retire this lock and transfer the other owner's state.
	recording_store &operator=(recording_store &&other) noexcept;
	/// The native writer lock has one owner.
	recording_store(const recording_store &) = delete;
	/// The native writer lock cannot be shared by copying.
	recording_store &operator=(const recording_store &) = delete;
	/// Reserve a unique ID before starting the microphone. Count quota is checked now; byte quota
	/// is checked on commit. A process crash leaves this empty reservation visible in list().
	ice::expected<std::string, diagnostic> reserve_id();
	/// Explicitly cancel this owner's still-empty reservation; committed/partial data are refused.
	ice::expected<void, diagnostic> cancel_reservation(const std::string &id);
	/// Write/fsync WAV+JSON, then atomically publish a no-replace committed marker and flush its
	/// directory. Only successful commit returns a receipt. Failure may leave an incomplete or,
	/// after a final directory-flush error, committed directory; list() resolves that uncertainty.
	/// No retry/replay or automatic cleanup is claimed. Only this owner's reserved ID is accepted.
	ice::expected<recording_receipt, diagnostic>
	commit(const std::string &id, const capture_buffer &capture, const recording_timing &timing);
	/// Explicit recovery/discovery, never a per-frame operation: validates every committed WAV
	/// and receipt. Corrupt/foreign/symlink data fail rather than silently hiding a clip.
	ice::expected<recording_inventory, diagnostic> list() const;
	/// Durably mark a committed recording acknowledged without deleting or rewriting its files.
	/// Repeated acknowledgment is idempotent. This does not grant sharing consent.
	ice::expected<void, diagnostic> acknowledge(const std::string &id);
	/// Explicitly remove only the named owned recording, complete or incomplete. A durable removal
	/// marker hides the receipt before file deletion, so interrupted removal is recoverable and
	/// never reappears as a playable clip. No other recording or legacy path is touched.
	ice::expected<void, diagnostic> remove(const std::string &id);

private:
	struct state;
	explicit recording_store(std::unique_ptr<state> state);
	std::unique_ptr<state> state_;
};
} // namespace ice::audio
