#include "ice/input/companion.h"
#include "ice/net/companion_link.h"
#include "ice/platform/host.h"
#import <CoreMotion/CoreMotion.h>
#import <QuartzCore/CAMetalLayer.h>
#import <UIKit/UIKit.h>
#include <chrono>
#import <metal/metal.h>
static double networkNow() {
	return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
	    .count();
}
#include "../Common/Demo.h"
#include <cmath>
#include <map>

static id<MTLDevice>                             device;
static id<MTLCommandQueue>                       commands;
static CMMotionManager                          *motion;
static ice::platform::device_id                  touchDevice = 0, accelDevice = 0, gyroDevice = 0;
static void                                      updateSensors(bool active);
static NSMapTable<NSNumber *, UIView *>         *companionViews;
@interface                                       ICEView : UIView
@property(nonatomic) ice::platform::surface_id   surface;
@property(nonatomic, strong) UILabel            *heading;
@property(nonatomic, strong) UILabel            *status;
@property(nonatomic, strong) UILabel            *shot;
@property(nonatomic, strong) UILabel            *hint;
@property(nonatomic, strong) UISegmentedControl *mode;
@property(nonatomic, strong) UIButton           *record;
@property(nonatomic, strong) UIButton           *replay;
@property(nonatomic, strong) UIButton           *calibrate;
@property(nonatomic, strong) UIButton           *share;
@property(nonatomic, strong) UIButton           *connect;
@property(nonatomic, strong) UIButton           *tools;
@property(nonatomic) BOOL                        toolsVisible;
@property(nonatomic, strong) UIProgressView     *power;
@property(nonatomic, strong) UIView             *pad;
@property(nonatomic, strong) CAShapeLayer       *trajectory;
@property(nonatomic, strong) CAShapeLayer       *marker;
- (void)refreshCompanion;
@end
struct ProbeClient : ice::platform::host_client {
	unsigned                      frames = 0, inputs = 0, surfaces = 0;
	double                        elapsed = 0;
	ice::input::companion_session companion;
	ice::net::companion_link      link;
	std::string                   takePath, message = "Drag upward to launch a local test shot.";
	void                          SaveTake() {
        if (companion.take().events.empty())
            return;
        message = companion.take().save(takePath) ? "Take saved. Replay verifies its shots."
		                                                                   : "Could not save this take.";
	}
	void FinishRecording() {
		if (companion.recording()) {
			companion.stop_recording();
			SaveTake();
		}
	}
	void on_app_state(ice::platform::app_state state) override {
		NSLog(@"ICE state=%d", (int)state);
		if (state != ice::platform::app_state::active) {
			FinishRecording();
			companion.deactivate();
			link.close();
		}
		updateSensors(state == ice::platform::app_state::active);
	}
	void on_surface_added(const ice::platform::surface &s) override {
		++surfaces;
		NSLog(@"ICE surface added=%llu %dx%d", s.id, s.width, s.height);
	}
	void on_surface_changed(const ice::platform::surface &s) override {
		NSLog(@"ICE surface resized=%llu %dx%d", s.id, s.width, s.height);
	}
	void on_surface_removed(const ice::platform::surface &s) override {
		--surfaces;
		NSLog(@"ICE surface removed=%llu", s.id);
	}
	void on_input(const ice::platform::device_input &e) override {
		++inputs;
		bool recording = companion.recording();
		auto shots     = companion.shots().size();
		companion.on_input(e);
		if (recording && !companion.recording())
			SaveTake();
		if (companion.shots().size() > shots) {
			const auto &shot = companion.shots().back();
			link.send_shot(shot, networkNow());
			NSLog(@"ICE companion shot aim=%.3f power=%.3f source=%d", shot.aim, shot.power,
			      (int)shot.source);
		}
	}
	void on_update(double dt) override {
		link.poll(networkNow());
		elapsed += dt;
		bool playing = companion.replaying();
		companion.advance_replay(dt);
		if (playing && !companion.replaying())
			NSLog(@"ICE companion replay matched=%d shots=%lu", companion.replay_matched(),
			      (unsigned long)companion.shots().size());
		if (!accelDevice && !companion.replaying() &&
		    companion.gesture().config().mode == ice::input::shot_mode::motion)
			companion.set_mode(ice::input::shot_mode::touch);
	}
	void on_render(const ice::platform::surface &s) override {
		CAMetalLayer       *layer    = (__bridge CAMetalLayer *)s.native_handle;
		id<CAMetalDrawable> drawable = [layer nextDrawable];
		if (!drawable)
			return;
		MTLRenderPassDescriptor *pass        = [MTLRenderPassDescriptor renderPassDescriptor];
		pass.colorAttachments[0].texture     = drawable.texture;
		pass.colorAttachments[0].loadAction  = MTLLoadActionClear;
		pass.colorAttachments[0].storeAction = MTLStoreActionStore;
		float strength                       = companion.gesture().power();
		pass.colorAttachments[0].clearColor =
		    MTLClearColorMake(.035 + .08 * strength, .07 + .2 * strength, .12 + .25 * strength, 1);
		[(ICEView *)[companionViews objectForKey:@(s.id)] refreshCompanion];
		id<MTLCommandBuffer>        buffer  = [commands commandBuffer];
		id<MTLRenderCommandEncoder> encoder = [buffer renderCommandEncoderWithDescriptor:pass];
		[encoder endEncoding];
		[buffer presentDrawable:drawable];
		[buffer commit];
		++frames;
		if (frames == 1 || frames % 300 == 0)
			NSLog(@"ICE Metal frame=%u input=%u surfaces=%u", frames, inputs, surfaces);
	}
};
static ProbeClient                                client;
static ice::platform::host                        host(client);
static std::map<void *, ice::platform::app_state> scenes;
static void                                       aggregateState() {
    ice::platform::app_state state = ice::platform::app_state::background;
    for (auto entry : scenes) {
        if (entry.second == ice::platform::app_state::active) {
            state = ice::platform::app_state::active;
            break;
        }
        if (entry.second == ice::platform::app_state::inactive)
            state = ice::platform::app_state::inactive;
    }
    host.set_state(state);
}
static void updateSensors(bool active) {
	if (!active) {
		[motion stopAccelerometerUpdates];
		[motion stopGyroUpdates];
		return;
	}
	if (accelDevice) {
		motion.accelerometerUpdateInterval = 1.0 / 60;
		[motion startAccelerometerUpdatesToQueue:NSOperationQueue.mainQueue
		                             withHandler:^(CMAccelerometerData *data, NSError *error) {
			                           if (!data || error)
				                           return;
			                           ice::platform::device_input event;
			                           event.device  = accelDevice;
			                           event.kind    = ice::platform::input_kind::acceleration;
			                           event.seconds = data.timestamp;
			                           // Match Android specific-force convention: stationary
			                           // face-up is +Z gravity.
			                           event.x = -data.acceleration.x * 9.80665f;
			                           event.y = -data.acceleration.y * 9.80665f;
			                           event.z = -data.acceleration.z * 9.80665f;
			                           host.post_input(event);
		                             }];
	}
	if (gyroDevice) {
		motion.gyroUpdateInterval = 1.0 / 60;
		[motion startGyroUpdatesToQueue:NSOperationQueue.mainQueue
		                    withHandler:^(CMGyroData *data, NSError *error) {
			                  if (!data || error)
				                  return;
			                  ice::platform::device_input event;
			                  event.device  = gyroDevice;
			                  event.kind    = ice::platform::input_kind::angular_velocity;
			                  event.seconds = data.timestamp;
			                  event.x       = data.rotationRate.x;
			                  event.y       = data.rotationRate.y;
			                  event.z       = data.rotationRate.z;
			                  host.post_input(event);
		                    }];
	}
}
@implementation ICEView
+ (Class)layerClass {
	return CAMetalLayer.class;
}
- (instancetype)initWithFrame:(CGRect)frame {
	if (!(self = [super initWithFrame:frame]))
		return nil;
	self.multipleTouchEnabled = YES;
	self.heading              = [UILabel new];
	self.heading.text         = @"SkeeBall Controller";
	self.heading.font         = [UIFont systemFontOfSize:26 weight:UIFontWeightBold];
	self.status               = [UILabel new];
	self.status.numberOfLines = 2;
	self.status.font          = [UIFont systemFontOfSize:14];
	self.shot                 = [UILabel new];
	self.shot.numberOfLines   = 2;
	self.shot.font            = [UIFont monospacedSystemFontOfSize:16 weight:UIFontWeightMedium];
	self.hint                 = [UILabel new];
	self.hint.numberOfLines   = 0;
	self.hint.textAlignment   = NSTextAlignmentCenter;
	self.hint.font            = [UIFont systemFontOfSize:20 weight:UIFontWeightMedium];
	for (UILabel *label in @[ self.heading, self.status, self.shot, self.hint ]) {
		label.textColor = UIColor.whiteColor;
		[self addSubview:label];
	}
	self.mode = [[UISegmentedControl alloc] initWithItems:@[ @"Touch drag", @"Motion swing" ]];
	self.mode.selectedSegmentIndex     = 0;
	self.mode.selectedSegmentTintColor = [UIColor colorWithRed:.2 green:.65 blue:.8 alpha:1];
	[self.mode addTarget:self
	              action:@selector(changeMode)
	    forControlEvents:UIControlEventValueChanged];
	[self addSubview:self.mode];
	NSArray<NSString *> *titles =
	    @[ @"Record", @"Replay", @"Calibrate", @"Share take", @"Connect" ];
	NSArray<NSString *> *actions =
	    @[ @"toggleRecording", @"replayTake", @"calibrateMotion", @"shareTake", @"connectDesktop" ];
	NSMutableArray<UIButton *> *buttons = [NSMutableArray array];
	for (NSUInteger i = 0; i < titles.count; ++i) {
		UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
		[button setTitle:titles[i] forState:UIControlStateNormal];
		[button setTitleColor:UIColor.whiteColor forState:UIControlStateNormal];
		button.backgroundColor                      = [UIColor colorWithWhite:1 alpha:.12];
		button.layer.cornerRadius                   = 9;
		button.titleLabel.font                      = [UIFont systemFontOfSize:14];
		button.titleLabel.adjustsFontSizeToFitWidth = YES;
		[button addTarget:self
		              action:NSSelectorFromString(actions[i])
		    forControlEvents:UIControlEventTouchUpInside];
		[self addSubview:button];
		[buttons addObject:button];
	}
	self.record    = buttons[0];
	self.replay    = buttons[1];
	self.calibrate = buttons[2];
	self.share     = buttons[3];
	self.connect   = buttons[4];
	self.tools     = [UIButton buttonWithType:UIButtonTypeSystem];
	[self.tools setTitle:@"Tools" forState:UIControlStateNormal];
	[self.tools addTarget:self
	               action:@selector(toggleTools)
	     forControlEvents:UIControlEventTouchUpInside];
	[self addSubview:self.tools];
	for (UIView *view in @[ self.mode, self.record, self.replay, self.calibrate, self.share ])
		view.hidden = YES;
	self.power                   = [UIProgressView new];
	self.power.progressTintColor = [UIColor colorWithRed:.25 green:.9 blue:.7 alpha:1];
	[self addSubview:self.power];
	self.pad                        = [UIView new];
	self.pad.userInteractionEnabled = NO;
	self.pad.backgroundColor        = [UIColor colorWithWhite:1 alpha:.045];
	self.pad.layer.cornerRadius     = 24;
	self.trajectory                 = [CAShapeLayer layer];
	self.trajectory.strokeColor     = [UIColor colorWithRed:.25 green:.9 blue:.7 alpha:.45].CGColor;
	self.trajectory.fillColor       = UIColor.clearColor.CGColor;
	self.trajectory.lineWidth       = 2;
	self.trajectory.lineDashPattern = @[ @5, @7 ];
	[self.pad.layer addSublayer:self.trajectory];
	self.marker           = [CAShapeLayer layer];
	self.marker.fillColor = UIColor.whiteColor.CGColor;
	[self.pad.layer addSublayer:self.marker];
	self.pad.layer.borderWidth = 1;
	self.pad.layer.borderColor = [UIColor colorWithWhite:1 alpha:.2].CGColor;
	[self insertSubview:self.pad belowSubview:self.hint];
	return self;
}
- (void)layoutSubviews {
	[super layoutSubviews];
	CAMetalLayer *layer = (CAMetalLayer *)self.layer;
	CGFloat       scale = self.window.screen.scale ?: UIScreen.mainScreen.scale;
	layer.contentsScale = scale;
	layer.drawableSize =
	    CGSizeMake(self.bounds.size.width * scale, self.bounds.size.height * scale);
	if (self.surface)
		host.resize_surface(self.surface, (int)layer.drawableSize.width,
		                    (int)layer.drawableSize.height, scale);
	CGFloat width_ = MIN(720, self.bounds.size.width - 40),
	        left = (self.bounds.size.width - width_) / 2, top = self.safeAreaInsets.top + 12;
	BOOL    compact  = self.bounds.size.height < 550;
	CGFloat controls = compact ? 76 : 90, extra = self.toolsVisible ? 92 : 0,
	        shot = controls + 58 + extra, power = shot + 50, pad = power + 26;
	self.heading.font  = [UIFont systemFontOfSize:MIN(26, width_ / 15) weight:UIFontWeightBold];
	self.heading.frame = CGRectMake(left, top, width_, 34);
	self.status.frame  = CGRectMake(left, top + 38, width_, 30);
	self.connect.frame = CGRectMake(left, top + controls, width_ * .72, 44);
	self.connect.backgroundColor = [UIColor colorWithRed:.08 green:.4 blue:.55 alpha:1];
	self.tools.frame           = CGRectMake(left + width_ * .76, top + controls, width_ * .24, 44);
	self.mode.frame            = CGRectMake(left, top + controls + 54, width_, 32);
	NSArray<UIButton *> *items = @[ self.record, self.replay, self.calibrate, self.share ];
	for (NSUInteger i = 0; i < items.count; ++i)
		items[i].frame =
		    CGRectMake(left + i * (width_ + 8) / 4, top + controls + 94, (width_ - 24) / 4, 36);
	self.shot.frame  = CGRectMake(left, top + shot, width_, 44);
	self.power.frame = CGRectMake(left, top + power, width_, 8);
	self.pad.frame =
	    CGRectMake(left, top + pad, width_,
	               MAX(60, self.bounds.size.height - self.safeAreaInsets.bottom - 24 - top - pad));
	self.hint.frame = CGRectInset(self.pad.frame, 20, 12);
}
- (void)refreshCompanion {
	auto &c         = client.companion;
	auto &gesture   = c.gesture();
	BOOL  replaying = c.replaying();
	[self.mode setEnabled:accelDevice != 0 forSegmentAtIndex:1];
	self.mode.enabled              = !replaying;
	self.mode.selectedSegmentIndex = (NSInteger)gesture.config().mode;
	self.calibrate.enabled         = accelDevice && !replaying;
	self.replay.enabled            = !c.take().events.empty() && !replaying;
	self.share.enabled             = !c.take().events.empty() && !c.recording();
	[self.record setTitle:c.recording() ? @"Stop & save" : @"Record" forState:UIControlStateNormal];
	self.record.enabled = !replaying;
	self.power.progress = gesture.power();
	[self.connect
	    setTitle:client.link.status() == ice::net::companion_link::state::connected ? @"Disconnect"
	                                                                                : @"Connect"
	    forState:UIControlStateNormal];
	self.status.text = [NSString stringWithUTF8String:client.link.message().c_str()];
	if (c.recording())
		self.status.text = [@"Recording · " stringByAppendingString:self.status.text];
	if (c.shots().empty())
		self.shot.text = @"Aim left or right. Drag up and release.";
	else {
		const auto &s = c.shots().back();
		self.shot.text =
		    [NSString stringWithFormat:@"Power %.0f%%   Aim %+.0f%%", s.power * 100, s.aim * 100];
	}
	self.hint.text =
	    gesture.config().mode == ice::input::shot_mode::touch ? @"DRAG UP\n\nRelease to roll"
	    : gesture.config().calibrated ? @"HOLD · SWING · RELEASE\n\nHold a finger here, swing "
	                                    @"forward,\nthen release to launch"
	                                  : @"CALIBRATE FIRST\n\nHold the device still, then tap "
	                                    @"Calibrate.\nMotion needs real device sensors.";
	float aim = gesture.aim(), strength = gesture.power();
	if (!gesture.armed() && !c.shots().empty()) {
		aim      = c.shots().back().aim;
		strength = c.shots().back().power;
	}
	CGFloat w = self.pad.bounds.size.width, h = self.pad.bounds.size.height;
	CGPoint start      = CGPointMake(w * .5, h * .9),
	        end        = CGPointMake(w * (.5 + aim * .4), h * (.9 - strength * .75));
	UIBezierPath *path = [UIBezierPath bezierPath];
	[path moveToPoint:start];
	[path addQuadCurveToPoint:end controlPoint:CGPointMake(start.x, (start.y + end.y) * .5)];
	[CATransaction begin];
	[CATransaction setDisableActions:YES];
	self.trajectory.path = path.CGPath;
	self.marker.path =
	    [UIBezierPath bezierPathWithOvalInRect:CGRectMake(end.x - 7, end.y - 7, 14, 14)].CGPath;
	[CATransaction commit];
	if (gesture.armed())
		self.hint.text =
		    [NSString stringWithFormat:@"POWER %.0f%%\nAIM %+.0f%%\n\nRelease to launch",
		                               gesture.power() * 100, gesture.aim() * 100];
}
- (void)toggleTools {
	self.toolsVisible = !self.toolsVisible;
	for (UIView *view in @[ self.mode, self.record, self.replay, self.calibrate, self.share ])
		view.hidden = !self.toolsVisible;
	[self.tools setTitle:self.toolsVisible ? @"Done" : @"Tools" forState:UIControlStateNormal];
	[self setNeedsLayout];
}
- (void)connectDesktop {
	if (client.link.status() == ice::net::companion_link::state::connected ||
	    client.link.status() == ice::net::companion_link::state::pairing) {
		client.link.close();
		return;
	}
	client.companion.suspend();
	UIAlertController *alert = [UIAlertController
	    alertControllerWithTitle:@"Connect to SkeeBall"
	                     message:@"Press F2 in the desktop game. Enter one displayed IPv4 address, "
	                             @"its port, and the six-digit pairing code."
	              preferredStyle:UIAlertControllerStyleAlert];
	[alert addTextFieldWithConfigurationHandler:^(UITextField *field) {
	  field.placeholder  = @"Desktop IPv4 address";
	  field.keyboardType = UIKeyboardTypeDecimalPad;
	  field.text = [NSUserDefaults.standardUserDefaults stringForKey:@"ICECompanionAddress"];
	  field.accessibilityIdentifier = @"desktop-address";
	}];
	[alert addTextFieldWithConfigurationHandler:^(UITextField *field) {
	  field.placeholder  = @"Port";
	  field.keyboardType = UIKeyboardTypeNumberPad;
	  field.text         = [NSUserDefaults.standardUserDefaults stringForKey:@"ICECompanionPort"];
	  field.accessibilityIdentifier = @"desktop-port";
	}];
	[alert addTextFieldWithConfigurationHandler:^(UITextField *field) {
	  field.placeholder             = @"Six-digit pairing code";
	  field.keyboardType            = UIKeyboardTypeNumberPad;
	  field.accessibilityIdentifier = @"pairing-code";
	}];
	[alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
	                                          style:UIAlertActionStyleCancel
	                                        handler:nil]];
	[alert addAction:[UIAlertAction
	                     actionWithTitle:@"Connect"
	                               style:UIAlertActionStyleDefault
	                             handler:^(UIAlertAction *) {
		                           NSString *address = alert.textFields[0].text;
		                           NSInteger port    = alert.textFields[1].text.integerValue;
		                           NSString *code    = alert.textFields[2].text;
		                           if (port < 1 || port > 65535 || code.length != 6) {
			                           client.message =
			                               "Enter a port from 1 to 65535 and a six-digit code.";
			                           return;
		                           }
		                           [NSUserDefaults.standardUserDefaults
		                               setObject:address
		                                  forKey:@"ICECompanionAddress"];
		                           [NSUserDefaults.standardUserDefaults
		                               setObject:alert.textFields[1].text
		                                  forKey:@"ICECompanionPort"];
		                           client.link.connect(address.UTF8String, (unsigned short)port,
		                                               (unsigned)code.integerValue, networkNow());
		                           client.message =
		                               "Allow Local Network access if asked. Replay stays local.";
	                             }]];
	[self.window.rootViewController presentViewController:alert animated:YES completion:nil];
}
- (void)changeMode {
	client.FinishRecording();
	client.companion.set_mode(self.mode.selectedSegmentIndex ? ice::input::shot_mode::motion
	                                                         : ice::input::shot_mode::touch);
}
- (void)toggleRecording {
	if (client.companion.recording())
		client.FinishRecording();
	else {
		client.companion.start_recording();
		client.message = "Recording raw input. Live shots go to a connected desktop.";
	}
}
- (void)replayTake {
	client.FinishRecording();
	if (client.companion.start_replay())
		client.message = "Live input paused during replay.";
}
- (void)calibrateMotion {
	client.FinishRecording();
	auto result = client.companion.calibrate();
	NSLog(@"ICE companion calibration=%d", (int)result);
	client.message = result == ice::input::calibration_result::ready
	                     ? "Calibrated. Hold, swing, release."
	                 : result == ice::input::calibration_result::need_samples
	                     ? "Wait for sensor samples, then try again."
	                     : "Keep the device still and try calibration again.";
}
- (void)shareTake {
	client.SaveTake();
	NSURL *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:client.takePath.c_str()]];
	if (![NSFileManager.defaultManager fileExistsAtPath:url.path])
		return;
	UIActivityViewController *sheet =
	    [[UIActivityViewController alloc] initWithActivityItems:@[ url ] applicationActivities:nil];
	sheet.popoverPresentationController.sourceView = self.share;
	sheet.popoverPresentationController.sourceRect = self.share.bounds;
	[self.window.rootViewController presentViewController:sheet animated:YES completion:nil];
}
- (void)addScene {
	[UIApplication.sharedApplication requestSceneSessionActivation:nil
	                                                  userActivity:nil
	                                                       options:nil
	                                                  errorHandler:^(NSError *error) {
		                                                NSLog(@"ICE scene request: %@", error);
	                                                  }];
}
- (void)sendTouches:(NSSet<UITouch *> *)touches phase:(ice::platform::input_phase)phase {
	for (UITouch *touch in touches) {
		CGPoint                     p = [touch locationInView:self];
		ice::platform::device_input event;
		event.device  = touchDevice;
		event.surface = self.surface;
		event.contact = (uintptr_t)(__bridge void *)touch;
		event.seconds = touch.timestamp;
		event.phase   = phase;
		event.x       = p.x / MAX(1, self.bounds.size.width);
		event.y       = p.y / MAX(1, self.bounds.size.height);
		host.post_input(event);
	}
}
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
	[self sendTouches:touches phase:ice::platform::input_phase::begin];
}
- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
	[self sendTouches:touches phase:ice::platform::input_phase::change];
}
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
	[self sendTouches:touches phase:ice::platform::input_phase::end];
}
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
	[self sendTouches:touches phase:ice::platform::input_phase::cancel];
}
@end
@interface                             ICESceneDelegate : UIResponder <UIWindowSceneDelegate>
@property(nonatomic, strong) UIWindow *window;
@end
@implementation ICESceneDelegate
- (void)scene:(UIScene *)scene
    willConnectToSession:(UISceneSession *)session
                 options:(UISceneConnectionOptions *)options {
	if (![scene isKindOfClass:UIWindowScene.class])
		return;
	self.window                  = [[UIWindow alloc] initWithWindowScene:(UIWindowScene *)scene];
	UIViewController *controller = [UIViewController new];
	ICEView          *view       = [[ICEView alloc] initWithFrame:self.window.bounds];
	view.multipleTouchEnabled    = YES;
	CAMetalLayer *layer          = (CAMetalLayer *)view.layer;
	layer.device                 = device;
	layer.pixelFormat            = MTLPixelFormatBGRA8Unorm;
	ice::platform::surface surface;
	surface.api           = ice::platform::surface_api::metal;
	surface.native_handle = (__bridge void *)layer;
	view.surface          = host.add_surface(surface);
	[companionViews setObject:view forKey:@(view.surface)];
	controller.view                = view;
	self.window.rootViewController = controller;
	scenes[(__bridge void *)scene] = ice::platform::app_state::inactive;
	[self.window makeKeyAndVisible];
}
- (void)sceneDidBecomeActive:(UIScene *)scene {
	scenes[(__bridge void *)scene] = ice::platform::app_state::active;
	aggregateState();
}
- (void)sceneWillResignActive:(UIScene *)scene {
	scenes[(__bridge void *)scene] = ice::platform::app_state::inactive;
	aggregateState();
}
- (void)sceneDidEnterBackground:(UIScene *)scene {
	ICEView *view = (ICEView *)self.window.rootViewController.view;
	host.set_surface_visible(view.surface, false);
	scenes[(__bridge void *)scene] = ice::platform::app_state::background;
	aggregateState();
}
- (void)sceneWillEnterForeground:(UIScene *)scene {
	ICEView *view = (ICEView *)self.window.rootViewController.view;
	host.set_surface_visible(view.surface, true);
	scenes[(__bridge void *)scene] = ice::platform::app_state::inactive;
	aggregateState();
}
- (void)sceneDidDisconnect:(UIScene *)scene {
	ICEView *view = (ICEView *)self.window.rootViewController.view;
	host.remove_surface(view.surface);
	[companionViews removeObjectForKey:@(view.surface)];
	view.surface = 0;
	scenes.erase((__bridge void *)scene);
	aggregateState();
	self.window = nil;
}
@end
@interface                                  ICEAppDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic, strong) CADisplayLink *displayLink;
@end
@implementation ICEAppDelegate
- (BOOL)application:(UIApplication *)app didFinishLaunchingWithOptions:(NSDictionary *)options {
	device   = MTLCreateSystemDefaultDevice();
	commands = [device newCommandQueue];
	if (!device || !commands)
		return NO;
	companionViews = [NSMapTable strongToWeakObjectsMapTable];
	NSString *documents =
	    NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
	client.takePath = [[documents stringByAppendingPathComponent:@"last-take.iceinput"] UTF8String];
	if (client.companion.take().load(client.takePath))
		client.message = "Previous take loaded. Tap Replay to inspect it.";
	else {
		client.companion.take() = icCompanionDemo();
		client.message          = "Demo take loaded. Replay or record.";
	}
	if ([NSProcessInfo.processInfo.arguments containsObject:@"--demo-replay"])
		client.companion.start_replay();
	touchDevice =
	    host.add_device(ice::platform::input_capability(ice::platform::input_kind::pointer));
	motion = [CMMotionManager new];
	if (motion.accelerometerAvailable)
		accelDevice = host.add_device(
		    ice::platform::input_capability(ice::platform::input_kind::acceleration));
	if (motion.gyroAvailable)
		gyroDevice = host.add_device(
		    ice::platform::input_capability(ice::platform::input_kind::angular_velocity));
	self.displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(tick:)];
	[self.displayLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
	return YES;
}
- (void)tick:(CADisplayLink *)link {
	host.frame(link.timestamp);
}
- (void)applicationWillTerminate:(UIApplication *)application {
	[self.displayLink invalidate];
	host.stop();
}
@end
int main(int argc, char **argv) {
	@autoreleasepool {
		return UIApplicationMain(argc, argv, nil, NSStringFromClass(ICEAppDelegate.class));
	}
}
