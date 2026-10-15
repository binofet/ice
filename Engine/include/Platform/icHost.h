#ifndef IC_HOST_H
#define IC_HOST_H
#include <atomic>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <vector>

using icSurfaceID = std::uint64_t;
using icDeviceID = std::uint64_t;
enum class icAppState { Inactive, Active, Background, Stopped };
enum class icSurfaceAPI { Software, OpenGL, Metal, Vulkan };
struct icSurface {
    icSurfaceID id = 0;
    icSurfaceAPI api = icSurfaceAPI::Software;
    // Borrowed; the platform keeps it alive through OnSurfaceRemoved.
    void* nativeHandle = nullptr;
    int width = 0, height = 0;             // drawable pixels, not UI points
    float scale = 1;
    bool visible = true;
};
enum class icInputKind { Pointer, Key, Acceleration, AngularVelocity, Attitude };
enum class icInputPhase { Begin, Change, End, Cancel, Sample };
inline std::uint32_t icInputCapability(icInputKind kind) { return 1u << static_cast<unsigned>(kind); }
struct icInputDevice {
    icDeviceID id = 0;
    std::uint32_t capabilities = 0;
};
struct icDeviceInput {
    icDeviceID device = 0;
    icSurfaceID surface = 0;              // zero is application-wide, e.g. a sensor
    std::uint64_t contact = 0;            // touch identity or key code
    double seconds = 0;                  // host monotonic clock, in seconds
    icInputKind kind = icInputKind::Pointer;
    icInputPhase phase = icInputPhase::Sample;
    // Pointer: normalized top-left x/y (may exceed 0..1 during a drag).
    // Motion: natural device axes, X right/Y top/Z out; acceleration is specific force
    // in m/s^2 including gravity (+Z when stationary face-up), gyro is rad/s.
    float x = 0, y = 0, z = 0, w = 0;
};
class icHostClient {
public:
    virtual ~icHostClient() {}
    virtual void OnAppState(icAppState) {}
    virtual void OnSurfaceAdded(const icSurface&) {}
    virtual void OnSurfaceChanged(const icSurface&) {}
    virtual void OnSurfaceRemoved(const icSurface&) {}
    virtual void OnDeviceAdded(const icInputDevice&) {}
    virtual void OnDeviceRemoved(icDeviceID) {}
    virtual void OnInput(const icDeviceInput&) {}
    virtual void OnUpdate(double) {}
    virtual void OnRender(const icSurface&) {}
};
// One explicit application owner; no native window, singleton, renderer, or loop required.
// All methods/callbacks are host-thread-only except PostInput. Do not mutate the host
// inside callbacks. Stop explicitly while client and native handles are still alive.
class icHost {
public:
    explicit icHost(icHostClient& client): m_client(client) {}
    bool SetState(icAppState state);
    icAppState State() const { return m_state; }
    icSurfaceID AddSurface(icSurface surface);
    bool ResizeSurface(icSurfaceID id, int width, int height, float scale);
    bool RemoveSurface(icSurfaceID id);
    bool SetSurfaceVisible(icSurfaceID id, bool visible);
    icDeviceID AddDevice(std::uint32_t capabilities);
    bool RemoveDevice(icDeviceID id);
    bool PostInput(const icDeviceInput& event);
    void Frame(double monotonicSeconds);
    void Stop();
    std::uint64_t DroppedInput() const { return m_dropped.load(); }
private:
    void CancelInputs(icDeviceID device = 0, icSurfaceID surface = 0);
    void DrainInput();
    icHostClient& m_client;
    icAppState m_state = icAppState::Inactive;
    std::map<icSurfaceID,icSurface> m_surfaces;
    std::map<icDeviceID,icInputDevice> m_devices;
    std::vector<icDeviceInput> m_held;
    std::mutex m_mutex;
    std::deque<icDeviceInput> m_pending;
    std::atomic<bool> m_acceptInput{false};
    std::atomic<std::uint64_t> m_dropped{0};
    bool m_overflow = false, m_clockValid = false;
    double m_previous = 0;
    std::uint64_t m_nextSurface = 1, m_nextDevice = 1;
};
#endif
