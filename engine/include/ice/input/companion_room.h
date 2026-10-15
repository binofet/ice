#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace ice::input {
/// Host-thread party-game reducer. Player IDs are session-local, times are monotonic seconds, and
/// scoring is authoritative on the host.
class companion_room {
public:
	/// Party-room phase: lobby, active round, or completed results.
	enum class phase { lobby, playing, results };
	/// Session-local party player, including online presence and authoritative score/ball totals.
	struct player {
		/// Owner-assigned identity; zero denotes no active binding.
		std::uint64_t id = 0;
		/// Human-readable identifier; the enclosing object owns the string.
		std::string name;
		/// Host-authoritative number of completed/consumed balls. Host-authoritative accumulated
		/// point total.
		unsigned score = 0, balls = 0;
		/// Whether the current round marks this player as forfeited. Whether the player currently
		/// has recent host-visible activity.
		bool online = true, forfeited = false;
		/// Last observed activity time in monotonic seconds.
		double seen = 0;
	};
	/// Add/update a party player using a monotonic timestamp; return a session-local identity or
	/// zero on rejection.
	std::uint64_t join(const std::string &name, double now);
	/// Refresh a players last-seen monotonic time; false rejects an unknown identity.
	bool touch(std::uint64_t id, double now);
	/// Mark a player offline and update turn eligibility.
	void leave(std::uint64_t id);
	/// Consume pending native/network activity without blocking; supplied times are monotonic
	/// seconds.
	void poll(double now);
	/// Start the explicit subsystem/round operation; inspect its return status before using the
	/// active state.
	bool start(std::uint64_t leader);
	/// Claim the current players shot; false rejects an invalid turn or an already in-flight shot.
	bool begin_shot(std::uint64_t player);
	/// Commit authoritative points for the in-flight player and advance the turn; false rejects a
	/// stale/mismatched shot.
	bool finish_shot(std::uint64_t player, unsigned points);
	/// Return whether this player currently owns an eligible turn with no shot in flight.
	bool can_shoot(std::uint64_t player) const;
	/// Invalidate an unfinished round after host suspension so stale actions cannot score.
	void interrupt(); // invalidate an unfinished round after host suspension
	/// Return the current lifecycle/reducer state without transferring ownership.
	phase state() const { return phase_; }
	/// Return the current eligible player identity, or zero when no player owns a turn.
	std::uint64_t current() const;
	/// Return the current online room-leader identity, or zero when none is available.
	std::uint64_t leader() const;
	/// Return the current authoritative turn counter.
	unsigned turn() const { return turn_; }
	/// Return the current authoritative round counter.
	unsigned round() const { return round_; }
	/// Return whether a claimed shot is awaiting its authoritative result.
	bool in_flight() const { return shooter_ != 0; }
	/// Borrow authoritative player records; future joins/updates may invalidate references.
	const std::vector<player> &players() const { return players_; }

private:
	player             *find(std::uint64_t id);
	void                advance();
	std::vector<player> players_;
	phase               phase_ = phase::lobby;
	std::uint64_t       next_ = 1, current_ = 0, shooter_ = 0;
	unsigned            round_ = 0, turn_ = 0;
};

} // namespace ice::input
