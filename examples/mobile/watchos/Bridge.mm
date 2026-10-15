#import "Bridge.h"
#include "../Common/Demo.h"
#include "ice/input/companion.h"
#import <CoreMotion/CoreMotion.h>
#include <chrono>
static double now() {
	return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
	    .count();
}
struct WatchClient : ice::platform::host_client {
	ice::input::companion_session session;
	void on_input(const ice::platform::device_input &e) override { session.on_input(e); }
	void on_update(double dt) override { session.advance_replay(dt); }
};
@implementation ICEWatchRuntime {
	WatchClient              _client;
	ice::platform::host     *_host;
	ice::platform::device_id _touch, _sensor;
	CMMotionManager         *_motion;
	NSTimer                 *_timer;
	NSString                *_path;
	BOOL                     _active;
}
- (instancetype)init {
	if ((self = [super init])) {
		_host = new ice::platform::host(_client);
		_touch =
		    _host->add_device(ice::platform::input_capability(ice::platform::input_kind::pointer));
		_sensor = _host->add_device(
		    ice::platform::input_capability(ice::platform::input_kind::acceleration) |
		    ice::platform::input_capability(ice::platform::input_kind::angular_velocity));
		_motion                             = [CMMotionManager new];
		_motion.accelerometerUpdateInterval = 1.0 / 50;
		_motion.gyroUpdateInterval          = 1.0 / 50;
		_path = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES)
		             .firstObject stringByAppendingPathComponent:@"last-take.iceinput"];
		_client.session.take().load(_path.UTF8String);
	}
	return self;
}
- (void)dealloc {
	[self setActive:NO];
	_host->stop();
	delete _host;
}
- (void)setActive:(BOOL)active {
	if (_active == active)
		return;
	_active = active;
	[_timer invalidate];
	_timer = nil;
	[_motion stopAccelerometerUpdates];
	[_motion stopGyroUpdates];
	if (!active) {
		if (_client.session.recording()) {
			_client.session.stop_recording();
			_client.session.take().save(_path.UTF8String);
		}
		_client.session.deactivate();
		_host->set_state(ice::platform::app_state::background);
		return;
	}
	_host->set_state(ice::platform::app_state::active);
	__weak ICEWatchRuntime *weakSelf = self;
	_timer                           = [NSTimer scheduledTimerWithTimeInterval:1.0 / 30
                                             repeats:YES
                                               block:^(NSTimer *) {
                                                 ICEWatchRuntime *s = weakSelf;
                                                 if (s)
                                                     s->_host->frame(now());
                                               }];
	[_motion startAccelerometerUpdatesToQueue:NSOperationQueue.mainQueue
	                              withHandler:^(CMAccelerometerData *data, NSError *error) {
		                            ICEWatchRuntime *s = weakSelf;
		                            if (!s || !data || !s->_active)
			                            return;
		                            ice::platform::device_input e;
		                            e.device  = s->_sensor;
		                            e.seconds = data.timestamp;
		                            e.kind    = ice::platform::input_kind::acceleration;
		                            e.x       = -data.acceleration.x * 9.80665;
		                            e.y       = -data.acceleration.y * 9.80665;
		                            e.z       = -data.acceleration.z * 9.80665;
		                            s->_host->post_input(e);
	                              }];
	if (_motion.gyroAvailable)
		[_motion startGyroUpdatesToQueue:NSOperationQueue.mainQueue
		                     withHandler:^(CMGyroData *data, NSError *error) {
			                   ICEWatchRuntime *s = weakSelf;
			                   if (!s || !data || !s->_active)
				                   return;
			                   ice::platform::device_input e;
			                   e.device  = s->_sensor;
			                   e.seconds = data.timestamp;
			                   e.kind    = ice::platform::input_kind::angular_velocity;
			                   e.x       = data.rotationRate.x;
			                   e.y       = data.rotationRate.y;
			                   e.z       = data.rotationRate.z;
			                   s->_host->post_input(e);
		                     }];
}
- (void)pointer:(NSInteger)phase x:(float)x y:(float)y {
	ice::platform::device_input e;
	e.device  = _touch;
	e.contact = 1;
	e.seconds = now();
	e.x       = x;
	e.y       = y;
	e.phase   = phase == 0   ? ice::platform::input_phase::begin
	            : phase == 1 ? ice::platform::input_phase::change
	            : phase == 2 ? ice::platform::input_phase::end
	                         : ice::platform::input_phase::cancel;
	_host->post_input(e);
	_host->frame(now());
}
- (void)setMotion:(BOOL)motion {
	_client.session.set_mode(motion ? ice::input::shot_mode::motion : ice::input::shot_mode::touch);
}
- (NSString *)calibrate {
	if (!_motion.accelerometerAvailable)
		return @"No accelerometer";
	switch (_client.session.calibrate()) {
	case ice::input::calibration_result::ready:
		return @"Calibrated";
	case ice::input::calibration_result::moving:
		return @"Keep wrist still";
	default:
		return @"Collecting samples";
	}
}
- (void)record {
	if (_client.session.recording()) {
		_client.session.stop_recording();
		_client.session.take().save(_path.UTF8String);
	} else
		_client.session.start_recording();
}
- (void)replay {
	if (_client.session.take().events.empty())
		_client.session.take() = icCompanionDemo();
	_client.session.start_replay();
}
- (NSString *)status {
	auto &s = _client.session;
	if (s.recording())
		return @"Recording";
	if (s.replaying())
		return @"Replaying";
	if (s.shots().empty())
		return @"Drag up to launch";
	const auto &shot = s.shots().back();
	return [NSString stringWithFormat:@"%lu shots · %.0f%%%@", (unsigned long)s.shots().size(),
	                                  shot.power * 100, s.replay_matched() ? @" · matched" : @""];
}
@end
