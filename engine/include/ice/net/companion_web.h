#pragma once
#include "ice/input/companion_room.h"
#include <memory>
#include <string>
namespace ice::net {
/// Optional local HTTP touch controller. Owns a native client connection to the
/// existing companion UDP host; gameplay does not depend on a browser transport.
class companion_web {
public:
	/// Create a stopped local browser controller; start explicitly before displaying a join URL.
	companion_web();
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	~companion_web();
	companion_web(const companion_web &)            = delete;
	companion_web &operator=(const companion_web &) = delete;
	/// Start the explicit subsystem/round operation; inspect its return status before using the
	/// active state.
	bool start(unsigned short companion_port, unsigned code);
	/// Consume pending native/network activity without blocking; supplied times are monotonic
	/// seconds.
	void poll(double now);
	/// Release the connection and discard pending session actions; reconnect does not replay an old
	/// shot.
	void close();
	/// Return the bound local port; zero denotes no active listener.
	unsigned short port() const;
	/// Publish host gameplay readiness to browser clients and gate shot acceptance.
	void set_game_ready(bool ready);
	/// Claim the browser player associated with the next eligible shot, using monotonic seconds.
	std::uint64_t claim_shot_owner(double now);
	/// Commit authoritative points for the in-flight player and advance the turn; false rejects a
	/// stale/mismatched shot.
	bool finish_shot(std::uint64_t player, unsigned points);
	/// Borrow the authoritative browser party room; the web host owns subsequent mutations.
	const ice::input::companion_room &room() const;
	/// Build the browser join URL for a supplied local address and this hosts listener/session.
	std::string join_url(const std::string &address) const;

private:
	struct impl;
	std::unique_ptr<impl> m_;
};

} // namespace ice::net
