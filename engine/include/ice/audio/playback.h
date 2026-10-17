#pragma once
#include "ice/audio/wav.h"
#include "ice/math/types.h"
#include <memory>

namespace ice::audio {
/// Explicit output selection. Loopback renders actual mixer samples without opening hardware.
enum class playback_mode { native, loopback };
/// HRTF activation policy; prefer may fall back, require rejects unavailable activation.
enum class hrtf_policy { disabled, prefer, require };
/// User-configurable live resource budgets. Exhaustion rejects; no voice is stolen or evicted.
struct playback_limits {
	/// Maximum simultaneous live handles, 1..65536, allocated lazily; backend limits can be lower.
	std::size_t max_voices = 1024;
	/// Total uploaded queued PCM float bytes across this device; default 512 MiB.
	std::size_t max_queued_bytes = 512u * 1024u * 1024u;
	/// Total uploaded native queue blocks across this device.
	std::size_t max_queued_blocks = 65536;
};
/// Immutable device preparation choices. Native playback never silently opens a null sink.
struct playback_options {
	/// Physical default output or explicit offline stereo rendering.
	playback_mode mode = playback_mode::native;
	/// Mixer/output sampling rate; default 48000 Hz, supported range 8000..192000.
	std::uint32_t sample_rate = 48000;
	/// Discover actual activation; prefer is an explicit fallback policy.
	hrtf_policy hrtf = hrtf_policy::prefer;
	/// Configurable resource limits.
	playback_limits limits;
};
/// Native capabilities observed at creation, not promises about untested hardware.
struct playback_capabilities {
	/// Actual library/output identity owned by this value.
	std::string renderer, output;
	/// Whether this is the explicitly requested offline mixer.
	bool loopback = false;
	/// Available floating PCM upload, HRTF policy and environmental effects extensions.
	bool float_pcm = false, hrtf = false, effects = false;
	/// Actual active HRTF state, distinct from extension availability or requested preference.
	bool hrtf_active = false;
	/// Actual selected native HRTF dataset name, empty when HRTF is inactive.
	std::string hrtf_name;
	/// Actual context sampling frequency.
	std::uint32_t sample_rate = 0;
};
/// Device identity and slot generation prevent stale/foreign-handle operations.
struct voice_handle {
	/// Opaque logical device identity; zero is invalid.
	std::uint64_t owner = 0;
	/// Opaque slot, unrelated to a native OpenAL source or application entity.
	std::uint32_t slot = 0;
	/// Slot lifetime; zero is invalid and retired generations are never reused.
	std::uint64_t generation = 0;
};
/// Continuous source controls. Spatial mono emitters use ICE's right-handed metre coordinates.
struct voice_parameters {
	/// Apply point-source spatialization; stereo sources must disable it explicitly.
	bool spatial = false;
	/// Finite world position in metres, each component within [-1000000,1000000].
	math::vec3 position{0};
	/// Finite world velocity in metres per second, each component within [-1000000,1000000].
	math::vec3 velocity{0};
	/// Finite source gain in [0,16].
	float gain = 1;
	/// Finite playback pitch multiplier in (0,16].
	float pitch = 1;
	/// Finite inverse-distance reference distance in metres, in (0,1000000].
	float reference_distance = 1;
	/// Finite clamped inverse-distance rolloff in [0,1000].
	float rolloff = 1;
	/// EFX low-pass high-frequency gain and environmental reverb send, each [0,1].
	/// Neutral defaults do not require EFX; unsupported requested effects reject.
	float high_frequency_gain = 1, reverb_send = 0;
};
/// Queued stream format; fixed for a voice lifetime. No file decoder runs in the mixer callback.
struct voice_description {
	/// Native source rate, 8000..192000 Hz; the backend resamples to the output rate.
	std::uint32_t sample_rate = 48000;
	/// Mono or stereo. Point-source spatialization requires mono.
	std::uint32_t channels = 1;
	/// Initial source controls, validated before native publication.
	voice_parameters parameters;
};
/// Listener pose shared by spatial voices, with +Y up and -Z forward by default.
struct listener_pose {
	/// Finite world position in metres, each component within [-1000000,1000000].
	math::vec3 position{0};
	/// Finite world velocity in metres per second, each component within [-1000000,1000000].
	math::vec3 velocity{0};
	/// Finite noncollinear direction/up; normalized and orthogonalized before publication.
	math::vec3 forward{0, 0, -1};
	/// Approximate up direction, orthogonalized against forward before publication.
	math::vec3 up{0, 1, 0};
};
/// Observable stream state. Starvation is distinct from explicit end-of-stream completion.
enum class voice_phase { stopped, playing, paused, starved, completed };
/// Owner-thread status after reclaiming processed native blocks.
struct voice_status {
	/// Logical phase, including remembered per-voice pause during device suspension.
	voice_phase phase = voice_phase::stopped;
	/// Unconsumed queued blocks/frames, including the current partially played block.
	std::size_t queued_blocks = 0;
	/// Frames in retained blocks; not a sample-accurate presentation cursor.
	std::uint64_t queued_frames = 0;
	/// Number of observed transitions from active playback to starvation.
	std::uint64_t starvation_count = 0;
	/// Whether the caller explicitly marked this stream complete.
	bool end_of_stream = false;
};
/// Move-only, owner-thread playback/streaming service. All public methods and destruction must
/// be serialized on the creating thread; the backend owns its audio processing thread. No Lua,
/// application callback, file read or provider call is invoked from that thread by this wrapper.
/// Queue blocks are copied/uploaded before publication and remain owned until consumed/stopped.
/// Keep pumping from native services while active; starvation is observable and queued blocks
/// restart only a previously explicitly playing, nonpaused stream. Suspend/resume is host-driven.
class playback_device {
public:
	/// Prepare the selected native output or genuine OpenAL Soft loopback context. Failure returns
	/// diagnostics; no hardware/null-backend substitution occurs. Requires floating PCM support.
	static ice::expected<playback_device, diagnostic> create(playback_options options = {});
	/// Stop sources and release queues/context/device; no capture/store data are touched.
	~playback_device();
	/// Transfer the device and stable handle ownership; moved-from methods reject.
	playback_device(playback_device &&other) noexcept;
	/// Retire this device and transfer the other's ownership.
	playback_device &operator=(playback_device &&other) noexcept;
	/// A native context has one owner.
	playback_device(const playback_device &) = delete;
	/// A native context cannot be copied.
	playback_device &operator=(const playback_device &) = delete;
	/// Return discovered native profile data; moved-from owners return an empty profile.
	playback_capabilities capabilities() const;
	/// Validate and publish a listener pose. Invalid input never reaches native setters.
	ice::expected<void, diagnostic> set_listener(const listener_pose &pose);
	/// Allocate a stopped stream with no queued data and return its new generation handle.
	ice::expected<voice_handle, diagnostic> create_voice(const voice_description &description = {});
	/// Validate and update one source; no application entity pointers are retained.
	ice::expected<void, diagnostic> configure(voice_handle            handle,
	                                          const voice_parameters &parameters);
	/// Upload finite normalized float samples, copied before this method returns. Whole interleaved
	/// frames are required. Invalid/budget-rejected blocks do not change the existing queue.
	/// Queuing after finish rejects until stop resets the stream; no unrequested voice starts
	/// automatically.
	ice::expected<void, diagnostic> queue(voice_handle handle, std::span<const float> samples);
	/// Mark end of stream without dropping queued samples; later draining becomes completed.
	ice::expected<void, diagnostic> finish(voice_handle handle);
	/// Explicitly request playback; empty unfinished streams become starved and await queue data.
	ice::expected<void, diagnostic> play(voice_handle handle);
	/// Pause this voice; later queue data do not resume it until an explicit play.
	ice::expected<void, diagnostic> pause(voice_handle handle);
	/// Stop and discard queued data, reset end/starvation state; the handle stays valid.
	ice::expected<void, diagnostic> stop(voice_handle handle);
	/// Stop/delete a voice and invalidate its generation. Other voices remain independent.
	ice::expected<void, diagnostic> retire(voice_handle handle);
	/// Reclaim processed blocks, detect device loss and service explicitly playing queues. This
	/// neither advances application simulation nor starts capture. Native failures are diagnostic.
	ice::expected<void, diagnostic> poll();
	/// Reclaim processed blocks and inspect one voice; no acoustic sample timestamp is implied.
	ice::expected<voice_status, diagnostic> inspect(voice_handle handle);
	/// Pause current output/voices, retaining per-voice play intent and queued data. No background
	/// playback is promised; host lifecycle must invoke this before becoming inactive.
	ice::expected<void, diagnostic> suspend();
	/// Explicitly resume retained playing voices. Native/device loss rejects, never silently
	/// reopens.
	ice::expected<void, diagnostic> resume();
	/// Render interleaved stereo float frames into caller storage, exclusively in loopback mode.
	/// No physical device is opened and no simulation/input is advanced. The span must have an even
	/// sample count; output is fully written on success. Native failures return diagnostics.
	ice::expected<void, diagnostic> render(std::span<float> samples);

private:
	struct state;
	explicit playback_device(std::unique_ptr<state> value);
	std::unique_ptr<state> state_;
};
} // namespace ice::audio
