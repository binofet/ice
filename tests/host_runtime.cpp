#include "Platform/icHost.h"
#include <cmath>
#include <cstdio>
#include <thread>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"host check failed line %d: %s\n",__LINE__,#x);return 1;}}while(0)
struct Client : icHostClient {
    int updates=0,renders=0,added=0,removed=0,cancels=0,inputs=0; double dt=-1;
    void OnSurfaceAdded(const icSurface&) override {++added;}
    void OnSurfaceRemoved(const icSurface&) override {++removed;}
    void OnUpdate(double d) override {++updates;dt=d;}
    void OnRender(const icSurface&) override {++renders;}
    void OnInput(const icDeviceInput& e) override {++inputs;if(e.phase==icInputPhase::Cancel)++cancels;}
};
int main(){
    Client client;icHost host(client);
    auto pointer=host.AddDevice(icInputCapability(icInputKind::Pointer));
    auto sensor=host.AddDevice(icInputCapability(icInputKind::Acceleration));
    icDeviceInput touch;touch.device=pointer;touch.phase=icInputPhase::Begin;
    CHECK(!host.PostInput(touch));host.SetState(icAppState::Active);
    host.Frame(10);CHECK(client.updates==1&&client.renders==0&&client.dt==0); // no root window
    icSurface surface;surface.width=320;surface.height=240;
    auto a=host.AddSurface(surface),b=host.AddSurface(surface);CHECK(a&&b!=a);
    touch.surface=a;CHECK(host.PostInput(touch));host.Frame(10.01);CHECK(client.inputs==1&&client.renders==2);
    CHECK(host.RemoveSurface(a));CHECK(client.cancels==1&&client.removed==1);
    CHECK(host.PostInput(touch));host.Frame(10.02);CHECK(client.inputs==2); // only cancellation; stale surface rejected
    auto c=host.AddSurface(surface);CHECK(c!=a&&c!=b);
    touch.surface=b;touch.contact=42;CHECK(host.PostInput(touch));host.Frame(10.03);
    host.SetState(icAppState::Background);CHECK(client.cancels==2);
    int updates=client.updates;host.Frame(900);CHECK(client.updates==updates&&!host.PostInput(touch));
    host.SetState(icAppState::Active);host.Frame(1000);CHECK(client.dt==0);host.Frame(1001);CHECK(client.dt==.1);
    host.Frame(999);CHECK(client.dt==0);host.Frame(1001.01);CHECK(client.dt>.009&&client.dt<.011);
    CHECK(host.ResizeSurface(b,0,0,2));int renders=client.renders;host.Frame(1001.02);CHECK(client.renders==renders+1);
    CHECK(host.PostInput(touch));host.Frame(1001.03);CHECK(host.RemoveDevice(pointer));CHECK(client.cancels==3);
    icDeviceInput motion;motion.device=sensor;motion.kind=icInputKind::Acceleration;motion.x=9.81f;
    int inputs=client.inputs;std::thread producer([&]{host.PostInput(motion);});producer.join();host.Frame(1001.04);CHECK(client.inputs==inputs+1);
    // Removing a pointer device must not remove an independent sensor.
    pointer=host.AddDevice(icInputCapability(icInputKind::Pointer));touch.device=pointer;touch.surface=c;
    CHECK(host.PostInput(touch));host.Frame(1001.05);
    for(int i=0;i<1025;++i)host.PostInput(motion);
    host.Frame(1001.06);CHECK(host.DroppedInput()==1&&client.cancels==4);
    touch.phase=icInputPhase::End;inputs=client.inputs;host.PostInput(touch);host.Frame(1001.07);CHECK(client.inputs==inputs);
    CHECK(host.SetSurfaceVisible(c,false));renders=client.renders;host.Frame(1001.08);CHECK(client.renders==renders);
    CHECK(host.SetSurfaceVisible(c,true));host.Frame(1001.09);CHECK(client.renders==renders+1);
    host.Stop();CHECK(client.removed==3&&host.State()==icAppState::Stopped);
    CHECK(!host.SetState(icAppState::Active)&&!host.AddSurface(surface)&&!host.AddDevice(1));host.Stop();
    Client routed;icHost routing(routed);routing.SetState(icAppState::Active);
    auto left=routing.AddSurface(surface),right=routing.AddSurface(surface);
    auto fingers=routing.AddDevice(icInputCapability(icInputKind::Pointer));
    icDeviceInput first;first.device=fingers;first.surface=left;first.phase=icInputPhase::Begin;
    icDeviceInput second=first;second.surface=right;
    routing.PostInput(first);routing.PostInput(second);routing.Frame(1);CHECK(routed.inputs==2);
    routing.RemoveSurface(left);CHECK(routed.cancels==1);
    second.phase=icInputPhase::End;routing.PostInput(second);routing.Frame(2);CHECK(routed.inputs==4&&routed.cancels==1);
    first.surface=right;routing.PostInput(first);routing.SetSurfaceVisible(right,false);routing.Frame(3);CHECK(routed.inputs==4);
    icDeviceInput unsupported;unsupported.device=fingers;unsupported.kind=icInputKind::Acceleration;
    routing.PostInput(unsupported);routing.Frame(4);CHECK(routed.inputs==4);
    unsupported.seconds=NAN;CHECK(!routing.PostInput(unsupported));routing.Stop();
    std::puts("Host checks passed: windowless updates, multiple/recreated surfaces, resume clock, touch cancellation, sensors, overflow and shutdown");
}
