#include "ice/input/companion.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#define CHECK(x)                                                                                   \
	do {                                                                                           \
		if (!(x)) {                                                                                \
			std::fprintf(stderr, "companion check failed %d: %s\n", __LINE__, #x);                 \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)
static ice::platform::device_input finger(ice::platform::input_phase phase, double t, float y,
                                          float x = .5f) {
	ice::platform::device_input e;
	e.device  = 1;
	e.surface = 1;
	e.contact = 7;
	e.phase   = phase;
	e.seconds = t;
	e.x       = x;
	e.y       = y;
	return e;
}
static ice::platform::device_input accel(double t, float z = 9.80665f) {
	ice::platform::device_input e;
	e.device  = 2;
	e.kind    = ice::platform::input_kind::acceleration;
	e.seconds = t;
	e.z       = z;
	return e;
}
int main(int argc, char **argv) {
	CHECK(argc == 2);
	ice::input::shot_gesture gesture;
	ice::input::shot_intent  shot;
	CHECK(!gesture.consume(finger(ice::platform::input_phase::begin, 1, .8f), shot));
	auto other    = finger(ice::platform::input_phase::end, 1.2, .1f);
	other.contact = 8;
	CHECK(!gesture.consume(other, shot) && gesture.armed());
	CHECK(gesture.consume(finger(ice::platform::input_phase::end, 1.3, .3f, .75f), shot));
	CHECK(std::abs(shot.power - .9f) < 1e-5 && shot.aim == .5f && !gesture.armed());
	gesture.consume(finger(ice::platform::input_phase::begin, 2, .8f), shot);
	CHECK(!gesture.consume(finger(ice::platform::input_phase::cancel, 2.3, .1f), shot));
	CHECK(!gesture.armed());
	gesture.consume(finger(ice::platform::input_phase::begin, 3, .8f), shot);
	CHECK(!gesture.consume(finger(ice::platform::input_phase::end, 3.2, .8f),
	                       shot)); // tap cannot launch
	gesture.consume(finger(ice::platform::input_phase::begin, 4, .8f), shot);
	CHECK(!gesture.consume(finger(ice::platform::input_phase::end, 14, .1f),
	                       shot)); // stale held input
	ice::input::gesture_config config;
	config.mode = ice::input::shot_mode::motion;
	gesture.reset(config);
	gesture.consume(finger(ice::platform::input_phase::begin, 15, .8f), shot);
	CHECK(!gesture.armed());
	CHECK(gesture.calibrate() == ice::input::calibration_result::need_samples);
	for (int i = 0; i < 40; ++i)
		gesture.consume(accel(20 + i / 60.), shot);
	CHECK(gesture.calibrate() == ice::input::calibration_result::ready &&
	      gesture.config().calibrated);
	gesture.consume(finger(ice::platform::input_phase::begin, 21, .8f), shot);
	for (int i = 0; i < 20; ++i)
		gesture.consume(accel(21 + i / 60., 20), shot);
	CHECK(gesture.consume(finger(ice::platform::input_phase::end, 21.34, .8f), shot) &&
	      shot.power > .5f && shot.source == ice::input::shot_mode::motion);
	gesture.consume(finger(ice::platform::input_phase::begin, 22, .8f), shot);
	for (int i = 0; i < 30; ++i)
		gesture.consume(accel(22 + i / 60.), shot);
	CHECK(!gesture.consume(finger(ice::platform::input_phase::end, 22.5, .8f),
	                       shot)); // gravity alone is not a throw
	gesture.reset(config);
	for (int i = 0; i < 40; ++i)
		gesture.consume(accel(30 + i / 60., i % 2 ? 12 : 8), shot);
	CHECK(gesture.calibrate() == ice::input::calibration_result::moving);
	ice::input::companion_session live;
	live.start_recording();
	live.on_input(finger(ice::platform::input_phase::begin, 100, .9f));
	live.on_input(finger(ice::platform::input_phase::change, 100.1, .7f));
	live.on_input(finger(ice::platform::input_phase::end, 100.3, .3f, .8f));
	live.on_input(finger(ice::platform::input_phase::begin, 101, .8f));
	live.on_input(finger(ice::platform::input_phase::cancel, 101.1, .1f));
	live.stop_recording();
	CHECK(live.shots().size() == 1);
	CHECK(live.take().save(argv[1]));
	CHECK(live.take().save(argv[1])); // replacing a completed take must work
	ice::input::companion_session replay;
	CHECK(replay.take().load(argv[1]));
	CHECK(replay.start_replay());
	replay.on_input(finger(ice::platform::input_phase::begin, 900,
	                       .9f)); // live input must not contaminate replay
	for (int i = 0; i < 200 && replay.replaying(); ++i)
		replay.advance_replay(.01);
	CHECK(!replay.replaying() && replay.replay_matched() && replay.shots().size() == 1);
	std::ofstream corrupt(argv[1]);
	corrupt << "ICE_COMPANION 1\n0 0 0 0 9.81\n18001 0\n";
	corrupt.close();
	CHECK(!replay.take().load(argv[1]) && replay.take().events.size() == 5);
	replay.take().expected[0].power = .1f;
	CHECK(replay.start_replay());
	replay.advance_replay(10);
	CHECK(!replay.replay_matched());
	live.set_mode(ice::input::shot_mode::motion);
	for (int i = 0; i < 40; ++i)
		live.on_input(accel(200 + i / 60.));
	CHECK(live.calibrate() == ice::input::calibration_result::ready);
	live.start_recording();
	live.on_input(finger(ice::platform::input_phase::begin, 201, .8f));
	for (int i = 0; i < 20; ++i)
		live.on_input(accel(201 + i / 60., 22));
	live.on_input(finger(ice::platform::input_phase::end, 201.34, .8f));
	live.stop_recording();
	CHECK(live.shots().size() == 1);
	CHECK(live.take().save(argv[1]));
	CHECK(replay.take().load(argv[1]));
	CHECK(replay.start_replay());
	replay.advance_replay(10);
	CHECK(replay.replay_matched());
	live.start_recording();
	live.on_input(finger(ice::platform::input_phase::begin, 300, .8f));
	live.suspend();
	CHECK(!live.recording() && !live.gesture().armed());
	live.deactivate();
	CHECK(!live.gesture().config().calibrated);
	ice::input::companion_session bounded;
	bounded.start_recording();
	for (int i = 0; i < 18001; ++i)
		bounded.on_input(accel(400 + i * .01));
	CHECK(!bounded.recording() && bounded.take().events.size() == 18000);
	CHECK(bounded.take().save(argv[1]));
	ice::input::companion_take invalid = bounded.take();
	invalid.events[0].seconds          = -1;
	CHECK(!invalid.save(argv[1]));
	CHECK(replay.take().load(argv[1]) && replay.take().events.size() == 18000);
	std::remove(argv[1]);
	std::puts("Companion checks passed: calibration, gestures, cancellation, bounded storage and "
	          "deterministic touch/motion replay");
}
