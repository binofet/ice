#pragma once
#include "../view_camera.h"
#include "ice/audio/recording_store.h"
#include "ice/render/device.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

struct android_app;
namespace ice::examples::xr {
/// Tracked grip in application world coordinates; invalid samples never imply a held control.
struct controller_sample {
	ice::math::mat4 transform{1};
	float           trigger = 0;
	bool            valid   = false;
	/// Optional left-Y/right-A action; left-X remains exclusively push-to-talk.
	bool primary = false;
	/// Native Touch thumbstick axes in [-1,1]; neutral when inactive/unfocused.
	ice::math::vec2 thumbstick{0};
	/// Whether a focused finite native thumbstick action is active. Inactive is not neutral
	/// release: clients must cancel/rearm pending actions rather than committing them.
	bool thumbstick_active = false;
	/// Native squeeze pressure in [0,1], distinct from the index trigger.
	float squeeze = 0;
	/// Tracked native aim pose in application world coordinates, independent of grip tracking.
	ice::math::mat4 aim_transform{1};
	/// Whether aim_transform is a valid tracked sample; never synthesize aim from the grip.
	bool aim_valid = false;
	/// Native trigger availability. Inactive is action loss, not a physical neutral release.
	bool trigger_active = false;
	/// Native squeeze availability. Pending grip gestures must cancel on action loss.
	bool squeeze_active = false;
};
/// Compose a physical room anchor with application-owned translation/yaw navigation.
/// Reject nonfinite, nonrigid or tilted navigation; the physical anchor is never modified.
/// The resulting matrix maps application world coordinates into native reference space.
inline ice::expected<ice::math::mat4, ice::render::diagnostic>
make_world_reference(const ice::math::mat4 &reference_from_room,
                     const ice::math::mat4 &world_from_room) {
	using namespace ice::math;
	auto rigid = [](const mat4 &pose) {
		for (float value : elements(pose))
			if (!std::isfinite(value))
				return false;
		for (int n = 0; n < 3; ++n)
			if (std::abs(glm::length(vec3(pose[n])) - 1.f) > 1e-4f || std::abs(pose[n][3]) > 1e-5f)
				return false;
		return std::abs(pose[3][3] - 1.f) < 1e-5f &&
		       std::abs(glm::determinant(glm::mat3(pose)) - 1.f) < 1e-4f &&
		       std::abs(glm::dot(vec3(pose[0]), vec3(pose[1]))) < 1e-4f &&
		       std::abs(glm::dot(vec3(pose[0]), vec3(pose[2]))) < 1e-4f &&
		       std::abs(glm::dot(vec3(pose[1]), vec3(pose[2]))) < 1e-4f;
	};
	if (!rigid(reference_from_room) || !rigid(world_from_room) ||
	    glm::length(vec3(world_from_room[1]) - vec3(0, 1, 0)) > 1e-4f)
		return ice::unexpected(ice::render::diagnostic{
		    "Invalid room anchor or navigation: require rigid yaw and translation"});
	auto result = reference_from_room * glm::inverse(world_from_room);
	if (!rigid(result))
		return ice::unexpected(
		    ice::render::diagnostic{"Navigation reference cannot be represented"});
	return result;
}
/// Capture lifecycle observed on the native owner thread. Failure/cancellation never implies a
/// clip.
enum class voice_status { idle, began, recording, completed, cancelled, failed };
/// One native frame's immutable observations, shared by both eyes; no application clock is owned.
struct frame_observation {
	std::uint64_t                    index             = 0;
	double                           predicted_seconds = 0, elapsed_seconds = 0;
	std::array<controller_sample, 2> controllers;
	/// Tracked head pose in canonical application-world coordinates; use only when tracked.
	ice::render::camera_pose head;
	bool                     focused = false, tracked = false;
	bool                     recording = false, permission_needed = false, clip_saved = false;
	/// Native steady-clock sample time in nanoseconds, distinct from OpenXR predicted_seconds.
	/// Pair these frame samples with receipt start/end on this same host process clock only.
	std::uint64_t monotonic_ns = 0;
	/// Current/last take identity, allocated before microphone start; empty if permission or
	/// reservation failed first. It remains until the next explicit PTT attempt.
	std::string recording_id;
	/// Host stream-start bracket in steady nanoseconds; not a hardware acoustic timestamp.
	std::uint64_t recording_started_ns = 0;
	/// Exact host terminal stamp even if focus loss prevents frame delivery until resume.
	/// Zero while active; a failed pre-start attempt may have no recording ID/start stamp.
	std::uint64_t recording_ended_ns = 0;
	/// Lifecycle state plus a monotonic sequence incremented for each begin/terminal transition.
	voice_status  voice_state          = voice_status::idle;
	std::uint64_t voice_event_sequence = 0;
	/// Bounded terminal failure/cancellation reason; never interpreted as executable content.
	std::string voice_diagnostic;
	/// Last successfully committed clip, retained until the next PTT attempt. Paths are relative
	/// to Android internalDataPath (recordings/<id>/...). Deduplicate by recording_id. No queue,
	/// network, automatic acknowledgment, deletion or transcription is implied.
	std::optional<ice::audio::recording_receipt> voice_receipt;
};
/// Synchronous borrowed application boundary. NativeActivity's render thread owns every call.
/// It supplies real local application behavior; the adapter owns no game, physics or provider.
class android_client {
public:
	virtual ~android_client() = default;
	/// Prepare exact destination-format resources before readiness. Runtime color remains borrowed.
	virtual ice::expected<void, ice::render::diagnostic>
	prepare(ice::render::color_format format) = 0;
	/// Report foreground/focused state; cancel held controls and suspend/resume the one
	/// application.
	virtual void lifecycle(bool active) = 0;
	/// Poll bounded nonblocking native services on the owner thread after lifecycle events,
	/// before the next XR frame. Called while rendering is idle too (100ms idle looper wait),
	/// without update(), input sampling or simulation advancement. Honor lifecycle(false):
	/// process transport/heartbeat/receipt work, but do not start capture or perform suspended
	/// world mutation/GPU work. Never retain borrowed native frame resources here. Exceptions
	/// are contained by the host; this is not a background thread or an Android foreground
	/// service guarantee, and OS process suspension/termination still stops polling.
	virtual void poll_services() {}
	/// Application-owned rigid yaw/translation from retained physical room to application world.
	/// Called before update and again afterward; teleport/snap applies to both eyes that frame.
	/// It must not advance simulation, mutate anchors or perform native graphics work.
	virtual ice::math::mat4 world_from_room() const { return ice::math::mat4(1); }
	/// Advance at most once per native frame, before either eye, including frames with no layer.
	virtual ice::expected<void, ice::render::diagnostic> update(const frame_observation &frame) = 0;
	/// Return borrowed draws valid until the synchronous encoder completes, before the next eye.
	virtual ice::expected<ice::render::frame_batch, ice::render::diagnostic>
	frame(const ice::render::camera_view &camera, const frame_observation &observations) = 0;
};
/// Run the existing bounded OpenXR/Android lifecycle, retained anchoring, controllers and X-held
/// private microphone path. Borrow a GLES device/client that outlive this call; retire all native
/// swapchains/session before returning. The function never owns/destroys the borrowed EGL root.
/// Finishes this Activity on normal exit or contained failure. No install, network or ASR occurs.
void run_android(android_app &app, ice::render::device &device, android_client &client,
                 std::string_view application_name = "ICE XR Lab");
/// Clear retired client callbacks, request Activity finish and drain native-app-glue commands
/// until Android acknowledges destruction. Required even if device/client construction fails:
/// Java onStart/onResume/window callbacks synchronously wait for their glue acknowledgements.
/// Call on the android_main thread; no OpenXR, EGL or application owner is required.
void finish_android_activity(android_app &app) noexcept;
/// Add a bounded application record to this host's existing private trace/logcat destination.
/// Render-thread only; diagnostic failures are contained and never change command publication.
void write_diagnostic(std::string_view message) noexcept;
} // namespace ice::examples::xr
