#ifndef IC_COMPANION_H
#define IC_COMPANION_H
#include "Platform/icHost.h"
#include <deque>
#include <string>
#include <vector>

enum class icShotMode { Touch, Motion };
struct icShotIntent {
    float aim = 0, power = 0;
    double seconds = 0;
    icShotMode source = icShotMode::Touch;
};
struct icGestureConfig {
    icShotMode mode = icShotMode::Touch;
    bool calibrated = false;
    float gravityX = 0, gravityY = 0, gravityZ = 9.80665f;
};
enum class icCalibrationResult { Ready, NeedSamples, Moving };
// Produces game intent, not a reconstructed physical phone trajectory.
class icShotGesture {
public:
    void Reset(const icGestureConfig& config);
    void Cancel();
    icCalibrationResult Calibrate();
    bool Consume(const icDeviceInput& event, icShotIntent& shot);
    const icGestureConfig& Config() const { return m_config; }
    bool Armed() const { return m_armed; }
    float Power() const;
    float Aim() const { return m_aim; }
private:
    icGestureConfig m_config;
    std::deque<icDeviceInput> m_samples;
    bool m_armed = false;
    icDeviceInput m_contact;
    float m_startY = 0, m_aim = 0, m_touchPower = 0, m_filtered = 0, m_peak = 0, m_gyro = 0;
    double m_lastMotion = -1, m_peakTime = -1, m_lastPointer = -1;
};
struct icCompanionTake {
    icGestureConfig config;
    std::vector<icDeviceInput> events;
    std::vector<icShotIntent> expected;
    bool Save(const std::string& path) const;
    bool Load(const std::string& path); // validates into a candidate; preserves current take on error
};
// Host-thread consumer. Replay drives the same reducer with original sample timestamps,
// in recorded arrival order. Live sensor/touch input is ignored during replay.
class icCompanionSession {
public:
    void OnInput(const icDeviceInput& event);
    void SetMode(icShotMode mode);
    icCalibrationResult Calibrate();
    void StartRecording();
    void StopRecording() { m_recording = false; }
    bool StartReplay();
    void AdvanceReplay(double seconds);
    void Suspend();
    void Deactivate(); // OS suspension also invalidates the physical calibration pose
    bool Recording() const { return m_recording; }
    bool Replaying() const { return m_replaying; }
    bool ReplayMatched() const { return m_replayMatched; }
    const icShotGesture& Gesture() const { return m_gesture; }
    const std::vector<icShotIntent>& Shots() const { return m_shots; }
    icCompanionTake& Take() { return m_take; }
private:
    void Consume(const icDeviceInput& event, bool record);
    icShotGesture m_gesture;
    icCompanionTake m_take;
    std::vector<icShotIntent> m_shots;
    bool m_recording = false, m_replaying = false, m_replayMatched = false;
    std::size_t m_replayIndex = 0;
    double m_replayTime = 0, m_deadline = 0;
};
#endif
