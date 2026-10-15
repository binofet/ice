#ifndef IC_COMPANION_LINK_H
#define IC_COMPANION_LINK_H
#include "Input/icCompanion.h"
#include <memory>
#include <string>
// Host-thread-only, nonblocking IPv4 UDP companion connection. No background worker.
// Call Poll with a monotonic clock even when a desktop game has no active match.
class icCompanionLink {
public:
    enum class State { Closed, Listening, Pairing, Connected, Failed };
    icCompanionLink();
    ~icCompanionLink();
    icCompanionLink(const icCompanionLink&)=delete;
    icCompanionLink& operator=(const icCompanionLink&)=delete;
    bool Listen(unsigned short port, unsigned code, const char* bindAddress="0.0.0.0");
    bool Connect(const char* ipv4, unsigned short port, unsigned code, double now);
    void Close(); // destroys session and pending shot; reconnect never resends it
    void Poll(double now);
    bool SendShot(const icShotIntent& shot, double now);
    bool TakeShot(icShotIntent& shot, double now);
    void AcceptShots(bool accept); // false also discards queued game actions
    State Status() const;
    const std::string& Message() const;
    unsigned short Port() const;
    std::uint64_t PeerDevice() const; // session-scoped remote device identity; zero when disconnected
    unsigned Delivered() const;
    static std::string LocalAddresses();
private:
    struct Impl;
    std::unique_ptr<Impl> m;
};
#endif
