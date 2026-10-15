#include "Platform/icHost.h"
#include <android_native_app_glue.h>
#include <android/native_window.h>
#include <android/sensor.h>
#include <android/log.h>
#include <chrono>
#include <algorithm>
#include <cmath>
#define LOG(...) __android_log_print(ANDROID_LOG_INFO,"ICEHost",__VA_ARGS__)
struct AndroidProbe : icHostClient {
    icHost host;
    android_app* app;
    icSurfaceID surface=0;
    icDeviceID touch=0,accel=0,gyro=0;
    ASensorManager* manager=nullptr;
    ASensorEventQueue* sensors=nullptr;
    const ASensor* accelerometer=nullptr;
    const ASensor* gyroscope=nullptr;
    bool resumed=false,visible=false,focused=false;
    unsigned frames=0,inputs=0;
    float x=.5f,y=.5f;double elapsed=0;
    explicit AndroidProbe(android_app* a):host(*this),app(a){}
    void UpdateState(){host.SetState(resumed&&focused?icAppState::Active:visible?icAppState::Inactive:icAppState::Background);}
    void OnAppState(icAppState state) override {
        LOG("state=%d",(int)state);
        if(!sensors)return;
        for(auto sensor:{accelerometer,gyroscope})if(sensor){
            if(state==icAppState::Active){ASensorEventQueue_enableSensor(sensors,sensor);ASensorEventQueue_setEventRate(sensors,sensor,16667);}
            else ASensorEventQueue_disableSensor(sensors,sensor);
        }
    }
    void OnSurfaceAdded(const icSurface& s) override {LOG("surface added=%llu %dx%d",(unsigned long long)s.id,s.width,s.height);}
    void OnSurfaceChanged(const icSurface& s) override {LOG("surface resized=%llu %dx%d",(unsigned long long)s.id,s.width,s.height);}
    void OnSurfaceRemoved(const icSurface& s) override {LOG("surface removed=%llu",(unsigned long long)s.id);}
    void OnInput(const icDeviceInput& event) override {
        ++inputs;if(event.kind==icInputKind::Pointer){x=event.x;y=event.y;if(event.phase!=icInputPhase::Change)LOG("touch phase=%d contact=%llu surface=%llu",(int)event.phase,(unsigned long long)event.contact,(unsigned long long)event.surface);}
    }
    void OnUpdate(double dt) override {elapsed+=dt;}
    void OnRender(const icSurface& s) override {
        // Software diagnostic consumer: proves ownership/loss/recreation before the Vulkan renderer.
        auto window=static_cast<ANativeWindow*>(s.nativeHandle);ANativeWindow_Buffer buffer;
        if(ANativeWindow_lock(window,&buffer,nullptr)<0)return;
        int r=20+(int)(x*50),g=30+(int)(y*60),b=70+(int)(std::sin(elapsed)*15);
        std::uint32_t color=0xff000000u|(b<<16)|(g<<8)|r;
        for(int row=0;row<buffer.height;++row){auto pixels=static_cast<std::uint32_t*>(buffer.bits)+row*buffer.stride;
            for(int col=0;col<buffer.width;++col)pixels[col]=(std::abs(col-(int)(x*buffer.width))<20&&std::abs(row-(int)(y*buffer.height))<20)?0xffffffffu:color;
        }
        ANativeWindow_unlockAndPost(window);++frames;
        if(frames==1||frames%300==0)LOG("software frame=%u input=%u",frames,inputs);
    }
    void Attach(){
        if(!app->window)return;
        if(surface)host.RemoveSurface(surface);
        ANativeWindow_setBuffersGeometry(app->window,0,0,WINDOW_FORMAT_RGBA_8888);
        icSurface s;s.nativeHandle=app->window;s.width=ANativeWindow_getWidth(app->window);s.height=ANativeWindow_getHeight(app->window);
        surface=host.AddSurface(s);
    }
    void Motion(){
        ASensorEvent sample;
        while(sensors&&ASensorEventQueue_getEvents(sensors,&sample,1)>0){
            icDeviceInput event;event.seconds=sample.timestamp*1e-9;
            if(sample.type==ASENSOR_TYPE_ACCELEROMETER){event.device=accel;event.kind=icInputKind::Acceleration;}
            else if(sample.type==ASENSOR_TYPE_GYROSCOPE){event.device=gyro;event.kind=icInputKind::AngularVelocity;}
            else continue;
            event.x=sample.vector.x;event.y=sample.vector.y;event.z=sample.vector.z;host.PostInput(event);
        }
    }
};
static void command(android_app* app,int32_t cmd){
    auto& probe=*static_cast<AndroidProbe*>(app->userData);
    switch(cmd){
    case APP_CMD_START:probe.visible=true;probe.UpdateState();break;
    case APP_CMD_RESUME:probe.resumed=true;probe.UpdateState();break;
    case APP_CMD_PAUSE:probe.resumed=false;probe.UpdateState();break;
    case APP_CMD_STOP:probe.visible=false;probe.UpdateState();break;
    case APP_CMD_GAINED_FOCUS:probe.focused=true;probe.UpdateState();break;
    case APP_CMD_LOST_FOCUS:probe.focused=false;probe.UpdateState();break;
    case APP_CMD_INIT_WINDOW:probe.Attach();break;
    case APP_CMD_TERM_WINDOW:probe.host.RemoveSurface(probe.surface);probe.surface=0;break;
    default:break;
    }
}
static int32_t input(android_app* app,AInputEvent* native){
    if(AInputEvent_getType(native)!=AINPUT_EVENT_TYPE_MOTION)return 0;
    auto& probe=*static_cast<AndroidProbe*>(app->userData);
    if(!probe.surface||!app->window)return 0;
    int action=AMotionEvent_getAction(native);int type=action&AMOTION_EVENT_ACTION_MASK;
    size_t count=AMotionEvent_getPointerCount(native),selected=(action&AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)>>AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    for(size_t i=0;i<count;++i){
        if(type!=AMOTION_EVENT_ACTION_MOVE&&type!=AMOTION_EVENT_ACTION_CANCEL&&i!=selected)continue;
        icDeviceInput event;event.device=probe.touch;event.surface=probe.surface;event.contact=AMotionEvent_getPointerId(native,i);
        event.seconds=AMotionEvent_getEventTime(native)*1e-9;
        if(type==AMOTION_EVENT_ACTION_DOWN||type==AMOTION_EVENT_ACTION_POINTER_DOWN)event.phase=icInputPhase::Begin;
        else if(type==AMOTION_EVENT_ACTION_UP||type==AMOTION_EVENT_ACTION_POINTER_UP)event.phase=icInputPhase::End;
        else if(type==AMOTION_EVENT_ACTION_MOVE)event.phase=icInputPhase::Change;
        else if(type==AMOTION_EVENT_ACTION_CANCEL)event.phase=icInputPhase::Cancel;else continue;
        event.x=AMotionEvent_getX(native,i)/std::max(1,ANativeWindow_getWidth(app->window));
        event.y=AMotionEvent_getY(native,i)/std::max(1,ANativeWindow_getHeight(app->window));probe.host.PostInput(event);
    }return 1;
}
extern "C" void android_main(android_app* app){
    AndroidProbe probe(app);app->userData=&probe;app->onAppCmd=command;app->onInputEvent=input;
    probe.touch=probe.host.AddDevice(icInputCapability(icInputKind::Pointer));
    probe.manager=ASensorManager_getInstance();
    if(probe.manager){
        probe.accelerometer=ASensorManager_getDefaultSensor(probe.manager,ASENSOR_TYPE_ACCELEROMETER);
        probe.gyroscope=ASensorManager_getDefaultSensor(probe.manager,ASENSOR_TYPE_GYROSCOPE);
        probe.sensors=ASensorManager_createEventQueue(probe.manager,app->looper,LOOPER_ID_USER,nullptr,nullptr);
        if(probe.sensors){
            if(probe.accelerometer)probe.accel=probe.host.AddDevice(icInputCapability(icInputKind::Acceleration));
            if(probe.gyroscope)probe.gyro=probe.host.AddDevice(icInputCapability(icInputKind::AngularVelocity));
        }
    }
    while(!app->destroyRequested){
        int events;android_poll_source* source=nullptr;
        int ident=ALooper_pollOnce(probe.host.State()==icAppState::Active?16:-1,nullptr,&events,(void**)&source);
        if(ident>=0&&source)source->process(app,source);
        probe.Motion();
        if(probe.surface&&app->window)probe.host.ResizeSurface(probe.surface,ANativeWindow_getWidth(app->window),ANativeWindow_getHeight(app->window),1);
        double now=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();probe.host.Frame(now);
    }
    probe.host.Stop();
    if(probe.sensors)ASensorManager_destroyEventQueue(probe.manager,probe.sensors);
}
