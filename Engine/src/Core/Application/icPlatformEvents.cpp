#include "Core/Application/icPlatformEvents.h"
#include <array>
#include <mutex>
namespace {
struct EventState {
    std::mutex mutex;
    std::array<icPlatformEvent, 1024> events;
    std::size_t head = 0, count = 0, dropped = 0;
    std::uint64_t nextWindow = 1, nextSequence = 1;
};
EventState& state() {
    // Process lifetime avoids static teardown order hazards with legacy global apps.
    // An explicit application owner can replace this during lifecycle migration.
    static EventState* instance = new EventState;
    return *instance;
}
}
icWindowID icPlatformEvents::AllocateWindowID() {
    auto& data = state();
    std::lock_guard<std::mutex> guard(data.mutex);
    return data.nextWindow++;
}
void icPlatformEvents::Post(icPlatformEventType type, icWindowID window) {
    auto& data = state();
    std::lock_guard<std::mutex> guard(data.mutex);
    if (data.count == data.events.size()) {
        ++data.dropped;
        return;
    }
    const std::size_t tail = (data.head + data.count) % data.events.size();
    data.events[tail] = {type, window, data.nextSequence++};
    ++data.count;
}
bool icPlatformEvents::Poll(icPlatformEvent& event) {
    auto& data = state();
    std::lock_guard<std::mutex> guard(data.mutex);
    if (!data.count) return false;
    event = data.events[data.head];
    data.head = (data.head + 1) % data.events.size();
    --data.count;
    return true;
}
std::size_t icPlatformEvents::DroppedEvents() {
    auto& data = state();
    std::lock_guard<std::mutex> guard(data.mutex);
    return data.dropped;
}
