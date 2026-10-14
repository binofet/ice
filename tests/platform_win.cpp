#include "Core/Application/Window/icWindowWIN.h"
#include "Core/GXDevice/icGXDeviceGL.h"
#include "Core/IO/icInput.h"
#include <cstdio>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"Win32 smoke failed line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static bool draw(icGXDeviceGL& device,float red,float green,float blue){
    device.FrameBegin();glClearColor(red,green,blue,1);glClear(GL_COLOR_BUFFER_BIT);
    unsigned char pixel[4]={};glReadBuffer(GL_BACK);glReadPixels(10,10,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    bool correct=abs(pixel[0]-(int)(red*255))<3&&abs(pixel[1]-(int)(green*255))<3&&abs(pixel[2]-(int)(blue*255))<3;
    device.FrameEnd();return correct&&glGetError()==GL_NO_ERROR;
}
int main(){
    icWindowDef def={};def.i16Width=320;def.i16Height=240;def.pWindowName="ICE native main";
    icWindowWIN main(def,nullptr);CHECK(main.GetHWND());CHECK(!ICEFAIL(main.Show()));
    icGXDeviceGL first;CHECK(!ICEFAIL(first.Init(&main)));
    def.i16Width=200;def.i16Height=160;def.pWindowName="ICE native preview";
    icWindowWIN preview(def,&main);CHECK(preview.GetHWND());CHECK(!ICEFAIL(preview.Show()));CHECK(main.GetID()!=preview.GetID()&&main.GetID()!=0);
    icGXDeviceGL second;CHECK(!ICEFAIL(second.Init(&preview)));icWindow::PumpEvents();
    CHECK(GetWindow(preview.GetHWND(),GW_OWNER)==main.GetHWND());
    CHECK(draw(first,1,0,0));CHECK(draw(second,0,1,0));CHECK(draw(first,0,0,1));
    def.i16Width=260;def.i16Height=180;CHECK(!ICEFAIL(preview.UpdateParams(def)));
    int width,height;preview.GetClientSize(&width,&height);CHECK(width==260&&height==180);
    def.bFullScreen=true;CHECK(!ICEFAIL(preview.UpdateParams(def)));icWindow::PumpEvents();
    preview.GetClientSize(&width,&height);CHECK(width>0&&height>0);
    def.bFullScreen=false;CHECK(!ICEFAIL(preview.UpdateParams(def)));icWindow::PumpEvents();
    preview.GetClientSize(&width,&height);CHECK(width==260&&height==180);
    SendMessage(preview.GetHWND(),WM_KEYDOWN,ICKEY_A,0);CHECK(icInput::Instance()->IsDown(ICKEY_A));icInput::Instance()->Update();
    SendMessage(preview.GetHWND(),WM_KEYUP,ICKEY_A,0);CHECK(icInput::Instance()->IsPressed(ICKEY_A));
    SendMessage(preview.GetHWND(),WM_CLOSE,0,0);CHECK(preview.ShouldClose());CHECK(!main.ShouldClose());
    second.Cleanup();preview.Cleanup();preview.Cleanup();CHECK(draw(first,0,1,1));
    first.Cleanup();main.Cleanup();main.Cleanup();
    std::puts("Win32 platform checks passed: independent windows/contexts, owner, resize, key events, close and cleanup");return 0;
}
