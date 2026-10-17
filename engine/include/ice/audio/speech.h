#pragma once
#include "ice/audio/wav.h"
#include <memory>
#include <string_view>

namespace ice::audio {
/// An explicitly available synthesis voice. Discovery does not download voice data.
struct speech_voice {
	/// Stable provider-specific identifier, required on each request.
	std::string id;
	/// Human-readable installed voice name.
	std::string name;
	/// Provider-declared BCP-47 language/region tag.
	std::string locale;
};
/// Immutable provider discovery snapshot; recreate the provider to refresh installed voices.
struct speech_profile {
	/// Provider identity for diagnostics/provenance, unrelated to an LLM or speaker device.
	std::string provider;
	/// Explicitly selectable available voices; empty means unavailable, never a fallback download.
	std::vector<speech_voice> voices;
	/// Supported finite speech-rate multiplier range, inclusive and strictly positive.
	float minimum_rate = .25f, maximum_rate = 2.f;
};
/// Plain synthesis data. No script, SSML, provider credentials or native pointer is interpreted.
struct speech_request {
	/// Caller correlation identifier, nonempty UTF-8 up to256 bytes; not a filesystem path.
	std::string id;
	/// Nonempty valid UTF-8 without NUL, bounded by the configured job profile.
	std::string text;
	/// Exact installed voice identifier from discovery; no implicit voice selection.
	std::string voice_id;
	/// Relative to the provider's normal speaking rate; discovery specifies its allowed range.
	float rate_multiplier = 1;
};
/// Configurable retained-output limits, independent of native provider scratch allocations.
struct speech_limits {
	/// Maximum UTF-8 request bytes; default1MiB, configurable1..16MiB.
	std::size_t max_text_bytes = 1024u * 1024u;
	/// Maximum pending copied float PCM; default128MiB, configurable4bytes..1GiB.
	std::size_t max_buffered_bytes = 128u * 1024u * 1024u;
	/// Maximum pending copied chunks; default16384, configurable1..1000000.
	std::size_t max_buffered_chunks = 16384;
	/// Maximum generated PCM duration, including already delivered chunks; default3600seconds,
	/// configurable1..86400. This is an output-frame limit, not an OS/worker wall-clock watchdog.
	std::uint32_t max_duration_seconds = 3600;
};
/// Synthesis state. Completed can still have buffered PCM; it is distinct from drained output.
enum class speech_phase { running, completed, cancelled, failed };
/// Generation-owned status snapshot. Audio already delivered cannot be rolled back on failure.
struct speech_status {
	/// Provider synthesis/cancellation state.
	speech_phase phase = speech_phase::running;
	/// Original request correlation identifier.
	std::string request_id;
	/// Exact requested provider voice identifier.
	std::string voice_id;
	/// Established PCM format; zero until the first accepted chunk, 8000..192000Hz mono/stereo.
	std::uint32_t sample_rate = 0, channels = 0;
	/// Total accepted/generated frames, including frames already returned by poll.
	std::uint64_t generated_frames = 0;
	/// Total frames handed to the owner by poll, not proof of acoustic playback.
	std::uint64_t delivered_frames = 0;
	/// Retained frames awaiting poll; completed with nonzero pending data is not drained.
	std::uint64_t buffered_frames = 0;
	/// Number of retained copied chunks awaiting poll.
	std::size_t buffered_chunks = 0;
	/// Bounded provider/validation failure, empty for ordinary completion/cancellation.
	diagnostic error;
};
/// Logical service identity and utterance generation; no native synthesis object escapes.
struct speech_handle {
	/// Opaque service identity; zero is invalid.
	std::uint64_t owner = 0;
	/// Current utterance lifetime; zero is invalid and generations never wrap/reuse.
	std::uint64_t generation = 0;
};
/// One atomic owner-thread drain. Chunks retain source rate/channels and own their PCM.
struct speech_update {
	/// Snapshot after this drain; delivered/buffered counts include its returned chunks.
	speech_status status;
	/// PCM handed to the owner exactly once; terminal failure/cancellation discard pending data.
	std::vector<pcm_data> chunks;
};
/// Copyable weak output token for provider callbacks. All methods are thread-safe/noexcept;
/// they validate/copy into a bounded mailbox and never call playback or application code.
/// Late, duplicate-terminal, cancelled and destroyed-owner callbacks are ignored. Callbacks
/// may allocate copied chunks but never throw through a native callback. This is not a
/// real-time mixer API. A rejected append faults the current running job unless already stale.
class speech_output {
public:
	/// An empty output token accepts nothing.
	speech_output() = default;
	/// Copy finite normalized interleaved float PCM. The first chunk fixes mono/stereo format;
	/// changing it, exceeding budgets or invalid data fails the job and discards pending PCM.
	bool append(std::uint32_t sample_rate, std::uint32_t channels,
	            std::span<const float> samples) const noexcept;
	/// Mark provider completion without discarding pending PCM; duplicate completion is ignored.
	void complete() const noexcept;
	/// Fault running synthesis with bounded UTF-8 context; discard unconsumed chunks. Existing
	/// completed/cancelled/failed generations remain terminal. Native code defaults to zero.
	void fail(std::string_view message, std::int32_t native_code = 0) const noexcept;
	/// Whether this generation still accepts PCM; no native provider action is performed.
	bool active() const noexcept;

private:
	struct state;
	explicit speech_output(std::weak_ptr<state> value);
	std::weak_ptr<state> state_;
	friend class speech_jobs;
};
/// Provider interface separate from playback/LLMs. The owner serializes discovery/start/cancel
/// and destruction on its creating thread. Native callbacks may use only the supplied output
/// token, not retain caller request references. A failed start must not be retried implicitly.
class speech_provider {
public:
	/// Retire native provider resources; implementations must contain native exceptions.
	virtual ~speech_provider() = default;
	/// Borrow immutable discovery until provider destruction. No downloads or permission prompts.
	virtual const speech_profile &profile() const noexcept = 0;
	/// Begin synthesis of copied request data and publish PCM/terminal callbacks to this token.
	/// Implementations must not open physical output. Exceptions are contained by speech_jobs.
	virtual ice::expected<void, diagnostic> start(const speech_request &request,
	                                              speech_output         output) = 0;
	/// Stop pending native synthesis. Must be idempotent/noexcept; late callbacks are permitted
	/// only through output tokens. It must never implicitly resume or download missing voices.
	virtual void cancel() noexcept = 0;
};
/// Move-only owner-thread coordinator for one active utterance. Use independent instances for
/// simultaneous speakers. Pending PCM is streamed through poll, not retained as a whole clip.
/// New work replaces only a terminal/drained job; cancellation discards pending PCM, retains
/// terminal metadata, and rejects late callbacks. Public operations/destruction must be
/// serialized on the creating thread. No playback, file/network I/O or app callback is implicit.
class speech_jobs {
public:
	/// Validate profile/limits and take provider ownership without starting synthesis.
	static ice::expected<speech_jobs, diagnostic> create(std::unique_ptr<speech_provider> provider,
	                                                     speech_limits limits = {});
	/// Cancel active synthesis, discard pending PCM and expire all weak output tokens.
	~speech_jobs();
	/// Transfer stable ownership/handles on the same owner thread; source becomes empty.
	speech_jobs(speech_jobs &&other) noexcept;
	/// Cancel/retire the previous owner and transfer the other service.
	speech_jobs &operator=(speech_jobs &&other) noexcept;
	/// A native provider has one service owner.
	speech_jobs(const speech_jobs &) = delete;
	/// Copy assignment is unavailable.
	speech_jobs &operator=(const speech_jobs &) = delete;
	/// Return immutable discovery as owned data; moved-from owners return an empty profile.
	speech_profile profile() const;
	/// Start one explicit utterance after validating text/voice/rate/busy state. Failed validation
	/// does not invoke the provider or replace the prior job. Provider failure/exception cancels
	/// its candidate and leaves prior terminal/drained metadata intact; no automatic replay.
	ice::expected<speech_handle, diagnostic> start(const speech_request &request);
	/// Drain pending copied PCM exactly once. Observing failure also cancels native synthesis.
	/// Allocation failure before draining leaves the queued chunks owned by the service.
	ice::expected<speech_update, diagnostic> poll(speech_handle handle);
	/// Inspect current status without consuming PCM or advancing synthesis/application time.
	ice::expected<speech_status, diagnostic> inspect(speech_handle handle) const;
	/// Cancel the current generation, discard pending PCM and ignore late callbacks. Repeating
	/// cancellation is successful; stale/foreign handles reject. Already drained completion and
	/// failed jobs keep their terminal outcome. Previously delivered audio is not rolled back.
	ice::expected<void, diagnostic> cancel(speech_handle handle);

private:
	struct state;
	explicit speech_jobs(std::unique_ptr<state> value);
	std::unique_ptr<state> state_;
};
/// Create Apple's installed system-voice PCM provider on the main thread. Uses only
/// AVSpeechSynthesizer.writeUtterance; never speakUtterance, downloads, Personal Voice
/// authorization or third-party voice extensions. Native event-loop servicing is required.
/// Non-Apple platforms return an unavailable diagnostic; no provider is silently substituted.
ice::expected<std::unique_ptr<speech_provider>, diagnostic> create_apple_speech_provider();
} // namespace ice::audio
