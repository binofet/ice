#ifndef IC_PLATFORM_EVENTS_H
#define IC_PLATFORM_EVENTS_H
#include <cstdint>
#include <cstddef>

using icWindowID = std::uint64_t;
// Zero denotes application scope. A native root window is not required.
constexpr icWindowID IC_APPLICATION_SCOPE = 0;

enum class icPlatformEventType {
    WindowRegistered,
    WindowRetired,
    CloseRequested,
    FocusGained,
    FocusLost,
    ApplicationQuitRequested
};

struct icPlatformEvent {
    icPlatformEventType type;
    icWindowID window;
    std::uint64_t sequence;
};

// A bounded application event stream. Poll once, then route by window ID.
// It supplements the legacy input singleton; it does not yet replace it.
class icPlatformEvents {
public:
    static icWindowID AllocateWindowID();
    static void Post(icPlatformEventType type, icWindowID window = IC_APPLICATION_SCOPE);
    static bool Poll(icPlatformEvent& event);
    static std::size_t DroppedEvents();
};
#endif
