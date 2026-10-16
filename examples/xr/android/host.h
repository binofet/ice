#pragma once
#include "../view_camera.h"
#include "ice/render/device.h"
#include <array>
#include <cstdint>
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
};
/// One native frame's immutable observations, shared by both eyes; no application clock is owned.
struct frame_observation {
	std::uint64_t                    index             = 0;
	double                           predicted_seconds = 0, elapsed_seconds = 0;
	std::array<controller_sample, 2> controllers;
	/// Tracked head pose in canonical application-world coordinates; use only when tracked.
	ice::render::camera_pose head;
	bool                     focused = false, tracked = false;
	bool                     recording = false, permission_needed = false, clip_saved = false;
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
