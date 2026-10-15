#include "ice/input/companion_room.h"
#include <cstdio>
#include <cstdlib>
static void check(bool ok, const char *why) {
	if (!ok) {
		std::fprintf(stderr, "FAIL: %s\n", why);
		std::exit(1);
	}
}
int main() {
	ice::input::companion_room room;
	auto                       a = room.join("Alice", 1), b = room.join("Bob", 1);
	check(a && b && a != b, "distinct player identities");
	check(!room.start(b) && room.start(a), "only lobby leader starts");
	check(room.can_shoot(a) && !room.can_shoot(b), "server owns turn");
	auto turn = room.turn();
	check(room.begin_shot(a) && !room.begin_shot(a) && !room.begin_shot(b), "one ball at a time");
	check(!room.finish_shot(b, 100) && room.finish_shot(a, 50), "score belongs to shooter");
	check(room.current() == b && room.turn() != turn, "round-robin next turn");
	auto c = room.join("Late", 2);
	check(c && !room.can_shoot(c) && room.players().back().forfeited,
	      "late join waits for next round");
	check(room.begin_shot(b), "second player launches");
	room.leave(b);
	check(room.finish_shot(b, 20), "disconnected in-flight player keeps score");
	check(room.current() == a, "forfeited player skipped");
	for (int i = 0; i < 4; ++i) {
		check(room.begin_shot(a) && room.finish_shot(a, 10), "remaining turns");
	}
	check(room.state() == ice::input::companion_room::phase::results,
	      "round ends after five balls per eligible player");
	check(room.players()[0].score == 90 && room.players()[0].balls == 5 &&
	          room.players()[1].score == 20,
	      "independent totals");
	check(room.touch(b, 3) && room.start(a), "reconnected player can join next round");
	check(room.players()[0].score == 0 && !room.players()[1].forfeited &&
	          !room.players()[2].forfeited,
	      "round reset includes late arrivals");
	room.touch(b, 20);
	room.touch(c, 20);
	room.poll(20);
	check(room.current() == b, "timed-out current player cannot stall table");
	room.interrupt();
	check(!room.current() && !room.in_flight() && !room.can_shoot(b),
	      "host suspension stops round");
	ice::input::companion_room capacity;
	for (int i = 0; i < 8; ++i)
		check(capacity.join("Player", 1) != 0, "eight seats");
	check(!capacity.join("Ninth", 1), "capacity bound");
	capacity.leave(1);
	check(capacity.join("Replacement", 2) != 0, "offline seat reusable");
	std::puts("Companion room turns, scores, disconnects and capacity passed");
}
