#ifndef ICE_COMPANION_DEMO_H
#define ICE_COMPANION_DEMO_H
#include "Input/icCompanion.h"
inline icCompanionTake icCompanionDemo() {
    icCompanionSession session;session.StartRecording();
    icDeviceInput e;e.device=1;e.surface=1;e.contact=1;e.seconds=10;e.x=.35f;e.y=.85f;e.phase=icInputPhase::Begin;session.OnInput(e);
    e.seconds=10.3;e.y=.65f;e.phase=icInputPhase::Change;session.OnInput(e);
    e.seconds=10.6;e.y=.35f;e.phase=icInputPhase::End;session.OnInput(e);
    e.contact=2;e.seconds=11.2;e.x=.7f;e.y=.8f;e.phase=icInputPhase::Begin;session.OnInput(e);
    e.seconds=11.6;e.y=.5f;e.phase=icInputPhase::End;session.OnInput(e);
    session.StopRecording();return session.Take();
}
#endif
