#import "Bridge.h"
#import <CoreMotion/CoreMotion.h>
#include "Input/icCompanion.h"
#include "../Common/Demo.h"
#include <chrono>
static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
struct WatchClient : icHostClient {
    icCompanionSession session;
    void OnInput(const icDeviceInput& e) override { session.OnInput(e); }
    void OnUpdate(double dt) override { session.AdvanceReplay(dt); }
};
@implementation ICEWatchRuntime {
    WatchClient _client;
    icHost* _host;
    icDeviceID _touch, _sensor;
    CMMotionManager* _motion;
    NSTimer* _timer;
    NSString* _path;
    BOOL _active;
}
- (instancetype)init {
    if ((self=[super init])) {
        _host=new icHost(_client);
        _touch=_host->AddDevice(icInputCapability(icInputKind::Pointer));
        _sensor=_host->AddDevice(icInputCapability(icInputKind::Acceleration)|icInputCapability(icInputKind::AngularVelocity));
        _motion=[CMMotionManager new];
        _motion.accelerometerUpdateInterval=1.0/50;
        _motion.gyroUpdateInterval=1.0/50;
        _path=[NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask,YES).firstObject stringByAppendingPathComponent:@"last-take.iceinput"];
        _client.session.Take().Load(_path.UTF8String);
    }
    return self;
}
- (void)dealloc { [self setActive:NO]; _host->Stop(); delete _host; }
- (void)setActive:(BOOL)active {
    if (_active==active) return;
    _active=active;
    [_timer invalidate];_timer=nil;
    [_motion stopAccelerometerUpdates];[_motion stopGyroUpdates];
    if (!active) {
        if (_client.session.Recording()) { _client.session.StopRecording();_client.session.Take().Save(_path.UTF8String); }
        _client.session.Deactivate();_host->SetState(icAppState::Background);return;
    }
    _host->SetState(icAppState::Active);
    __weak ICEWatchRuntime* weakSelf=self;
    _timer=[NSTimer scheduledTimerWithTimeInterval:1.0/30 repeats:YES block:^(NSTimer*) {
        ICEWatchRuntime* s=weakSelf;if(s) s->_host->Frame(now());
    }];
    [_motion startAccelerometerUpdatesToQueue:NSOperationQueue.mainQueue withHandler:^(CMAccelerometerData* data,NSError* error) {
        ICEWatchRuntime* s=weakSelf;if(!s || !data || !s->_active)return;
        icDeviceInput e;e.device=s->_sensor;e.seconds=data.timestamp;e.kind=icInputKind::Acceleration;
        e.x=-data.acceleration.x*9.80665;e.y=-data.acceleration.y*9.80665;e.z=-data.acceleration.z*9.80665;s->_host->PostInput(e);
    }];
    if (_motion.gyroAvailable) [_motion startGyroUpdatesToQueue:NSOperationQueue.mainQueue withHandler:^(CMGyroData* data,NSError* error) {
        ICEWatchRuntime* s=weakSelf;if(!s || !data || !s->_active)return;
        icDeviceInput e;e.device=s->_sensor;e.seconds=data.timestamp;e.kind=icInputKind::AngularVelocity;
        e.x=data.rotationRate.x;e.y=data.rotationRate.y;e.z=data.rotationRate.z;s->_host->PostInput(e);
    }];
}
- (void)pointer:(NSInteger)phase x:(float)x y:(float)y {
    icDeviceInput e;e.device=_touch;e.contact=1;e.seconds=now();e.x=x;e.y=y;
    e.phase=phase==0?icInputPhase::Begin:phase==1?icInputPhase::Change:phase==2?icInputPhase::End:icInputPhase::Cancel;
    _host->PostInput(e);_host->Frame(now());
}
- (void)setMotion:(BOOL)motion { _client.session.SetMode(motion?icShotMode::Motion:icShotMode::Touch); }
- (NSString*)calibrate {
    if (!_motion.accelerometerAvailable) return @"No accelerometer";
    switch (_client.session.Calibrate()) {
        case icCalibrationResult::Ready:return @"Calibrated";
        case icCalibrationResult::Moving:return @"Keep wrist still";
        default:return @"Collecting samples";
    }
}
- (void)record {
    if (_client.session.Recording()) { _client.session.StopRecording();_client.session.Take().Save(_path.UTF8String); }
    else _client.session.StartRecording();
}
- (void)replay {
    if (_client.session.Take().events.empty()) _client.session.Take()=icCompanionDemo();
    _client.session.StartReplay();
}
- (NSString*)status {
    auto& s=_client.session;
    if (s.Recording()) return @"Recording";
    if (s.Replaying()) return @"Replaying";
    if (s.Shots().empty()) return @"Drag up to launch";
    const auto& shot=s.Shots().back();
    return [NSString stringWithFormat:@"%lu shots · %.0f%%%@",(unsigned long)s.Shots().size(),shot.power*100,s.ReplayMatched()?@" · matched":@""];
}
@end
