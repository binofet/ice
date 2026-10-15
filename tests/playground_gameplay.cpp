#include "GameModel.h"
#include <cmath>
#include <cstdio>
#define CHECK(x)                                                                                   \
	do {                                                                                           \
		if (!(x)) {                                                                                \
			std::fprintf(stderr, "failed line %d: %s\n", __LINE__, #x);                            \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)
using namespace playground;
int main() {
	Simulation salvage(0);
	salvage.player = salvage.cargo[0].pos;
	Input hook;
	hook.action = true;
	salvage.update(.01f, hook);
	CHECK(salvage.attached == 0);
	salvage.update(.01f, hook);
	CHECK(salvage.attached == -1);
	for (auto &c : salvage.cargo) {
		c.pos      = Vec();
		c.velocity = Vec();
	}
	salvage.update(.01f, Input());
	CHECK(salvage.won && salvage.score == 3);
	Simulation stealth(1);
	stealth.player = Vec(11, -11);
	stealth.update(.01f, Input());
	CHECK(stealth.score == 1);
	stealth.player = Vec(-11, 10);
	stealth.update(.01f, Input());
	CHECK(stealth.won);
	stealth.reset();
	stealth.player = Vec(-5.3f, 0);
	Input right;
	right.turn = 1;
	stealth.update(.2f, right);
	CHECK(stealth.player.x == -5.3f);
	stealth.reset();
	for (int i = 0; i < 240 && !stealth.lost; ++i) {
		stealth.player = stealth.guards[0];
		stealth.update(1.f / 60, Input());
	}
	CHECK(stealth.lost);
	Simulation kart(2);
	for (int i = 0; i < 12; ++i) {
		kart.player = kart.target();
		kart.update(.01f, Input());
	}
	CHECK(kart.won && kart.laps == 3);
	kart.reset();
	Input throttle;
	throttle.forward = 1;
	for (int i = 0; i < 30; ++i)
		kart.update(1.f / 60, throttle);
	CHECK(kart.player.z < 10 && kart.velocity.x > 0);
	Simulation voxel(3);
	CHECK(!voxel.voxels[8][1][8]);
	voxel.update(.01f, hook);
	CHECK(voxel.voxels[8][1][8]);
	voxel.update(.01f, hook);
	CHECK(!voxel.voxels[8][1][8]);
	std::puts("Playground gameplay checks passed");
	return 0;
}
