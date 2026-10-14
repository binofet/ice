#include "Core/IO/icFile.h"
#include "Core/Application/icPlatformEvents.h"
#include "Core/IO/icInput.h"
#include "Util/icSettingsFile.h"
#include "Util/icTime.h"
#include "Util/icStateMachine.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"failed line %d: %s\n",__LINE__,#x); return 1; } } while(0)
struct TestState : icState {
    int pops=0;
    ICRESULT Update(float,bool*) override {return IC_OK;}
    ICRESULT Render() override {return IC_OK;}
    void OnPush() override {}
    void OnPop() override {++pops;}
};
int main() {
    icPlatformEvent event;
    while(icPlatformEvents::Poll(event)) {}
    auto firstID=icPlatformEvents::AllocateWindowID(),secondID=icPlatformEvents::AllocateWindowID();
    CHECK(firstID!=0&&secondID>firstID);
    icPlatformEvents::Post(icPlatformEventType::CloseRequested,secondID);
    icPlatformEvents::Post(icPlatformEventType::ApplicationQuitRequested);
    CHECK(icPlatformEvents::Poll(event)&&event.window==secondID);
    auto sequence=event.sequence;
    CHECK(icPlatformEvents::Poll(event)&&event.window==IC_APPLICATION_SCOPE&&event.sequence>sequence);
    CHECK(!icPlatformEvents::Poll(event));
    CHECK(ICEFAIL(IC_FAIL_GEN)); CHECK(!ICEFAIL(IC_OK));
    char path[]="/tmp/ice-core-XXXXXX";int fd=mkstemp(path);CHECK(fd>=0);close(fd);
    icFile file;size_t actual=0;char payload[]="preserve";
    CHECK(!ICEFAIL(file.Open(path,ICFMREAD_WRITE_NEW)));
    CHECK(!ICEFAIL(file.Write(payload,8,&actual))&&actual==8);
    CHECK(!ICEFAIL(file.SetPos(3)));uint64 size=0,pos=0;
    CHECK(!ICEFAIL(file.GetSize(&size))&&size==8);
    CHECK(!ICEFAIL(file.GetPos(&pos))&&pos==3);file.Close();
    CHECK(!ICEFAIL(file.Open(path,ICFMREAD_CREATE)));
    char data[16]={};CHECK(ICEFAIL(file.Read(data,16,&actual))&&actual==8);
    CHECK(std::strcmp(data,payload)==0);file.Close();
    FILE* f=std::fopen(path,"wb");CHECK(f);std::fputs("[Engine.Graphics]\r\nScreenX = 1280\r\n",f);std::fclose(f);
    icSettingsFile settings;CHECK(!ICEFAIL(settings.ReadFile(path)));
    CHECK(std::strcmp(settings.GetValue("Engine.Graphics.ScreenX").ToChar(),"1280")==0);
    CHECK(!ICEFAIL(settings.AddPair("Engine.Graphics.ScreenX","800")));
    CHECK(!ICEFAIL(settings.WriteFile(path)));
    icSettingsFile restored;CHECK(!ICEFAIL(restored.ReadFile(path)));
    CHECK(std::strcmp(restored.GetValue("Engine.Graphics.ScreenX").ToChar(),"800")==0);unlink(path);
    icTimer timer;std::this_thread::sleep_for(std::chrono::milliseconds(20));
    auto elapsed=timer.Elapsed();CHECK(elapsed>=0.01&&elapsed<1);
    auto input=icInput::Instance();input->SetDown(ICKEY_A,true);CHECK(input->IsDown(ICKEY_A));
    input->Update();input->SetDown(ICKEY_A,false);CHECK(input->IsPressed(ICKEY_A));
    input->Update();CHECK(!input->IsPressed(ICKEY_A));
    input->SetDown(ICMOUSE_L,true);input->Update();input->CancelHeldInput();
    CHECK(!input->IsDown(ICMOUSE_L)&&!input->IsPressed(ICMOUSE_L));
    input->HideCurs();input->UpdateMousePosition(100000,100000,720);input->Update();
    input->UpdateMousePosition(100072,99928,720);
    CHECK(std::fabs(input->GetAxis(ICAXIS_MOUSE_X)-.1f)<.0001f);
    CHECK(std::fabs(input->GetAxis(ICAXIS_MOUSE_Y)+.1f)<.0001f);
    input->ShowCurs();input->UpdateMousePosition(100,100,720);
    CHECK(input->GetAxis(ICAXIS_MOUSE_X)==0);
    TestState first,second;icStateMachine machine;
    machine.PushState(&first);machine.PushState(&second);
    CHECK(machine.PopState()==&second&&second.pops==1&&machine.PeekState()==&first);
    CHECK(machine.PopState()==&first&&first.pops==1&&machine.PeekState()==nullptr);
    std::puts("Core runtime checks passed");return 0;
}
