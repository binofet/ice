#import <UIKit/UIKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <CoreMotion/CoreMotion.h>
#include "Platform/icHost.h"
#include "Input/icCompanion.h"
#include "../Common/Demo.h"
#include <map>
#include <cmath>

static id<MTLDevice> device;
static id<MTLCommandQueue> commands;
static CMMotionManager* motion;
static icDeviceID touchDevice=0,accelDevice=0,gyroDevice=0;
static void updateSensors(bool active);
static NSMapTable<NSNumber*,UIView*>* companionViews;
@interface ICEView : UIView
@property(nonatomic) icSurfaceID surface;
@property(nonatomic,strong) UILabel* heading;
@property(nonatomic,strong) UILabel* status;
@property(nonatomic,strong) UILabel* shot;
@property(nonatomic,strong) UILabel* hint;
@property(nonatomic,strong) UISegmentedControl* mode;
@property(nonatomic,strong) UIButton* record;
@property(nonatomic,strong) UIButton* replay;
@property(nonatomic,strong) UIButton* calibrate;
@property(nonatomic,strong) UIButton* share;
@property(nonatomic,strong) UIProgressView* power;
@property(nonatomic,strong) UIView* pad;
@property(nonatomic,strong) CAShapeLayer* trajectory;
@property(nonatomic,strong) CAShapeLayer* marker;
- (void)refreshCompanion;
@end
struct ProbeClient : icHostClient {
    unsigned frames=0,inputs=0,surfaces=0;double elapsed=0;
    icCompanionSession companion;
    std::string takePath,message="Drag upward to launch a local test shot.";
    void SaveTake(){if(companion.Take().events.empty())return;message=companion.Take().Save(takePath)?"Take saved. Replay verifies its shots.":"Could not save this take.";}
    void FinishRecording(){if(companion.Recording()){companion.StopRecording();SaveTake();}}
    void OnAppState(icAppState state) override {
        NSLog(@"ICE state=%d",(int)state);if(state!=icAppState::Active){FinishRecording();companion.Deactivate();}updateSensors(state==icAppState::Active);
    }
    void OnSurfaceAdded(const icSurface& s) override {++surfaces;NSLog(@"ICE surface added=%llu %dx%d",s.id,s.width,s.height);}
    void OnSurfaceChanged(const icSurface& s) override {NSLog(@"ICE surface resized=%llu %dx%d",s.id,s.width,s.height);}
    void OnSurfaceRemoved(const icSurface& s) override {--surfaces;NSLog(@"ICE surface removed=%llu",s.id);}
    void OnInput(const icDeviceInput& e) override {
        ++inputs;bool recording=companion.Recording();auto shots=companion.Shots().size();companion.OnInput(e);
        if(recording&&!companion.Recording())SaveTake();
        if(companion.Shots().size()>shots){const auto& shot=companion.Shots().back();NSLog(@"ICE companion shot aim=%.3f power=%.3f source=%d",shot.aim,shot.power,(int)shot.source);}
    }
    void OnUpdate(double dt) override {
        elapsed+=dt;bool playing=companion.Replaying();companion.AdvanceReplay(dt);
        if(playing&&!companion.Replaying())NSLog(@"ICE companion replay matched=%d shots=%lu",companion.ReplayMatched(),(unsigned long)companion.Shots().size());
        if(!accelDevice&&!companion.Replaying()&&companion.Gesture().Config().mode==icShotMode::Motion)companion.SetMode(icShotMode::Touch);
    }
    void OnRender(const icSurface& s) override {
        CAMetalLayer* layer=(__bridge CAMetalLayer*)s.nativeHandle;
        id<CAMetalDrawable> drawable=[layer nextDrawable];if(!drawable)return;
        MTLRenderPassDescriptor* pass=[MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture=drawable.texture;
        pass.colorAttachments[0].loadAction=MTLLoadActionClear;
        pass.colorAttachments[0].storeAction=MTLStoreActionStore;
        float strength=companion.Gesture().Power();
        pass.colorAttachments[0].clearColor=MTLClearColorMake(.035+.08*strength,.07+.2*strength,.12+.25*strength,1);
        [(ICEView*)[companionViews objectForKey:@(s.id)] refreshCompanion];
        id<MTLCommandBuffer> buffer=[commands commandBuffer];
        id<MTLRenderCommandEncoder> encoder=[buffer renderCommandEncoderWithDescriptor:pass];
        [encoder endEncoding];[buffer presentDrawable:drawable];[buffer commit];++frames;
        if(frames==1||frames%300==0)NSLog(@"ICE Metal frame=%u input=%u surfaces=%u",frames,inputs,surfaces);
    }
};
static ProbeClient client;
static icHost host(client);
static std::map<void*,icAppState> scenes;
static void aggregateState(){
    icAppState state=icAppState::Background;
    for(auto entry:scenes){if(entry.second==icAppState::Active){state=icAppState::Active;break;}if(entry.second==icAppState::Inactive)state=icAppState::Inactive;}
    host.SetState(state);
}
static void updateSensors(bool active){
    if(!active){[motion stopAccelerometerUpdates];[motion stopGyroUpdates];return;}
    if(accelDevice){motion.accelerometerUpdateInterval=1.0/60;[motion startAccelerometerUpdatesToQueue:NSOperationQueue.mainQueue withHandler:^(CMAccelerometerData* data,NSError* error){
        if(!data||error)return;icDeviceInput event;event.device=accelDevice;event.kind=icInputKind::Acceleration;event.seconds=data.timestamp;
        // Match Android specific-force convention: stationary face-up is +Z gravity.
        event.x=-data.acceleration.x*9.80665f;event.y=-data.acceleration.y*9.80665f;event.z=-data.acceleration.z*9.80665f;host.PostInput(event);
    }];}
    if(gyroDevice){motion.gyroUpdateInterval=1.0/60;[motion startGyroUpdatesToQueue:NSOperationQueue.mainQueue withHandler:^(CMGyroData* data,NSError* error){
        if(!data||error)return;icDeviceInput event;event.device=gyroDevice;event.kind=icInputKind::AngularVelocity;event.seconds=data.timestamp;
        event.x=data.rotationRate.x;event.y=data.rotationRate.y;event.z=data.rotationRate.z;host.PostInput(event);
    }];}
}
@implementation ICEView
+ (Class)layerClass{return CAMetalLayer.class;}
- (instancetype)initWithFrame:(CGRect)frame {
    if(!(self=[super initWithFrame:frame]))return nil;
    self.multipleTouchEnabled=YES;
    self.heading=[UILabel new];self.heading.text=@"ICE / COMPANION LAB";self.heading.font=[UIFont systemFontOfSize:26 weight:UIFontWeightBold];
    self.status=[UILabel new];self.status.numberOfLines=2;self.status.font=[UIFont systemFontOfSize:14];
    self.shot=[UILabel new];self.shot.numberOfLines=2;self.shot.font=[UIFont monospacedSystemFontOfSize:16 weight:UIFontWeightMedium];
    self.hint=[UILabel new];self.hint.numberOfLines=0;self.hint.textAlignment=NSTextAlignmentCenter;self.hint.font=[UIFont systemFontOfSize:20 weight:UIFontWeightMedium];
    for(UILabel* label in @[self.heading,self.status,self.shot,self.hint]){label.textColor=UIColor.whiteColor;[self addSubview:label];}
    self.mode=[[UISegmentedControl alloc] initWithItems:@[@"Touch drag",@"Motion swing"]];self.mode.selectedSegmentIndex=0;
    self.mode.selectedSegmentTintColor=[UIColor colorWithRed:.2 green:.65 blue:.8 alpha:1];
    [self.mode addTarget:self action:@selector(changeMode) forControlEvents:UIControlEventValueChanged];[self addSubview:self.mode];
    NSArray<NSString*>* titles=@[@"Record",@"Replay",@"Calibrate",@"Share take"];
    NSArray<NSString*>* actions=@[@"toggleRecording",@"replayTake",@"calibrateMotion",@"shareTake"];
    NSMutableArray<UIButton*>* buttons=[NSMutableArray array];
    for(NSUInteger i=0;i<titles.count;++i){UIButton* button=[UIButton buttonWithType:UIButtonTypeSystem];
        [button setTitle:titles[i] forState:UIControlStateNormal];[button setTitleColor:UIColor.whiteColor forState:UIControlStateNormal];
        button.backgroundColor=[UIColor colorWithWhite:1 alpha:.12];button.layer.cornerRadius=9;button.titleLabel.font=[UIFont systemFontOfSize:14];
        button.titleLabel.adjustsFontSizeToFitWidth=YES;[button addTarget:self action:NSSelectorFromString(actions[i]) forControlEvents:UIControlEventTouchUpInside];[self addSubview:button];[buttons addObject:button];
    }
    self.record=buttons[0];self.replay=buttons[1];self.calibrate=buttons[2];self.share=buttons[3];
    self.power=[UIProgressView new];self.power.progressTintColor=[UIColor colorWithRed:.25 green:.9 blue:.7 alpha:1];[self addSubview:self.power];
    self.pad=[UIView new];self.pad.userInteractionEnabled=NO;self.pad.backgroundColor=[UIColor colorWithWhite:1 alpha:.045];self.pad.layer.cornerRadius=24;
    self.trajectory=[CAShapeLayer layer];self.trajectory.strokeColor=[UIColor colorWithRed:.25 green:.9 blue:.7 alpha:.45].CGColor;self.trajectory.fillColor=UIColor.clearColor.CGColor;self.trajectory.lineWidth=2;self.trajectory.lineDashPattern=@[@5,@7];[self.pad.layer addSublayer:self.trajectory];
    self.marker=[CAShapeLayer layer];self.marker.fillColor=UIColor.whiteColor.CGColor;[self.pad.layer addSublayer:self.marker];
    self.pad.layer.borderWidth=1;self.pad.layer.borderColor=[UIColor colorWithWhite:1 alpha:.2].CGColor;[self insertSubview:self.pad belowSubview:self.hint];return self;
}
- (void)layoutSubviews {
    [super layoutSubviews];CAMetalLayer* layer=(CAMetalLayer*)self.layer;
    CGFloat scale=self.window.screen.scale?:UIScreen.mainScreen.scale;
    layer.contentsScale=scale;layer.drawableSize=CGSizeMake(self.bounds.size.width*scale,self.bounds.size.height*scale);
    if(self.surface)host.ResizeSurface(self.surface,(int)layer.drawableSize.width,(int)layer.drawableSize.height,scale);
    CGFloat width=MIN(720,self.bounds.size.width-40),left=(self.bounds.size.width-width)/2,top=self.safeAreaInsets.top+12;
    BOOL compact=self.bounds.size.height<550;CGFloat modes=compact?60:100,buttons=compact?102:146,shot=compact?150:198,power=compact?198:260,pad=compact?220:288;
    self.heading.font=[UIFont systemFontOfSize:MIN(26,width/15) weight:UIFontWeightBold];self.heading.frame=CGRectMake(left,top,width,34);self.status.frame=CGRectMake(left,top+36,width,compact?20:52);
    self.mode.frame=CGRectMake(left,top+modes,width,32);
    NSArray<UIButton*>* items=@[self.record,self.replay,self.calibrate,self.share];for(NSUInteger i=0;i<items.count;++i)items[i].frame=CGRectMake(left+i*(width+8)/4,top+buttons,(width-24)/4,40);
    self.shot.frame=CGRectMake(left,top+shot,width,48);self.power.frame=CGRectMake(left,top+power,width,8);
    self.pad.frame=CGRectMake(left,top+pad,width,MAX(60,self.bounds.size.height-self.safeAreaInsets.bottom-24-top-pad));
    self.hint.frame=CGRectInset(self.pad.frame,20,12);
}
- (void)refreshCompanion {
    auto& c=client.companion;auto& gesture=c.Gesture();BOOL replaying=c.Replaying();
    [self.mode setEnabled:accelDevice!=0 forSegmentAtIndex:1];self.mode.enabled=!replaying;self.mode.selectedSegmentIndex=(NSInteger)gesture.Config().mode;
    self.calibrate.enabled=accelDevice&&!replaying;self.replay.enabled=!c.Take().events.empty()&&!replaying;self.share.enabled=!c.Take().events.empty()&&!c.Recording();
    [self.record setTitle:c.Recording()?@"Stop & save":@"Record" forState:UIControlStateNormal];self.record.enabled=!replaying;
    self.power.progress=gesture.Power();
    NSString* state=c.Recording()?@"RECORDING":replaying?@"REPLAYING":c.ReplayMatched()?@"REPLAY MATCHED":@"LOCAL TEST / NOT CONNECTED";
    self.status.text=[NSString stringWithFormat:@"%@%@\n%s",state,c.Recording()?[NSString stringWithFormat:@" · %lu samples",(unsigned long)c.Take().events.size()]:@"",client.message.c_str()];
    if(c.Shots().empty())self.shot.text=@"No shot yet. Aim across the pad; power grows as you drag up.";
    else{const auto& s=c.Shots().back();self.shot.text=[NSString stringWithFormat:@"SHOT %lu   AIM %+.0f%%   POWER %.0f%%\n%@",(unsigned long)c.Shots().size(),s.aim*100,s.power*100,s.source==icShotMode::Touch?@"Touch intent":@"Motion intent"];}
    self.hint.text=gesture.Config().mode==icShotMode::Touch?@"DRAG UP TO ROLL\n\nRelease to launch\nHorizontal position sets aim":gesture.Config().calibrated?@"HOLD · SWING · RELEASE\n\nHold a finger here, swing forward,\nthen release to launch":@"CALIBRATE FIRST\n\nHold the device still, then tap Calibrate.\nMotion needs real device sensors.";
    float aim=gesture.Aim(),strength=gesture.Power();if(!gesture.Armed()&&!c.Shots().empty()){aim=c.Shots().back().aim;strength=c.Shots().back().power;}
    CGFloat w=self.pad.bounds.size.width,h=self.pad.bounds.size.height;CGPoint start=CGPointMake(w*.5,h*.9),end=CGPointMake(w*(.5+aim*.4),h*(.9-strength*.75));
    UIBezierPath* path=[UIBezierPath bezierPath];[path moveToPoint:start];[path addQuadCurveToPoint:end controlPoint:CGPointMake(start.x,(start.y+end.y)*.5)];[CATransaction begin];[CATransaction setDisableActions:YES];self.trajectory.path=path.CGPath;
    self.marker.path=[UIBezierPath bezierPathWithOvalInRect:CGRectMake(end.x-7,end.y-7,14,14)].CGPath;[CATransaction commit];
    if(gesture.Armed())self.hint.text=[NSString stringWithFormat:@"POWER %.0f%%\nAIM %+.0f%%\n\nRelease to launch",gesture.Power()*100,gesture.Aim()*100];
}
- (void)changeMode {client.FinishRecording();client.companion.SetMode(self.mode.selectedSegmentIndex?icShotMode::Motion:icShotMode::Touch);}
- (void)toggleRecording {if(client.companion.Recording())client.FinishRecording();else{client.companion.StartRecording();client.message="Recording raw input. Shots remain local.";}}
- (void)replayTake {client.FinishRecording();if(client.companion.StartReplay())client.message="Live input paused during replay.";}
- (void)calibrateMotion {
    client.FinishRecording();auto result=client.companion.Calibrate();NSLog(@"ICE companion calibration=%d",(int)result);
    client.message=result==icCalibrationResult::Ready?"Calibrated. Hold, swing, release.":result==icCalibrationResult::NeedSamples?"Wait for sensor samples, then try again.":"Keep the device still and try calibration again.";
}
- (void)shareTake {
    client.SaveTake();NSURL* url=[NSURL fileURLWithPath:[NSString stringWithUTF8String:client.takePath.c_str()]];
    if(![NSFileManager.defaultManager fileExistsAtPath:url.path])return;
    UIActivityViewController* sheet=[[UIActivityViewController alloc] initWithActivityItems:@[url] applicationActivities:nil];
    sheet.popoverPresentationController.sourceView=self.share;sheet.popoverPresentationController.sourceRect=self.share.bounds;
    [self.window.rootViewController presentViewController:sheet animated:YES completion:nil];
}
- (void)addScene {
    [UIApplication.sharedApplication requestSceneSessionActivation:nil userActivity:nil options:nil errorHandler:^(NSError* error){NSLog(@"ICE scene request: %@",error);}];
}
- (void)sendTouches:(NSSet<UITouch*>*)touches phase:(icInputPhase)phase {
    for(UITouch* touch in touches){CGPoint p=[touch locationInView:self];icDeviceInput event;
        event.device=touchDevice;event.surface=self.surface;event.contact=(uintptr_t)(__bridge void*)touch;event.seconds=touch.timestamp;
        event.phase=phase;event.x=p.x/MAX(1,self.bounds.size.width);event.y=p.y/MAX(1,self.bounds.size.height);host.PostInput(event);
    }
}
- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event{[self sendTouches:touches phase:icInputPhase::Begin];}
- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event{[self sendTouches:touches phase:icInputPhase::Change];}
- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event{[self sendTouches:touches phase:icInputPhase::End];}
- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event{[self sendTouches:touches phase:icInputPhase::Cancel];}
@end
@interface ICESceneDelegate : UIResponder<UIWindowSceneDelegate>
@property(nonatomic,strong) UIWindow* window;
@end
@implementation ICESceneDelegate
- (void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session options:(UISceneConnectionOptions*)options {
    if(![scene isKindOfClass:UIWindowScene.class])return;
    self.window=[[UIWindow alloc] initWithWindowScene:(UIWindowScene*)scene];
    UIViewController* controller=[UIViewController new];ICEView* view=[[ICEView alloc] initWithFrame:self.window.bounds];view.multipleTouchEnabled=YES;
    CAMetalLayer* layer=(CAMetalLayer*)view.layer;layer.device=device;layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
    icSurface surface;surface.api=icSurfaceAPI::Metal;surface.nativeHandle=(__bridge void*)layer;view.surface=host.AddSurface(surface);
    [companionViews setObject:view forKey:@(view.surface)];
    controller.view=view;self.window.rootViewController=controller;scenes[(__bridge void*)scene]=icAppState::Inactive;[self.window makeKeyAndVisible];
}
- (void)sceneDidBecomeActive:(UIScene*)scene{scenes[(__bridge void*)scene]=icAppState::Active;aggregateState();}
- (void)sceneWillResignActive:(UIScene*)scene{scenes[(__bridge void*)scene]=icAppState::Inactive;aggregateState();}
- (void)sceneDidEnterBackground:(UIScene*)scene{ICEView* view=(ICEView*)self.window.rootViewController.view;host.SetSurfaceVisible(view.surface,false);scenes[(__bridge void*)scene]=icAppState::Background;aggregateState();}
- (void)sceneWillEnterForeground:(UIScene*)scene{ICEView* view=(ICEView*)self.window.rootViewController.view;host.SetSurfaceVisible(view.surface,true);scenes[(__bridge void*)scene]=icAppState::Inactive;aggregateState();}
- (void)sceneDidDisconnect:(UIScene*)scene{
    ICEView* view=(ICEView*)self.window.rootViewController.view;host.RemoveSurface(view.surface);[companionViews removeObjectForKey:@(view.surface)];view.surface=0;scenes.erase((__bridge void*)scene);aggregateState();self.window=nil;
}
@end
@interface ICEAppDelegate : UIResponder<UIApplicationDelegate>
@property(nonatomic,strong) CADisplayLink* displayLink;
@end
@implementation ICEAppDelegate
- (BOOL)application:(UIApplication*)app didFinishLaunchingWithOptions:(NSDictionary*)options {
    device=MTLCreateSystemDefaultDevice();commands=[device newCommandQueue];if(!device||!commands)return NO;
    companionViews=[NSMapTable strongToWeakObjectsMapTable];
    NSString* documents=NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask,YES).firstObject;
    client.takePath=[[documents stringByAppendingPathComponent:@"last-take.iceinput"] UTF8String];
    if(client.companion.Take().Load(client.takePath))client.message="Previous take loaded. Tap Replay to inspect it.";
    else{client.companion.Take()=icCompanionDemo();client.message="Demo take loaded. Replay or record.";}
    if([NSProcessInfo.processInfo.arguments containsObject:@"--demo-replay"])client.companion.StartReplay();
    touchDevice=host.AddDevice(icInputCapability(icInputKind::Pointer));motion=[CMMotionManager new];
    if(motion.accelerometerAvailable)accelDevice=host.AddDevice(icInputCapability(icInputKind::Acceleration));
    if(motion.gyroAvailable)gyroDevice=host.AddDevice(icInputCapability(icInputKind::AngularVelocity));
    self.displayLink=[CADisplayLink displayLinkWithTarget:self selector:@selector(tick:)];[self.displayLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];return YES;
}
- (void)tick:(CADisplayLink*)link{host.Frame(link.timestamp);}
- (void)applicationWillTerminate:(UIApplication*)application{[self.displayLink invalidate];host.Stop();}
@end
int main(int argc,char**argv){@autoreleasepool{return UIApplicationMain(argc,argv,nil,NSStringFromClass(ICEAppDelegate.class));}}
