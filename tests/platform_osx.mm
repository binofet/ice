#import <AppKit/AppKit.h>
#include "Core/Application/Window/icWindowOSX.h"
#include "Core/IO/icInput.h"
#include "Core/GXDevice/icGLext.h"
#include <cstdio>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"Platform smoke failed line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static bool draw(icWindowOSX& window,float red,float green,float blue){
    window.MakeCurrent();int width,height;window.GetFramebufferSize(&width,&height);
    glViewport(0,0,width,height);glClearColor(red,green,blue,1);glClear(GL_COLOR_BUFFER_BIT);
    unsigned char pixel[4]={};glReadBuffer(GL_BACK);glReadPixels(width/2,height/2,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    bool correct=abs(pixel[0]-(int)(red*255))<3&&abs(pixel[1]-(int)(green*255))<3&&abs(pixel[2]-(int)(blue*255))<3;
    window.SwapBuffers();return correct&&glGetError()==GL_NO_ERROR;
}
int main(){
    @autoreleasepool {
        icWindowDef def={};def.i16Width=320;def.i16Height=240;def.pWindowName="ICE native main";
        icWindowOSX main(def);CHECK(main.GetRef());CHECK(main.CreateGLContext());CHECK(!ICEFAIL(main.Show()));
        def.i16Width=200;def.i16Height=160;def.pWindowName="ICE native preview";
        icWindowOSX preview(def,&main);CHECK(preview.GetRef());CHECK(preview.CreateGLContext());CHECK(!ICEFAIL(preview.Show()));CHECK(main.GetID()!=preview.GetID()&&main.GetID()!=0);
        icWindow::PumpEvents();
        CHECK([(NSWindow*)preview.GetRef() parentWindow]==(NSWindow*)main.GetRef());
        CHECK(draw(main,1,0,0));CHECK(draw(preview,0,1,0));CHECK(draw(main,0,0,1));
        def.i16Width=260;def.i16Height=180;def.pWindowName="ICE resized preview";
        CHECK(!ICEFAIL(preview.UpdateParams(def)));icWindow::PumpEvents();
        int width,height;preview.GetClientSize(&width,&height);CHECK(width==260&&height==180);
        CHECK([[(NSWindow*)preview.GetRef() title] isEqualToString:@"ICE resized preview"]);
        CHECK(draw(preview,1,1,0));
    def.bFullScreen=true;CHECK(!ICEFAIL(preview.UpdateParams(def)));icWindow::PumpEvents();
    preview.GetClientSize(&width,&height);CHECK(width>0&&height>0);
    def.bFullScreen=false;CHECK(!ICEFAIL(preview.UpdateParams(def)));icWindow::PumpEvents();
    preview.GetClientSize(&width,&height);CHECK(width==260&&height==180);
        // Exercise the view's native event handlers using local test events.
        NSView* view=(NSView*)preview.GetView();
        NSEvent* key=[NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:[(NSWindow*)preview.GetRef() windowNumber] context:nil characters:@"a" charactersIgnoringModifiers:@"a" isARepeat:NO keyCode:0];
        [view keyDown:key];CHECK(icInput::Instance()->IsDown(ICKEY_A));
        icInput::Instance()->Update();
        key=[NSEvent keyEventWithType:NSEventTypeKeyUp location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:[(NSWindow*)preview.GetRef() windowNumber] context:nil characters:@"a" charactersIgnoringModifiers:@"a" isARepeat:NO keyCode:0];
        [view keyUp:key];CHECK(icInput::Instance()->IsPressed(ICKEY_A));
        icInput::Instance()->SetDown(ICKEY_LEFT,true);icWindowOSX::FocusChanged();CHECK(!icInput::Instance()->IsDown(ICKEY_LEFT));
        [(NSWindow*)preview.GetRef() performClose:nil];CHECK(preview.ShouldClose());CHECK(!main.ShouldClose());
        preview.Cleanup();preview.Cleanup();CHECK(draw(main,0,1,1));
        main.RequestClose();CHECK(main.ShouldClose());main.Cleanup();main.Cleanup();
        std::puts("Native platform checks passed: independent windows/contexts, parent, resize/title, key/focus events, close and cleanup");
    }return 0;
}
