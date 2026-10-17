#ifndef ICE_COMPANION_DEMO_H
#define ICE_COMPANION_DEMO_H
#include "ice/input/companion.h"
inline ice::input::companion_take icCompanionDemo() {
	ice::input::companion_session session;
	session.start_recording();
	ice::platform::device_input e;
	e.device  = 1;
	e.surface = 1;
	e.contact = 1;
	e.seconds = 10;
	e.x       = .35f;
	e.y       = .85f;
	e.phase   = ice::platform::input_phase::begin;
	session.on_input(e);
	e.seconds = 10.3;
	e.y       = .65f;
	e.phase   = ice::platform::input_phase::change;
	session.on_input(e);
	e.seconds = 10.6;
	e.y       = .35f;
	e.phase   = ice::platform::input_phase::end;
	session.on_input(e);
	e.contact = 2;
	e.seconds = 11.2;
	e.x       = .7f;
	e.y       = .8f;
	e.phase   = ice::platform::input_phase::begin;
	session.on_input(e);
	e.seconds = 11.6;
	e.y       = .5f;
	e.phase   = ice::platform::input_phase::end;
	session.on_input(e);
	session.stop_recording();
	return session.take();
}
#endif
