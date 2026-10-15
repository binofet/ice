#include "Input/icCompanion.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"companion check failed %d: %s\n",__LINE__,#x);return 1;}}while(0)
static icDeviceInput finger(icInputPhase phase,double t,float y,float x=.5f){icDeviceInput e;e.device=1;e.surface=1;e.contact=7;e.phase=phase;e.seconds=t;e.x=x;e.y=y;return e;}
static icDeviceInput accel(double t,float z=9.80665f){icDeviceInput e;e.device=2;e.kind=icInputKind::Acceleration;e.seconds=t;e.z=z;return e;}
int main(int argc,char**argv){
    CHECK(argc==2);icShotGesture gesture;icShotIntent shot;
    CHECK(!gesture.Consume(finger(icInputPhase::Begin,1,.8f),shot));
    auto other=finger(icInputPhase::End,1.2,.1f);other.contact=8;CHECK(!gesture.Consume(other,shot)&&gesture.Armed());
    CHECK(gesture.Consume(finger(icInputPhase::End,1.3,.3f,.75f),shot));CHECK(std::abs(shot.power-.9f)<1e-5&&shot.aim==.5f&&!gesture.Armed());
    gesture.Consume(finger(icInputPhase::Begin,2,.8f),shot);CHECK(!gesture.Consume(finger(icInputPhase::Cancel,2.3,.1f),shot));CHECK(!gesture.Armed());
    gesture.Consume(finger(icInputPhase::Begin,3,.8f),shot);CHECK(!gesture.Consume(finger(icInputPhase::End,3.2,.8f),shot)); // tap cannot launch
    gesture.Consume(finger(icInputPhase::Begin,4,.8f),shot);CHECK(!gesture.Consume(finger(icInputPhase::End,14,.1f),shot)); // stale held input
    icGestureConfig config;config.mode=icShotMode::Motion;gesture.Reset(config);
    gesture.Consume(finger(icInputPhase::Begin,15,.8f),shot);CHECK(!gesture.Armed());
    CHECK(gesture.Calibrate()==icCalibrationResult::NeedSamples);
    for(int i=0;i<40;++i)gesture.Consume(accel(20+i/60.),shot);
    CHECK(gesture.Calibrate()==icCalibrationResult::Ready&&gesture.Config().calibrated);
    gesture.Consume(finger(icInputPhase::Begin,21,.8f),shot);
    for(int i=0;i<20;++i)gesture.Consume(accel(21+i/60.,20),shot);
    CHECK(gesture.Consume(finger(icInputPhase::End,21.34,.8f),shot)&&shot.power>.5f&&shot.source==icShotMode::Motion);
    gesture.Consume(finger(icInputPhase::Begin,22,.8f),shot);
    for(int i=0;i<30;++i)gesture.Consume(accel(22+i/60.),shot);
    CHECK(!gesture.Consume(finger(icInputPhase::End,22.5,.8f),shot)); // gravity alone is not a throw
    gesture.Reset(config);for(int i=0;i<40;++i)gesture.Consume(accel(30+i/60.,i%2?12:8),shot);
    CHECK(gesture.Calibrate()==icCalibrationResult::Moving);
    icCompanionSession live;live.StartRecording();
    live.OnInput(finger(icInputPhase::Begin,100,.9f));live.OnInput(finger(icInputPhase::Change,100.1,.7f));live.OnInput(finger(icInputPhase::End,100.3,.3f,.8f));
    live.OnInput(finger(icInputPhase::Begin,101,.8f));live.OnInput(finger(icInputPhase::Cancel,101.1,.1f));live.StopRecording();CHECK(live.Shots().size()==1);
    CHECK(live.Take().Save(argv[1]));CHECK(live.Take().Save(argv[1])); // replacing a completed take must work
    icCompanionSession replay;CHECK(replay.Take().Load(argv[1]));CHECK(replay.StartReplay());
    replay.OnInput(finger(icInputPhase::Begin,900,.9f)); // live input must not contaminate replay
    for(int i=0;i<200&&replay.Replaying();++i)replay.AdvanceReplay(.01);
    CHECK(!replay.Replaying()&&replay.ReplayMatched()&&replay.Shots().size()==1);
    std::ofstream corrupt(argv[1]);corrupt<<"ICE_COMPANION 1\n0 0 0 0 9.81\n18001 0\n";corrupt.close();
    CHECK(!replay.Take().Load(argv[1])&&replay.Take().events.size()==5);
    replay.Take().expected[0].power=.1f;CHECK(replay.StartReplay());replay.AdvanceReplay(10);CHECK(!replay.ReplayMatched());
    live.SetMode(icShotMode::Motion);for(int i=0;i<40;++i)live.OnInput(accel(200+i/60.));CHECK(live.Calibrate()==icCalibrationResult::Ready);
    live.StartRecording();live.OnInput(finger(icInputPhase::Begin,201,.8f));
    for(int i=0;i<20;++i)live.OnInput(accel(201+i/60.,22));live.OnInput(finger(icInputPhase::End,201.34,.8f));live.StopRecording();CHECK(live.Shots().size()==1);
    CHECK(live.Take().Save(argv[1]));CHECK(replay.Take().Load(argv[1]));CHECK(replay.StartReplay());replay.AdvanceReplay(10);CHECK(replay.ReplayMatched());
    live.StartRecording();live.OnInput(finger(icInputPhase::Begin,300,.8f));live.Suspend();CHECK(!live.Recording()&&!live.Gesture().Armed());
    live.Deactivate();CHECK(!live.Gesture().Config().calibrated);
    icCompanionSession bounded;bounded.StartRecording();for(int i=0;i<18001;++i)bounded.OnInput(accel(400+i*.01));CHECK(!bounded.Recording()&&bounded.Take().events.size()==18000);
    CHECK(bounded.Take().Save(argv[1]));icCompanionTake invalid=bounded.Take();invalid.events[0].seconds=-1;CHECK(!invalid.Save(argv[1]));
    CHECK(replay.Take().Load(argv[1])&&replay.Take().events.size()==18000);
    std::remove(argv[1]);std::puts("Companion checks passed: calibration, gestures, cancellation, bounded storage and deterministic touch/motion replay");
}
