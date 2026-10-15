#pragma once
#include "ice/platform/host.h"
#include <deque>
#include <string>
#include <vector>
namespace ice::input {

/// Shot reducer input mode: touch drag or calibrated motion.
enum class shot_mode { touch, motion };
/// Game-level shot intent: normalized aim/power and original monotonic sample time; not an absolute
/// physical trajectory.
struct shot_intent {
	/// Normalized shot strength in [0,1]. Normalized horizontal shot aim; interpreted by the game,
	/// not an absolute pose.
	float aim = 0, power = 0;
	/// Original monotonic sample timestamp in seconds.
	double seconds = 0;
	/// Origin URI/source payload or input mode, as specified by the enclosing record.
	shot_mode source = shot_mode::touch;
};
/// Gesture reducer configuration. Gravity is calibrated device-frame acceleration in metres per
/// second squared.
struct gesture_config {
	/// Selected touch/motion input mode.
	shot_mode mode = shot_mode::touch;
	/// Whether the reducer has accepted a stationary motion calibration.
	bool calibrated = false;
	/// Calibrated device-frame gravity Z in metres per second squared. Calibrated device-frame
	/// gravity Y in metres per second squared. Calibrated device-frame gravity X in metres per
	/// second squared.
	float gravity_x = 0, gravity_y = 0, gravity_z = 9.80665f;
};
/// Motion calibration outcome; callers can request more samples or wait for a stationary device.
enum class calibration_result { ready, need_samples, moving };
/// Produces game intent, not a reconstructed physical phone trajectory.
class shot_gesture {
public:
	/// Discard reducer history and apply the supplied configuration.
	void reset(const gesture_config &config);
	/// Cancel the current gesture without producing a shot.
	void cancel();
	/// Estimate stationary gravity from recent samples; report ready, insufficient samples, or
	/// movement.
	calibration_result calibrate();
	/// Reduce one timestamped input sample; true writes a completed shot to the output.
	bool consume(const ice::platform::device_input &event, shot_intent &shot);
	/// Borrow the active reducer configuration for the lifetime of the owning session.
	const gesture_config &config() const { return config_; }
	/// Return whether a gesture is currently armed to produce a shot.
	bool armed() const { return armed_; }
	/// Return the current normalized shot-strength estimate in [0,1].
	float power() const;
	/// Return the current normalized horizontal shot-aim estimate.
	float aim() const { return aim_; }

private:
	gesture_config                          config_;
	std::deque<ice::platform::device_input> samples_;
	bool                                    armed_ = false;
	ice::platform::device_input             contact_;
	float  start_y_ = 0, aim_ = 0, touch_power_ = 0, filtered_ = 0, peak_ = 0, gyro_ = 0;
	double last_motion_ = -1, peak_time_ = -1, last_pointer_ = -1;
};
/// Owned input recording and expected shot sequence, serialized with original sample timestamps for
/// deterministic replay.
struct companion_take {
	/// Configuration captured for recording/replay.
	gesture_config config;
	/// Owned input samples retained in original arrival order.
	std::vector<ice::platform::device_input> events;
	/// Owned expected shot intents used to validate replay.
	std::vector<shot_intent> expected;
	/// Serialize current owned state to the supplied path; inspect the return status for write
	/// failure.
	bool save(const std::string &path) const;
	bool
	/// Load into owned state; inspect the return status before using the resulting data.
	load(const std::string &path); // validates into a candidate; preserves current take on error
};
/// Host-thread consumer. Replay drives the same reducer with original sample timestamps,
/// in recorded arrival order. Live sensor/touch input is ignored during replay.
class companion_session {
public:
	/// Consume the borrowed input sample on the owning thread; recorded timestamps retain their
	/// original units.
	void on_input(const ice::platform::device_input &event);
	/// Select touch/motion reduction and cancel incompatible in-progress gesture state.
	void set_mode(shot_mode mode);
	/// Estimate stationary gravity from recent samples; report ready, insufficient samples, or
	/// movement.
	calibration_result calibrate();
	/// Begin retaining input samples and expected shot outputs for replay.
	void start_recording();
	/// Stop retaining live samples without discarding the completed take.
	void stop_recording() { recording_ = false; }
	/// Start replaying the retained take; false rejects a missing or invalid take.
	bool start_replay();
	/// Advance recorded input using elapsed seconds while preserving original event arrival order.
	void advance_replay(double seconds);
	/// Cancel held/armed gesture state and stop in-progress replay activity.
	void suspend();
	/// Suspend the session and invalidate physical motion calibration after OS deactivation.
	void deactivate(); // OS suspension also invalidates the physical calibration pose
	/// Return whether live input is currently being retained.
	bool recording() const { return recording_; }
	/// Return whether recorded samples currently drive the reducer.
	bool replaying() const { return replaying_; }
	/// Return whether replayed shots have matched the recorded expected output so far.
	bool replay_matched() const { return replay_matched_; }
	/// Borrow the gesture reducer for inspection; mutation remains owned by the session.
	const shot_gesture &gesture() const { return gesture_; }
	/// Borrow the accumulated shot results; later session updates may invalidate references.
	const std::vector<shot_intent> &shots() const { return shots_; }
	/// Borrow the owned recording; edits affect subsequent replay.
	companion_take &take() { return take_; }

private:
	void                     consume(const ice::platform::device_input &event, bool record);
	shot_gesture             gesture_;
	companion_take           take_;
	std::vector<shot_intent> shots_;
	bool                     recording_ = false, replaying_ = false, replay_matched_ = false;
	std::size_t              replay_index_ = 0;
	double                   replay_time_ = 0, deadline_ = 0;
};

} // namespace ice::input
