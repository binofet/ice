#pragma once
#include "ice/input/companion.h"
#include <memory>
#include <string>
namespace ice::net {
/// Host-thread-only, nonblocking IPv4 UDP companion connection. No background worker.
/// Call poll with a monotonic clock even when a desktop game has no active match.
class companion_link {
public:
	/// Connection lifecycle; failed retains a diagnostic until explicitly reopened.
	enum class state { closed, listening, pairing, connected, failed };
	/// Create a closed companion link; listen/connect explicitly before polling session actions.
	companion_link();
	/// Close the socket and release pending local protocol state on the host thread.
	~companion_link();
	companion_link(const companion_link &)            = delete;
	companion_link &operator=(const companion_link &) = delete;
	/// Open a nonblocking IPv4 UDP host with a pairing code; false records a socket/bind failure.
	bool listen(unsigned short port, unsigned code, const char *bind_address = "0.0.0.0");
	/// Begin nonblocking IPv4 pairing using a monotonic timestamp; inspect status/message while
	/// polling.
	bool connect(const char *ipv4, unsigned short port, unsigned code, double now);
	/// Release the connection and discard pending session actions; reconnect does not replay an old
	/// shot.
	void close(); // destroys session and pending shot; reconnect never resends it
	/// Consume pending native/network activity without blocking; supplied times are monotonic
	/// seconds.
	void poll(double now);
	/// Queue one shot for reliable session delivery; false rejects disconnected, stale, or already-
	/// pending actions.
	bool send_shot(const ice::input::shot_intent &shot, double now);
	/// Consume one accepted pending shot into caller storage; false means none is currently
	/// available.
	bool take_shot(ice::input::shot_intent &shot, double now);
	/// Enable/disable gameplay shot acceptance; disabling also discards queued actions.
	void accept_shots(bool accept); // false also discards queued game actions
	/// Return current connection state without blocking.
	state status() const;
	/// Borrow the latest human-readable connection diagnostic; the next poll/state change may
	/// replace it.
	const std::string &message() const;
	/// Return the bound local port; zero denotes no active listener.
	unsigned short port() const;
	std::uint64_t
	/// Return the session-scoped peer identity; zero denotes no connected peer.
	peer_device() const; // session-scoped remote device identity; zero when disconnected
	/// Return the cumulative count of acknowledged/delivered session actions.
	unsigned delivered() const;
	/// Return local IPv4 addresses suitable for displaying a companion join endpoint.
	static std::string local_addresses();

private:
	struct impl;
	std::unique_ptr<impl> m_;
};

} // namespace ice::net
