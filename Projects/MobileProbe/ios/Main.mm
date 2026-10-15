#import <UIKit/UIKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <CoreMotion/CoreMotion.h>
#include "Platform/icHost.h"
#include <map>
#include <cmath>

static id<MTLDevice> device;
static id<MTLCommandQueue> commands;
static CMMotionManager* motion;
static icDeviceID touchDevice=0,accelDevice=0,gyroDevice=0;
static void updateSensors(bool active);
struct ProbeClient : icHostClient {
    unsigned frames=0,inputs=0,surfaces=0;float touchX=.2f,touchY=.4f;double elapsed=0;
    void OnAppState(icAppState state) override {
        NSLog(@"ICE state=%d",(int)state);updateSensors(state==icAppState::Active);
    }
    void OnSurfaceAdded(const icSurface& s) override {++surfaces;NSLog(@"ICE surface added=%llu %dx%d",s.id,s.width,s.height);}
    void OnSurfaceChanged(const icSurface& s) override {NSLog(@"ICE surface resized=%llu %dx%d",s.id,s.width,s.height);}
    void OnSurfaceRemoved(const icSurface& s) override {--surfaces;NSLog(@"ICE surface removed=%llu",s.id);}
    void OnInput(const icDeviceInput& e) override {
        ++inputs;if(e.kind==icInputKind::Pointer){touchX=e.x;touchY=e.y;NSLog(@"ICE touch phase=%d contact=%llu surface=%llu",(int)e.phase,e.contact,e.surface);}
    }
    void OnUpdate(double dt) override {elapsed+=dt;}
    void OnRender(const icSurface& s) override {
        CAMetalLayer* layer=(__bridge CAMetalLayer*)s.nativeHandle;
        id<CAMetalDrawable> drawable=[layer nextDrawable];if(!drawable)return;
        MTLRenderPassDescriptor* pass=[MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture=drawable.texture;
        pass.colorAttachments[0].loadAction=MTLLoadActionClear;
        pass.colorAttachments[0].storeAction=MTLStoreActionStore;
        pass.colorAttachments[0].clearColor=MTLClearColorMake(.08+.2*touchX,.12+.25*touchY,.25+.07*std::sin(elapsed),1);
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
@interface ICEView : UIView
@property(nonatomic) icSurfaceID surface;
@end
@implementation ICEView
+ (Class)layerClass{return CAMetalLayer.class;}
- (void)layoutSubviews {
    [super layoutSubviews];CAMetalLayer* layer=(CAMetalLayer*)self.layer;
    CGFloat scale=self.window.screen.scale?:UIScreen.mainScreen.scale;
    layer.contentsScale=scale;layer.drawableSize=CGSizeMake(self.bounds.size.width*scale,self.bounds.size.height*scale);
    if(self.surface)host.ResizeSurface(self.surface,(int)layer.drawableSize.width,(int)layer.drawableSize.height,scale);
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
    UILabel* label=[[UILabel alloc] initWithFrame:CGRectMake(24,80,340,160)];label.numberOfLines=0;label.textColor=UIColor.whiteColor;label.font=[UIFont monospacedSystemFontOfSize:16 weight:UIFontWeightRegular];label.tag=100;label.userInteractionEnabled=NO;
    label.text=@"ICE native mobile host\nMetal / touch / CoreMotion\nTouch to change the clear color\nBackground/resume and rotate";[view addSubview:label];
    if(UIApplication.sharedApplication.supportsMultipleScenes){
        UIButton* button=[UIButton buttonWithType:UIButtonTypeSystem];button.frame=CGRectMake(24,250,250,44);
        [button setTitle:@"Open another ICE surface" forState:UIControlStateNormal];[button setTitleColor:UIColor.whiteColor forState:UIControlStateNormal];
        [button addTarget:view action:@selector(addScene) forControlEvents:UIControlEventTouchUpInside];[view addSubview:button];
    }
    controller.view=view;self.window.rootViewController=controller;scenes[(__bridge void*)scene]=icAppState::Inactive;[self.window makeKeyAndVisible];
}
- (void)sceneDidBecomeActive:(UIScene*)scene{scenes[(__bridge void*)scene]=icAppState::Active;aggregateState();}
- (void)sceneWillResignActive:(UIScene*)scene{scenes[(__bridge void*)scene]=icAppState::Inactive;aggregateState();}
- (void)sceneDidEnterBackground:(UIScene*)scene{ICEView* view=(ICEView*)self.window.rootViewController.view;host.SetSurfaceVisible(view.surface,false);scenes[(__bridge void*)scene]=icAppState::Background;aggregateState();}
- (void)sceneWillEnterForeground:(UIScene*)scene{ICEView* view=(ICEView*)self.window.rootViewController.view;host.SetSurfaceVisible(view.surface,true);scenes[(__bridge void*)scene]=icAppState::Inactive;aggregateState();}
- (void)sceneDidDisconnect:(UIScene*)scene{
    ICEView* view=(ICEView*)self.window.rootViewController.view;host.RemoveSurface(view.surface);view.surface=0;scenes.erase((__bridge void*)scene);aggregateState();self.window=nil;
}
@end
@interface ICEAppDelegate : UIResponder<UIApplicationDelegate>
@property(nonatomic,strong) CADisplayLink* displayLink;
@end
@implementation ICEAppDelegate
- (BOOL)application:(UIApplication*)app didFinishLaunchingWithOptions:(NSDictionary*)options {
    device=MTLCreateSystemDefaultDevice();commands=[device newCommandQueue];if(!device||!commands)return NO;
    touchDevice=host.AddDevice(icInputCapability(icInputKind::Pointer));motion=[CMMotionManager new];
    if(motion.accelerometerAvailable)accelDevice=host.AddDevice(icInputCapability(icInputKind::Acceleration));
    if(motion.gyroAvailable)gyroDevice=host.AddDevice(icInputCapability(icInputKind::AngularVelocity));
    self.displayLink=[CADisplayLink displayLinkWithTarget:self selector:@selector(tick:)];[self.displayLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];return YES;
}
- (void)tick:(CADisplayLink*)link{host.Frame(link.timestamp);}
- (void)applicationWillTerminate:(UIApplication*)application{[self.displayLink invalidate];host.Stop();}
@end
int main(int argc,char**argv){@autoreleasepool{return UIApplicationMain(argc,argv,nil,NSStringFromClass(ICEAppDelegate.class));}}
